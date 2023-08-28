/* FullSpectrum: a command-line and web interface to the GADRAS Full Spectrum
 Isotope ID algorithm.  Lee Harding and Will Johnson, SNL.
 
 Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 Government retains certain rights in this software.
 For questions contact William Johnson via email at wcjohns@sandia.gov, or
 alternative email of full-spectrum@sandia.gov.
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <mutex>
#include <thread>
#include <condition_variable>

#include <Wt/WLogger.h>

#include "SpecUtils/SpecFile.h"
#include "SpecUtils/DateTime.h"
#include "SpecUtils/StringAlgo.h"
#include "SpecUtils/Filesystem.h"
#include "SpecUtils/EnergyCalibration.h"

#include "FullSpectrumId/AnalysisFromFiles.h"

using namespace std;

namespace
{
const char * const ns_foreground_names[] = {
  "fore", "ipc", "ioi", "item", "primary", "interest", "concern", "source", "src", "unk"
};
const char * const ns_background_names[] = {
  "back", "bkg"
};
}//namespace


namespace AnalysisFromFiles
{

std::shared_ptr<SpecUtils::SpecFile> parse_file( const std::string &filepath,
                                                 const std::string &fname )
{
  string extension = fname;
  const size_t period_pos = extension.find_last_of( '.' );
  if( period_pos != string::npos )
    extension = extension.substr( period_pos+1 );
  SpecUtils::to_lower_ascii( extension );
  
  bool loaded = false;
  auto spec = make_shared<SpecUtils::SpecFile>();
  
  if( SpecUtils::file_size(filepath) > 512*1024 )
  {
    // N42, PCF, MPS, daily files (.txt), and list-mode (.Lis) files seem to be the only ones ever
    //  above about 200K, of which we will only accept N42 and PCF files, so we'll limit parsing
    //  to only these types
    bool triedN42 = false, triedPcf = false;
    if( extension == "n42" )
    {
      triedN42 = true;
      loaded = spec->load_N42_file( filepath );
    }
    
    if( !loaded && (extension == "pcf") )
    {
      triedPcf = true;
      loaded = spec->load_pcf_file( filepath );
    }
    
    if( !loaded && !triedN42 )
      loaded = spec->load_N42_file( filepath );
    
    if( !loaded && !triedPcf )
      loaded = spec->load_pcf_file( filepath );
  }else
  {
    loaded = spec->load_file( filepath, SpecUtils::ParserType::Auto, extension );
  }
  
  if( !loaded )
    spec.reset();
  
  /*
   if( spec && spec->contains_derived_data() && spec->contains_non_derived_data() )
   {
   // We know how the verifinder behaves, so we'll cut to the chase and just keep its derived data.
   switch( spec->detector_type() )
   {
   case SpecUtils::DetectorType::VerifinderNaI:
   case SpecUtils::DetectorType::VerifinderLaBr:
   spec->keep_derived_data_variant( SpecUtils::SpecFile::DerivedVariantToKeep::Derived );
   break;
   
   default:
   break;
   }//switch( spec->detector_type() )
   }//if( we have derived and non-derived data )
   */
  
  return spec;
}//parse_file


void filter_energy_cal_variants( std::shared_ptr<SpecUtils::SpecFile> spec )
{
  // Some portals or search systems will give like a 1.5 MeV and 6 MeV, or Linear vs Sqrt, lets
  //  just keep the useful ones
  
  if( !spec )
    throw runtime_error( "Not a spectrum file." );
  
  const set<string> cals = spec->energy_cal_variants();
  if( cals.size() < 2 )
    return;
  
  // This next logic taken from Cambio command line, which was probably informed by a few specific
  //  models
  
  //Calibrations I've seen:
  //"CmpEnCal", and "LinEnCal"
  //"2.5MeV" vs "9MeV"
  
  string prefered_variant;
  for( const auto str : cals )
  {
    if( SpecUtils::icontains( str, "Lin") )
    {
      prefered_variant = str;
      Wt::log("debug:app") << "Selecting energy cal variant '" << str
      << "' based on if containing 'Lin'";
      
      spec->keep_energy_cal_variant( prefered_variant );
      return;
    }
  }//for( const auto &str : cals )
  
  
  auto getMev = []( string str ) -> double {
    SpecUtils::to_lower_ascii(str);
    size_t pos = str.find("mev");
    if( pos == string::npos )
      return -999.9;
    str = str.substr(0,pos);
    SpecUtils::trim( str );
    for( size_t index = str.size(); index > 0; --index )
    {
      const char c = str[index-1];
      if( !isdigit(c) && c != '.' && c!=',' )
      {
        str = str.substr(index);
        break;
      }
    }
    
    double val;
    if( (stringstream(str) >> val) )
      return val;
    
    return -999.9;
  };//getMev lamda
  
  double maxenergy = -999.9;
  for( const auto str : cals )
  {
    const double energy = getMev(str);
    if( (energy > 0.0) && (energy > maxenergy) )
    {
      maxenergy = energy;
      prefered_variant = str;
    }
  }//for( const auto str : cals )
  
  if( !prefered_variant.empty() )
  {
    Wt::log("debug:app") << "Selecting energy cal variant '" << prefered_variant
    << "' based on its name having the highest energy listed";
    spec->keep_energy_cal_variant( prefered_variant );
    return;
  }
  
  
  // From here on out, this is new logic, and not from cambio
  
  //If we're here, we have to work a little harder and look at the data to decide - I dont think
  //  this should probably happen much...
  try
  {
    vector<string> variant_name;
    vector<shared_ptr<SpecUtils::SpecFile>> variants;
    vector<string> cal_variants_vec;
    float max_upper_energy = 0.0f;
    set<size_t> nchannels_set;
    size_t maxchannels = 0, maxchannels_index = 0, max_upper_energy_index = 0;
    
    for( const string &variant : cals )
    {
      auto newspec = make_shared<SpecUtils::SpecFile>();
      *newspec = *spec;
      newspec->keep_energy_cal_variant( variant );
      
      variant_name.push_back( variant );
      
      const size_t index = variants.size();
      variants.push_back( newspec );
      cal_variants_vec.push_back(variant );
      
      float upper_energy = 0.0f;
      size_t nchannels = 0, ngammameas = 0;
      for( const auto &m : newspec->measurements() )
      {
        if( m->num_gamma_channels() < 32 )
          continue;
        
        ++ngammameas;
        nchannels = (std::max)(nchannels, m->num_gamma_channels() );
        upper_energy = (std::max)( upper_energy, m->gamma_energy_max() );
      }
      
      if( !ngammameas )
        continue;
      
      nchannels_set.insert( nchannels );
      
      if( upper_energy > max_upper_energy )
      {
        max_upper_energy = upper_energy;
        max_upper_energy_index = index;
      }
      
      if( nchannels > maxchannels )
      {
        maxchannels = nchannels;
        maxchannels_index = index;
      }
    }//for( const string &variant : cal_variants )
    
    if( maxchannels_index >= variants.size() )
      throw runtime_error( "maxchannels_index=" + std::to_string(maxchannels_index)
                          + " while variants.size()=" + std::to_string(variants.size()) );
    
    if( max_upper_energy_index >= variants.size() )
      throw runtime_error( "max_upper_energy_index=" + std::to_string(max_upper_energy_index)
                          + " while variants.size()=" + std::to_string(variants.size()) );
    
    if( (nchannels_set.size() > 1) )
    {
      assert( maxchannels_index < variants.size() );
      assert( maxchannels_index < cal_variants_vec.size() );
      
      Wt::log("debug:app") << "Selecting energy cal variant '"
      << cal_variants_vec[maxchannels_index]
      << "' based on number of channels";
      
      spec->keep_energy_cal_variant( cal_variants_vec[maxchannels_index] );
      return;
    }
    
    
    assert( maxchannels_index < variants.size() );
    assert( maxchannels_index < cal_variants_vec.size() );
    
    Wt::log("debug:app") << "Selecting energy cal variant '"
    << cal_variants_vec[max_upper_energy_index]
    << "' based on max energy (or maybe what just came first)";
    
    spec->keep_energy_cal_variant( cal_variants_vec[max_upper_energy_index] );
    return;
  }catch( std::exception &e )
  {
    //Probably shouldnt ever happen.
    Wt::log("error:app") << "Caught exception deciding on energy cal variant: " << e.what();
    
    throw runtime_error( "Multiple energy calibration ranges or types were found"
                        " and there was an error selecting which one to use."
                        "  Please use a tool like InterSpec or Cambio to fix." );
  }//try / catch to choose an energy calibration variant )
  
  Wt::log("error:app") << "Got to the end of filterEnergyCalVariant(...) function, which shouldnt have happened";
  throw runtime_error( "Logic error in filterEnergyCalVariant" );
}//void filter_energy_cal_variants( std::shared_ptr<SpecUtils::SpecFile> spec );



shared_ptr<SpecUtils::SpecFile> create_input( const std::tuple<SpecClassType,string,string> &input1,
                                                  boost::optional<tuple<SpecClassType,string,string>> input2 )
{
  auto parse_specfile = []( boost::optional<tuple<SpecClassType,string,string>> input ) -> shared_ptr<SpecUtils::SpecFile> {
    if( !input )
      return nullptr;
    
    string filepath = get<1>(*input);
    string filename = get<2>(*input);
    auto f = AnalysisFromFiles::parse_file( filepath, filename );
    if( !f )
      throw std::runtime_error( "Failed to parse spectrum file." );
    
    return f;
  };//parse_specfile

  // TODO: Need to refactor all the {potentially_use_derived_data, is_search_data, is_portal_data}
  //       logic from AnalysisGui.cpp and re-use here to cleanup files from Verifinder and such
  
  if( input2 && (get<1>(*input2) == get<1>(input1)) )
    input2 = boost::none;

  auto file1 = parse_specfile( input1 );
  auto file2 = parse_specfile( input2 );
  
  assert( file1 );
  if( !file1 )
    throw runtime_error( "Invalid logic." );
  
  auto check_use_derived = []( shared_ptr<SpecUtils::SpecFile> &f ) {
    
    // TODO: need to match logic of AnalysisGui::checkInputState() state of when and how to use derived data.
    if( potentially_analyze_derived_data( f ) )
    {
      set<shared_ptr<const SpecUtils::Measurement>> derived_foregrounds, derived_backgrounds;
      get_derived_measurements( f, derived_foregrounds, derived_backgrounds );
      
      if( !derived_foregrounds.empty() )
      {
        vector<shared_ptr<const SpecUtils::Measurement>> meas_to_remove;
        for( const auto &m : f->measurements() )
        {
          if( !derived_foregrounds.count(m) && !derived_backgrounds.count(m) )
            meas_to_remove.push_back( m );
        }
        
        f->remove_measurements( meas_to_remove );
      }//if( !foreground.empty() )
    }//if( potentially_analyze_derived_data( f ) )
  };//check_use_derived
  
  
  auto filter_types_out = []( shared_ptr<SpecUtils::SpecFile> &f, vector<SpecUtils::SourceType> unwanted ) {
    vector<shared_ptr<const SpecUtils::Measurement> > meas_to_remove;
    for( const auto m : f->measurements() )
    {
      if( std::find(begin(unwanted),end(unwanted),m->source_type()) != end(unwanted) )
        meas_to_remove.push_back( m );
    }
    f->remove_measurements( meas_to_remove );
  };//filter_types_out
  
  if( file1 )
  {
    filter_energy_cal_variants( file1 );
    check_use_derived( file1 );
    filter_types_out( file1, {SpecUtils::SourceType::IntrinsicActivity, SpecUtils::SourceType::Calibration} );
  }
  
  if( file2 )
  {
    filter_energy_cal_variants( file2 );
    check_use_derived( file2 );
    filter_types_out( file2, {SpecUtils::SourceType::IntrinsicActivity, SpecUtils::SourceType::Calibration} );
  }
  
  
  /** Makes sure number of foreground and background channels are consistent, and if there are multiple detectors for each
   sample, will sum them together to leave a single SpecUtils::Measurement per sample.
   */
  auto clean_up_simple_final_file = []( shared_ptr<SpecUtils::SpecFile> &f ) {
    
    assert( f->sample_numbers().size() == 2 );
    size_t nchannel = 0;
    for( const auto &m : f->measurements() )
    {
      const size_t nchan = m->num_gamma_channels();
      if( nchannel && nchan && (nchannel != nchan) )
        throw runtime_error( "Inconsistent number of channels" );
      nchannel = nchan;
    }
    
    if( f->num_measurements() == 2 )
      return;
    
    vector<shared_ptr<SpecUtils::Measurement>> summed;
    const vector<string> &det_names = f->detector_names();
    for( const int sample : f->sample_numbers() )
    {
      shared_ptr<SpecUtils::Measurement> m;
      
      try
      {
        m = f->sum_measurements( {sample}, det_names, nullptr );
      }catch( std::exception & )
      {
        throw runtime_error( "Couldnt determine energy calibration to use for summing multiple detectors data together." );
      }
      
      // Lets make sure sample and SourceType are set for the summed measurement (older version
      //  of SpecUtils dont do this in SpecFile::sum_measurements).
      m->set_sample_number( sample );
      
      for( const auto &sm : f->sample_measurements(sample) )
      {
        const SpecUtils::SourceType st = sm->source_type();
        if( st == SpecUtils::SourceType::Foreground || st == SpecUtils::SourceType::Background )
        {
          m->set_source_type( st );
          break;
        }
      }//for( const auto &sm : f->sample_measurements(sample) )
      
      summed.push_back( m );
    }//for( const int sample : f->sample_numbers() )
    
    assert( summed.size() == 2 );
    if( summed.size() != 2 )
      throw runtime_error( "Logic error summing detectors measurements together." );
    
    // Remove all the old measurements, and then add in our summed ones.
    for( const auto &m : f->measurements() )
      f->remove_measurement( m, false );
    
    for( const auto &m : summed )
      f->add_measurement( m, false );
    f->cleanup_after_load();
  };//clean_up_simple_final_file
  
  if( !file2 )
  {
    if( file1->passthrough() )
      return file1;
   
    switch( get<0>(input1) )
    {
      case SpecClassType::Background:
        throw runtime_error( "Only one file was provided, and it was specified as background." );
        break;
        
      case SpecClassType::Unknown:
      case SpecClassType::Foreground:
      case SpecClassType::SuspectForeground:
      case SpecClassType::SuspectBackground:
      case SpecClassType::ForegroundAndBackground:
        break;
    }//switch( input1.first )
    
    map<int,double> sample_cps;
    set<int> foreground_samples, background_samples, unknown_samples;
    for( const int sample : file1->sample_numbers() )
    {
      size_t nchannel = 0;
      double sample_sum = 0.0, sample_livetime = 0.0;
      bool back = false, unwanted = false, fore = false, unknown = false;
      for( const auto &m : file1->sample_measurements(sample) )
      {
        if( m->num_gamma_channels() )
        {
          sample_sum += m->gamma_count_sum();
          sample_livetime += m->live_time();
        }
        
        nchannel = std::max( nchannel, m->num_gamma_channels() );
        
        switch( m->source_type() )
        {
          case SpecUtils::SourceType::IntrinsicActivity: unwanted = true; break;
          case SpecUtils::SourceType::Calibration:       unwanted = true; break;
          case SpecUtils::SourceType::Background:        back = true;     break;
          case SpecUtils::SourceType::Foreground:        fore = true;     break;
          case SpecUtils::SourceType::Unknown:           unknown = true;  break;
        }//switch( m->source_type() )
      }//for( const auto &m : file1->sample_measurements(sample) )
      
      
      sample_cps[sample] = sample_sum / ((sample_livetime <= 0.0) ? 1.0 : sample_livetime);
      
      if( nchannel == 0 )
        continue;
      
      if( (back + unwanted + fore + unknown) > 1 )
        throw runtime_error( "Could not definitively determine measurement type of all samples in spectrum file." );
      
      if( (back + unwanted + fore + unknown) == 0 )
        throw runtime_error( "Error interpreting sample type in spectrum file." );
      
      if( unwanted )
      {
       // Nothing to do here
      }else if( back )
      {
        background_samples.insert( sample );
      }else if( fore )
      {
        foreground_samples.insert( sample );
      }else if( unknown )
      {
        unknown_samples.insert( sample );
      }
    }//for( const int sample : file1->sample_numbers() )
    
    const size_t ntotal_samples = background_samples.size()
                                  + foreground_samples.size()
                                  + unknown_samples.size();
    
    if( ntotal_samples < 0 )
      throw runtime_error( "No foreground or background found" );
    
    if( ntotal_samples == 1 )
      throw runtime_error( "No background provided" );
    
    if( foreground_samples.size() > 1 )
      throw runtime_error( "More than one foreground sample in spectrum file." );
    
    if( background_samples.size() > 1 )
      throw runtime_error( "More than one background sample in spectrum file." );
    
    int fore_sample = -999999, back_sample = -999999;
    if( (foreground_samples.size() == 1) && (background_samples.size() == 1) )
    {
      fore_sample = *begin(foreground_samples);
      back_sample = *begin(background_samples);
    }else if( (foreground_samples.size() == 1) && (unknown_samples.size() == 1) )
    {
      fore_sample = *begin(foreground_samples);
      back_sample = *begin(unknown_samples);
    }else if( (unknown_samples.size() == 1) && (background_samples.size() == 1)
              && (foreground_samples.size() == 0) )
    {
      fore_sample = *begin(unknown_samples);
      back_sample = *begin(background_samples);
    }else if( (foreground_samples.size() == 0) && (background_samples.size() == 0)
             && (unknown_samples.size() == 2) )
    {
      // Hmmm, not to sure about this one; will arbitrarily assume that if one of the samples has
      //  a 25% higher count rate than the other, than that is the signal - but this 25% is really
      //  based on nothing.
      const int sample1 = *unknown_samples.begin();
      const int sample2 = *unknown_samples.rbegin();
      const double cps1 = sample_cps[sample1];
      const double cps2 = sample_cps[sample2];
      
      if( (cps1 > 0.75*cps2) && (cps2 > 0.75*cps1) )
        throw runtime_error( "Could not unambiguously determine foreground and background samples"
                             " in spectrum file; the two candidate spectrums are about same counts." );
      
      fore_sample = cps1 > cps2 ? sample1 : sample2;
      back_sample = cps1 > cps2 ? sample2 : sample1;
    }else
    {
      throw runtime_error( "Could not unambiguously determine foreground and background samples in spectrum file." );
    }
    
    if( !file1->sample_numbers().count(fore_sample)
       || !file1->sample_numbers().count(back_sample)
       || (fore_sample == back_sample) )
    {
      assert( 0 );
      throw runtime_error( "Error determining foreground/background sample numbers." );
    }
    
    vector<shared_ptr<const SpecUtils::Measurement> > meas_to_remove;
    for( const shared_ptr<const SpecUtils::Measurement> &m : file1->measurements() )
    {
      if( (m->sample_number() != fore_sample) && (m->sample_number() != back_sample) )
        meas_to_remove.push_back( m );
      else if( m->sample_number() == fore_sample )
        file1->set_source_type( SpecUtils::SourceType::Foreground, m );
      else if( m->sample_number() == back_sample )
        file1->set_source_type( SpecUtils::SourceType::Background, m );
    }//for( const shared_ptr<const SpecUtils::Measurement> &m : file1->measurements() )
    
    file1->remove_measurements( meas_to_remove );
    
    assert( file1->sample_numbers().size() == 2 );
    for( const shared_ptr<const SpecUtils::Measurement> &m : file1->measurements() )
    {
      assert( (m->source_type() == SpecUtils::SourceType::Foreground)
              || (m->source_type() == SpecUtils::SourceType::Background) );
    }
    
    // Now make sure all detectors get summed together, so there will only be two Measurements
    //  in the file we are returning.
    clean_up_simple_final_file( file1 );
    
    return file1;
  }//if( !file2 )
  
  assert( file1 && file2 );
  
  if( file1->passthrough() || file2->passthrough() )
    throw runtime_error( "One or both spectrum files are portal/search;"
                         " not supported when multiple files are specified." );
  
  SpecClassType type1 = get<0>(input1);
  SpecClassType type2 = get<0>(*input2);
  
  if( (type1 == SpecClassType::ForegroundAndBackground)
     || (type2 == SpecClassType::ForegroundAndBackground) )
    throw runtime_error( "A spectrum file was specified as foreground and background,"
                         " but more than one spectrum file specified." );
  
  
  auto source_types = []( shared_ptr<SpecUtils::SpecFile> &f ) -> set<SpecUtils::SourceType> {
    set<SpecUtils::SourceType> answer;
    for( const auto &m : f->measurements() )
      answer.insert( m->source_type() );
    return answer;
  };//source_types( lamda )
  
  
  if( file1->measurements().empty() || file2->measurements().empty() )
    throw runtime_error( "Spectrum file didnt contain expected measurement types." );
  
  // Lets cleanup the possible combinations of types input 1 and 2 could be.  We can get it down
  //  to just three possible cases to deal with.
  if( type1 == type2 )
  {
    type1 = type2 = SpecClassType::Unknown;
  }else if( (type1 == SpecClassType::Foreground) || (type1 == SpecClassType::Background) )
  {
    type2 = (type1 == SpecClassType::Foreground) ? SpecClassType::Background : SpecClassType::Foreground;
  }else if( (type2 == SpecClassType::Foreground) || (type2 == SpecClassType::Background) )
  {
    type1 = (type2 == SpecClassType::Foreground) ? SpecClassType::Background : SpecClassType::Foreground;
  }else if( (type1 == SpecClassType::Unknown)
      && ((type2 == SpecClassType::SuspectForeground) || (type2 == SpecClassType::SuspectBackground)) )
  {
    type1 = (type2 == SpecClassType::SuspectForeground) ? SpecClassType::SuspectBackground : SpecClassType::SuspectForeground;
  }else if( (type2 == SpecClassType::Unknown)
          && ((type1 == SpecClassType::SuspectForeground) || (type1 == SpecClassType::SuspectBackground)) )
  {
    type2 = (type1 == SpecClassType::SuspectForeground) ? SpecClassType::SuspectBackground : SpecClassType::SuspectForeground;
  }
  
  
  if( (type1 == SpecClassType::Background) || (type1 == SpecClassType::SuspectBackground) )
  {
    std::swap( file1, file2 );
    std::swap( type1, type2 );
  }
  
  assert( ((type1 == SpecClassType::Unknown) && (type2 == SpecClassType::Unknown) )
         || ((type1 == SpecClassType::Foreground) && (type2 == SpecClassType::Background) )
         || ((type1 == SpecClassType::SuspectForeground) && (type2 == SpecClassType::SuspectBackground) )
  );
  
  // At this point, SourceType is only: SourceType::Background, SourceType::Foreground, SourceType::Unknown
  
  // Lets filter out measurement types we dont want.  We will prefer foreground marked measurements,
  //  then non-marked measurements, then background marked measurements.
  auto filter_source_types = [filter_types_out,source_types]( shared_ptr<SpecUtils::SpecFile> &f ){
    set<SpecUtils::SourceType> src_types = source_types( f );
    assert( !src_types.count(SpecUtils::SourceType::IntrinsicActivity) );
    assert( !src_types.count(SpecUtils::SourceType::Calibration) );
    
    if( src_types.size() > 1 )
    {
      filter_types_out( f, {SpecUtils::SourceType::Background} );
      src_types = source_types( f );
      assert( !src_types.count(SpecUtils::SourceType::Background) );
    }
    
    if( src_types.size() > 1 )
    {
      filter_types_out( f, {SpecUtils::SourceType::Unknown} );
      src_types = source_types( f );
      assert( !src_types.count(SpecUtils::SourceType::Unknown) );
    }
    
    if( src_types.size() != 1 )
      throw runtime_error( "Error filtering measurement types in spectrum file." );
    
    assert( src_types.size() == 1 );
  };//filter_source_types(...)
  
  filter_source_types( file1 );
  filter_source_types( file2 );
  
  if( (file1->sample_numbers().size() != 1) || (file2->sample_numbers().size() != 1) )
    throw runtime_error( "Could not unambiguously select sample in spectrum file to use for measurement." );
  
  double cps1 = 0.0, cps2 = 0.0;
  if( (file1->gamma_live_time() > 0.0) && (file2->gamma_live_time() > 0.0) )
  {
    cps1 = file1->gamma_count_sum() / file1->gamma_live_time();
    cps2 = file2->gamma_count_sum() / file2->gamma_live_time();
  }else
  {
    cps1 = file1->gamma_count_sum();
    cps2 = file2->gamma_count_sum();
  }
  
  // At the end of this if/else selection, if we make it through (we'll throw exception if anything
  //  is ambiguous), then file1 will be the foreground with a single sample, and file2 will be
  //  background with a single sample.
  if( (type1 == SpecClassType::Unknown) && (type2 == SpecClassType::Unknown) )
  {
    // require one of the spectrum files to have 25% more counts than the other, or else call it
    //  ambiguous.  The 25% is arbitrarily chosen, without much insight.
    if( (cps1 > 0.75*cps2) && (cps2 > 0.75*cps1) )
      throw runtime_error( "Could not unambiguously determine foreground and background "
                          " spectrum files; the two spectrums are about same cps." );
    
    if( cps2 > cps1 )
    {
      std::swap( file1, file2 );
      std::swap( type1, type2 );
      std::swap( cps1, cps2 );
    }
  }else if( (type1 == SpecClassType::Foreground) && (type2 == SpecClassType::Background) )
  {
    // take the user at their word; nothing to do here
  }else if( (type1 == SpecClassType::SuspectForeground) && (type2 == SpecClassType::SuspectBackground) )
  {
    // require one of the spectrum files to have 10% more counts than the other, or else call it
    //  ambiguous.  The 10% and 25% are arbitrarily chosen, without any insight.
    
    if( (cps2 > 0.90*cps1) && (cps2 <= 1.25*cps1))
      throw runtime_error( "Could not unambiguously determine foreground from background;"
                          " the two spectrums are about same cps." );
    
    if( cps2 > 1.25*cps1 )
    {
      std::swap( file1, file2 );
      std::swap( type1, type2 );
      std::swap( cps1, cps2 );
    }
  }else
  {
    assert( 0 );
    throw runtime_error( "Logic error in figuring out which file is foreground and background" );
  }
  
  // Now check that each spectrum file has the same detector names (if more than one detector),
  //  and the same number of channels.
  if( file1->detector_names().size() != file2->detector_names().size() )
    throw runtime_error( "Mismatch between number of detectors in foreground and background file." );
  
  const size_t ndetectors = file1->detector_names().size();
  map<string,size_t> nchan_per_det;
  for( const auto &m : file1->measurements() )
  {
    const size_t nchan = m->num_gamma_channels();
    if( nchan )
      nchan_per_det[m->detector_name()] = nchan;
  }
  
  assert( !nchan_per_det.empty() );
  
  if( nchan_per_det.empty() )
    throw runtime_error( "Logic error retrieving detector names." );
  
  for( const auto &m : file2->measurements() )
  {
    const string &name = m->detector_name();
    const size_t nchan = m->num_gamma_channels();
    if( !nchan )
      continue;
    
    if( ndetectors == 1 )
    {
      if( begin(nchan_per_det)->second != m->num_gamma_channels() )
        throw runtime_error( "Mismatch between number of channels in foreground and background files" );
    }else
    {
      const auto iter = nchan_per_det.find(name);
      if( iter == end(nchan_per_det) )
        throw runtime_error( "Mismatch between detector names in foreground and background files" );
      
      if( iter->second != nchan )
        throw runtime_error( "Mismatch between number of channels for detector '" + name
                            + "' between foreground and background files." );
    }//if( ndetectors == 1 ) / ekse
  }//for( const auto &m : file2->measurements() )
  
  
  int fore_sample = 0;
  for( const auto &m : file1->measurements() )
  {
    fore_sample = m->sample_number();
    file1->set_source_type( SpecUtils::SourceType::Foreground, m );
  }
  
  const int back_sample = fore_sample + 1;
  for( const auto &m : file2->measurements() )
  {
    auto m2 = make_shared<SpecUtils::Measurement>( *m );
    m2->set_source_type( SpecUtils::SourceType::Background );
    m2->set_sample_number( back_sample );
    file1->add_measurement( m2, false );
  }
  
  file1->cleanup_after_load();
  
  // Now make sure all detectors get summed together, so there will only be two Measurements
  //  in the file we are returning.
  clean_up_simple_final_file( file1 );
  
  return file1;
}//create_input(...)




bool maybe_foreground_from_filename( const std::string &name )
{
  bool fore = false, back = false;
  
  for( const char *test : ns_foreground_names )
    fore = (fore || SpecUtils::icontains(name, test));
  
  for( const char *test : ns_background_names )
    back = (back || SpecUtils::icontains(name, test));
  
  return (fore && !back);
}


bool maybe_background_from_filename( const std::string &name )
{
  bool fore = false, back = false;
  
  for( const char *test : ns_foreground_names )
    fore = (fore || SpecUtils::icontains(name, test));
  
  for( const char *test : ns_background_names )
    back = (back || SpecUtils::icontains(name, test));
  
  return (!fore && back);
}


void get_derived_measurements( shared_ptr<const SpecUtils::SpecFile> spec,
                              set<shared_ptr<const SpecUtils::Measurement>> &foreground,
                              set<shared_ptr<const SpecUtils::Measurement>> &background )
{
  foreground.clear();
  background.clear();
  
  if( !spec )
    return;
  
  try
  {
    for( const auto &m : spec->measurements() )
    {
      if( !m || !m->derived_data_properties() || m->num_gamma_channels() < 32 )
        continue;
      
      const uint32_t properties = m->derived_data_properties();
      assert( properties & static_cast<uint32_t>(SpecUtils::Measurement::DerivedDataProperties::IsDerived) );
      const bool ioi_sum = (properties & static_cast<uint32_t>(SpecUtils::Measurement::DerivedDataProperties::ItemOfInterestSum));
      const bool for_ana = (properties & static_cast<uint32_t>(SpecUtils::Measurement::DerivedDataProperties::UsedForAnalysis));
      const bool processed = (properties & static_cast<uint32_t>(SpecUtils::Measurement::DerivedDataProperties::ProcessedFurther));
      const bool back_sub = (properties & static_cast<uint32_t>(SpecUtils::Measurement::DerivedDataProperties::BackgroundSubtracted));
      
      if( back_sub || processed )
        continue;
      
      switch( m->source_type() )
      {
        case SpecUtils::SourceType::Foreground:
          foreground.insert( m );
          break;
          
        case SpecUtils::SourceType::Background:
          background.insert( m );
          break;
          
        case SpecUtils::SourceType::Unknown:
          //This makes it so the order of seeing Foreground marked record an a IOI sum matters
          //  ... whatever for now
          if( ioi_sum && foreground.empty() )
            foreground.insert( m );
          break;
          
        case SpecUtils::SourceType::IntrinsicActivity:
        case SpecUtils::SourceType::Calibration:
          break;
      }//switch( m->source_type() )
    }//for( const auto &m : derived->measurements() )
    
    if( foreground.size() > 1 )
      throw runtime_error( "Multiple foreground" );
    
    if( background.size() > 1 )
      throw runtime_error( "Multiple background" );
    
    if( foreground.empty() )
      throw runtime_error( "No foreground in derived data" );
    
    if( background.empty() )
      throw runtime_error( "No background in derived data" );
    
    Wt::log("debug:app") << "Use derived data from foreground for analysis";
  }catch( std::exception &e )
  {
    foreground.clear();
    background.clear();
    Wt::log("debug:app") << "Couldnt use derived data: " << e.what();
  }//try / catch to get derived data spectra to use
}//get_derived_measurements



bool potentially_analyze_derived_data( std::shared_ptr<const SpecUtils::SpecFile> spec )
{
  // Right now we will only use derived data from Verifinder detectors, since they will show
  //  up as searchmode data, but their derived data is what we would sum anyway
  
  if( !spec )
    return false;
  
  bool potentially_use = false;
  switch( spec->detector_type() )
  {
    case SpecUtils::DetectorType::VerifinderNaI:
    case SpecUtils::DetectorType::VerifinderLaBr:
      //We'll use derived data for the Verifinder, if we have it
      potentially_use = spec->contains_derived_data();
      break;
      
    default:
      // For all other systems, we will only consider using derived data, if thats the only data
      //  we have.  The meaning of derived data is not well-specified in N42 files, so we should
      //  probably manually inspect contents of systems before using derived data from them.
      potentially_use = (spec->contains_derived_data() && !spec->contains_non_derived_data());
      break;
  }//switch( spec->detector_type() )
  
  return potentially_use;
}//potentially_analyze_derived_data(...)


bool is_portal_data( std::shared_ptr<const SpecUtils::SpecFile> inputspec )
{
  if( !inputspec )
    return false;
  
  set<int> foregroundSamples, backgroundSamples;
  for( const auto &m : inputspec->measurements() )
  {
    if( (m->num_gamma_channels() >= 32)
       && (m->real_time() >= 30.0f)
       && ((m->source_type() == SpecUtils::SourceType::Background )
           || (m->occupied() == SpecUtils::OccupancyStatus::NotOccupied)) )
    {
      backgroundSamples.insert( m->sample_number() );
    }
    
    if( (m->num_gamma_channels() >= 32)
       && (m->real_time() <= 2.0f)
       && ((m->source_type() == SpecUtils::SourceType::Foreground)
           || (m->source_type() == SpecUtils::SourceType::Unknown)
           || (m->occupied() == SpecUtils::OccupancyStatus::Occupied)) )
    {
      foregroundSamples.insert( m->sample_number() );
    }
    
    if( !backgroundSamples.empty() && (foregroundSamples.size() >= 3) )
      break;
  }//for( const auto &m : m_foreground->measurements() )
  
  return ( !backgroundSamples.empty() && (foregroundSamples.size() >= 3) );
}
}//namespace AnalysisFromFiles
