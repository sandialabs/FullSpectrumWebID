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
#include <deque>
#include <memory>
#include <thread>
#include <fstream>
#include <condition_variable>

#include <Wt/WString.h>
#include <Wt/WServer.h>
#include <Wt/WLogger.h>
#include <Wt/Json/Array.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Object.h>

#include "SpecUtils/SpecFile.h"
#include "SpecUtils/DateTime.h"
#include "SpecUtils/StringAlgo.h"
#include "SpecUtils/Filesystem.h"
#include "FullSpectrumId/Analysis.h"
#include "FullSpectrumId/EnergyCal.h"
#include "SpecUtils/EnergyCalibration.h"

#include "GadrasIsotopeID.h"

#if( !STATICALLY_LINK_TO_GADRAS && !defined(_WIN32) )
#include <dlfcn.h>
#endif

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <strsafe.h>
#endif


using namespace std;


namespace
{
#if( STATICALLY_LINK_TO_GADRAS )
//We are statically linking to the GADRAS library (currently only on Linux), so we have the
//  functions available.  We'll grab the pointers to these functions here so we can handle things
//  consistently elsewhere

// StreamingSearch we will put the type here to over-ride whats in GadrasIsotopeID.h - the
//  int32_t spectrumBuffer[NDETECTORS][NCHANNELS] parameter makes me a bit weezy incase
//   NDETECTORS or NCHANNELS is somehow used somewhere by the compiler (I dont think it should be
//   though...)
typedef int32_t (*fctn_StreamingSearch)(float *, float *, int32_t *, float *, char **, float *, int, int32_t *, int32_t, float *);

auto g_gadrasversionnumber = &gadrasversionnumber;
auto g_InitializeIsotopeIdCalibrated = &InitializeIsotopeIdCalibrated;
auto g_StaticIsotopeID = &StaticIsotopeID;
auto g_SearchIsotopeID = &SearchIsotopeID;
auto g_InitializeIsotopeIdRaw = &InitializeIsotopeIdRaw;
auto g_StreamingSearch = (fctn_StreamingSearch)&StreamingSearch;
auto g_GetCurrentIsotopeIDResults = &GetCurrentIsotopeIDResults;
auto g_ClearIsotopeIDResults = &ClearIsotopeIDResults;
auto g_RebinUsingK40 = &RebinUsingK40;
auto g_PortalIsotopeIDCInterface = &PortalIsotopeIDCInterface;

#else //STATICALLY_LINK_TO_GADRAS
#ifdef _WIN32
// On windows we will manually load the gadras shared library and resolve the needed functions,
//  so here are some handles and function pointers that get set in load_gadras_lib(...)
typedef int32_t (__cdecl *fctn_gadrasversionnumber_t)();
typedef int32_t (__cdecl *fctn_InitializeIsotopeIdCalibrated_t)(const char*, const char*, int32_t);
typedef int32_t (__cdecl *fctn_StaticIsotopeID_t)(float, float, float [], float, float, float [], float*, char**, float [], int, int, float*);
typedef int32_t (__cdecl *fctn_SearchIsotopeID_t)(float, float, float *, float *, char **, int, float *, int, float *);
typedef int32_t (__cdecl *fctn_InitializeIsotopeIdRaw_t)(const char *, const char *, int32_t nChannels, int32_t, const char *);
typedef int32_t (__cdecl *fctn_StreamingSearch_t)(float *, float *, int32_t *, float *, char **, float *, int, int32_t *, int32_t, float *);
typedef void (__cdecl *fctn_GetCurrentIsotopeIDResults_t)(struct IsotopeIDResult *isotopeInfoOut);
typedef void (__cdecl *fctn_ClearIsotopeIDResults_t)();
typedef int32_t (__cdecl *fctn_RebinUsingK40_t)(int32_t, float, float*, float*, float*, float*);
typedef int (__cdecl *fctn_PortalIsotopeIDCInterface_t)( char *, char *, struct PortalIsotopeIDOptions *,
                                                    int, struct PortalPlotOptions *,
                                                    struct PortalIsotopeIDOutput *, char *);


HINSTANCE g_gadras_dll_handle = NULL;
fctn_gadrasversionnumber_t g_gadrasversionnumber = nullptr;
fctn_InitializeIsotopeIdCalibrated_t g_InitializeIsotopeIdCalibrated = nullptr;
fctn_StaticIsotopeID_t g_StaticIsotopeID = nullptr;
fctn_SearchIsotopeID_t g_SearchIsotopeID = nullptr;
fctn_InitializeIsotopeIdRaw_t g_InitializeIsotopeIdRaw = nullptr;
fctn_StreamingSearch_t g_StreamingSearch = nullptr;
fctn_GetCurrentIsotopeIDResults_t g_GetCurrentIsotopeIDResults = nullptr;
fctn_ClearIsotopeIDResults_t g_ClearIsotopeIDResults = nullptr;
fctn_RebinUsingK40_t g_RebinUsingK40 = nullptr;
fctn_PortalIsotopeIDCInterface_t g_PortalIsotopeIDCInterface = nullptr;
#else //macOS or Linux

typedef int32_t (*fctn_gadrasversionnumber_t)();
typedef int32_t (*fctn_InitializeIsotopeIdCalibrated_t)(const char*, const char*, int32_t);
typedef int32_t (*fctn_StaticIsotopeID_t)(float, float, float [], float, float, float [], float*, char**, float [], int, int, float*);
typedef int32_t (*fctn_SearchIsotopeID_t)(float, float, float *, float *, char **, int, float *, int, float *);
typedef int32_t (*fctn_InitializeIsotopeIdRaw_t)(const char *, const char *, int32_t nChannels, int32_t, const char *);
typedef int32_t (*fctn_StreamingSearch_t)(float *, float *, int32_t *, float *, char **, float *, int, int32_t *, int32_t, float *);
typedef void (*fctn_GetCurrentIsotopeIDResults_t)(struct IsotopeIDResult *isotopeInfoOut);
typedef void (*fctn_ClearIsotopeIDResults_t)();
typedef int32_t (*fctn_RebinUsingK40_t)(int32_t, float, float*, float*, float*, float*);
typedef int (*fctn_PortalIsotopeIDCInterface_t)( char *, char *, struct PortalIsotopeIDOptions *,
                                                 int, struct PortalPlotOptions *,
                                                 struct PortalIsotopeIDOutput *, char *);


void *g_gadras_dll_handle = NULL;
fctn_gadrasversionnumber_t g_gadrasversionnumber = nullptr;
fctn_InitializeIsotopeIdCalibrated_t g_InitializeIsotopeIdCalibrated = nullptr;
fctn_StaticIsotopeID_t g_StaticIsotopeID = nullptr;
fctn_SearchIsotopeID_t g_SearchIsotopeID = nullptr;
fctn_InitializeIsotopeIdRaw_t g_InitializeIsotopeIdRaw = nullptr;
fctn_StreamingSearch_t g_StreamingSearch = nullptr;
fctn_GetCurrentIsotopeIDResults_t g_GetCurrentIsotopeIDResults = nullptr;
fctn_ClearIsotopeIDResults_t g_ClearIsotopeIDResults = nullptr;
fctn_RebinUsingK40_t g_RebinUsingK40 = nullptr;
fctn_PortalIsotopeIDCInterface_t g_PortalIsotopeIDCInterface = nullptr;
#endif // _WIN32 / else
#endif  // STATICALLY_LINK_TO_GADRAS / else


/** A helper function to extra make sure char arrays on the stack are null-terminated */
template<size_t N>
void null_terminate_static_str(char (&str)[N])
{
  str[N-1] = '\0';
}


std::mutex g_analysis_thread_mutex;
std::unique_ptr<std::thread> g_analysis_thread;

bool g_keep_analyzing = false;
std::mutex g_ana_queue_mutex;
std::condition_variable g_ana_queue_cv;
std::deque<Analysis::AnalysisInput> g_simple_ana_queue;


//g_gad_mutex protects gadars and g_gad_drf and g_gad_nchannel, although right now, this isnt
//  actally needed since the analysis happens in a dedicated thread anyway.
std::mutex g_gad_mutex;

// The GADRAS application directory.
string g_gad_app_folder = "gadras_isotope_id_run_directory";

/** How the "raw" search methods (i.e, StreamingSearch) should adjust the gain. */
enum class AutoGainAdjustType
{
  None, K40, Th232
};//enum class AutoGainAdjustType



/** We will track if GADRAS is currently initialized for the DRF we want, using the following five variables.
 
 When calling into GADRAS, we will first check if what we want matches whats in these variables, and if not, re-init GADRAS and set
 these variables to the new values.
 
 Both "raw" and "calibrated" ID methods use the first three variables, while only the "raw" methods use the last two.
 */
string g_gad_drf;
int32_t g_gad_nchannel = -1;
bool g_gad_calibrated = false;
int32_t g_num_detectors = -1;
AutoGainAdjustType g_gad_cal_adjust = AutoGainAdjustType::None;


std::string k40_fit_fail_reason( const int32_t rval )
{
  switch( rval )
  {
    case 0: return "Success";
    case 1: return "High count rate above 1100 keV.";
    case 2: return "High continuum to K40 peak count rate.";
    case 3: return "Low K40 peak-to-background ratio.";
    case 4: return "Nominal K40 peak off by over 200 keV.";
    case 5: return "Measurement live time is less than 60 seconds.";
  }//
  
  return "Unknown reason: code=" + std::to_string(rval);
};//k40_fit_fail_reason(...)

struct DoWorkOnDestruct
{
  std::function<void(void)> m_work;
  explicit DoWorkOnDestruct( std::function<void(void)> work ) : m_work( work ){}
  
  ~DoWorkOnDestruct()
  {
    if( m_work )
      m_work();
  }
};//struct DoWorkOnDestruct


// Takes in GADRAS isotope string, and returns a mapping from isotope to conf.
//  Confidences look to be "H", "F", "L", or empty.
map<string,string> get_iso_to_conf( const char *str )
{
  map<string,string> answer;
  
  if( !str )
    return answer;
  
  vector<string> isotopes;
  SpecUtils::split( isotopes, str, "+" );
  
  for( const string &isostr : isotopes )
  {
    if( SpecUtils::iequals_ascii( isostr, "NONE") )
      continue;
    
    const auto par_start_pos = isostr.find( '(' );
    const auto par_end_pos = isostr.find( ')', par_start_pos );
    
    if( (par_start_pos != string::npos) && (par_end_pos != string::npos) )
    {
      assert( par_end_pos > par_start_pos );
      string iso = isostr.substr( 0, par_start_pos );
      string conf = isostr.substr( par_start_pos + 1, par_end_pos - par_start_pos - 1 );
      SpecUtils::trim(iso);
      SpecUtils::trim(conf);
      
      answer[iso] = conf;
    }else
    {
      answer[isostr] = "";
    }//If( we have an isotope and confidence ) / else
  }//for( const string &isostr : isotopes )
  
  return answer;
}//get_iso_to_conf(...)


// Currently in Linux we are statically linking to the GADRAS library, so we can call into the
//  GADRAS functions directly.  But on Windows and macOS, we are loading a shared library, and using
//  the function pointers, so well use wrapper functions, to kinda try and make everything work.
int32_t gadras_version_number()
{
  assert( g_gadrasversionnumber );
  return g_gadrasversionnumber();
}


int32_t initialize_isotope_id_calibrated(const char* applicationFolder,
                                      const char* detectorName, int32_t numChannels )
{
  assert( g_InitializeIsotopeIdCalibrated );
  return g_InitializeIsotopeIdCalibrated( applicationFolder, detectorName, numChannels );
}


int32_t static_isotope_id(float tl, float tt, float foregroundSpectrum[],
                        float tlb, float ttb, float backgroundSpectrum[],
                        float* SOI, char** isotopeStr, float rebinnedEnergyGroups[],
                        int neutronsForeground, int neutronsBackground, float* rateNotNorm)
{
  assert( g_StaticIsotopeID );
  return g_StaticIsotopeID( tl, tt, foregroundSpectrum, tlb, ttb, backgroundSpectrum,
                            SOI, isotopeStr, rebinnedEnergyGroups,
                            neutronsForeground, neutronsBackground, rateNotNorm );
}//static_isotope_id



void get_current_isotope_id_results(struct IsotopeIDResult *isotopeInfoOut)
{
  assert( g_GetCurrentIsotopeIDResults );
  g_GetCurrentIsotopeIDResults( isotopeInfoOut );
}//get_current_isotope_id_results(...)


void clear_isotope_id_results()
{
  assert( g_ClearIsotopeIDResults );
  g_ClearIsotopeIDResults();
}//clear_isotope_id_results(...)


string stream_search_result_str( const int32_t status )
{
  switch( status )
  {
    case 0: return "Energy calibration was not performed (e.g., calTag="")";
    case 1: return "Calibration was successful";
    case -1: return "Spectrum not suitable for energy calibration";
    case -2: return "The specified background peak was not found";
    case -3: return "There was a large error (based on chi-square) in the fit to the photopeak";
    case -4: return "There were large uncertainties in the peak characteristics";
  }//switch( status )
  
  return "Other code " + std::to_string(status);
}//string stream_search_result_str( const int32_t status )


void check_init_results( const int32_t status )
{
  switch( status )
  {
    case 0: break;
    case -1: throw runtime_error( "DRF Init Error: error initializing application directory" );
    case -2: throw runtime_error( "DRF Init Error: general error initializing application" );
    case -3: throw runtime_error( "DRF Init Error: error initializing detector directory" );
    case -4: throw runtime_error( "DRF Init Error: Detector.dat read error" );
    case -5: throw runtime_error( "DRF Init Error: Response.win out of date" );
    case -6: throw runtime_error( "DRF Init Error: Response.win does not exist" );
    case -7: case -8: case -9:
      throw runtime_error( "DRF Init Error: General response read errors" );
    default:
      if( status < 0 )
        throw runtime_error( "DRF Init Error: Unknown error code " + std::to_string(status) );
      break;
  }//switch( init_code )
}//void check_init_results( const int32_t status )



/** Call this function before calling StreamingSearch (i.e., for multiple detectors, and channel counts as integers)
 */
int32_t init_gadras_drf_raw( const std::string &drf, const int32_t nchannel,
                             const int32_t num_detectors, const AutoGainAdjustType cal_type )
{
  if( (drf == g_gad_drf)
      && (nchannel == g_gad_nchannel)
      && (g_gad_calibrated == false)
      && (g_num_detectors == num_detectors)
      && (g_gad_cal_adjust == cal_type) )
  {
    return 0;
  }
  
  const char *calTag = "";
  switch( cal_type )
  {
    case AutoGainAdjustType::None:  calTag = "";  break; //Do not adjust gain
    case AutoGainAdjustType::K40:   calTag = "k"; break; //Use 1460 keV peak for calibration
    case AutoGainAdjustType::Th232: calTag = "t"; break; //Use 2614 keV peak for calibration
  }//switch( cal_type )
  
  assert( g_InitializeIsotopeIdRaw );
  const int32_t rval = g_InitializeIsotopeIdRaw( g_gad_app_folder.c_str(), drf.c_str(), nchannel, num_detectors, calTag);
  
  if( rval == 0 )
  {
    g_gad_drf = drf;
    g_gad_nchannel = nchannel;
    g_gad_calibrated = false;
    g_num_detectors = num_detectors;
    g_gad_cal_adjust = cal_type;
  }else
  {
    g_gad_drf = "";
    g_gad_nchannel = -1;
    g_gad_calibrated = false;
    g_num_detectors = -1;
    g_gad_cal_adjust = AutoGainAdjustType::None;
    
    Wt::log("error") << "Failed call to initialize_isotope_id_raw(\""
           << g_gad_app_folder << "\", \"" << drf << "\", " << nchannel << " );";
  }
  
  return rval;
}//void init_gadras_drf_raw( const std::string &drf, int32_t nchannel )


/** Call this function before doing a "calibrated" isotope ID (e.g., SearchIsotopeID or static_isotope_id)
 */
int32_t init_gadras_drf_calibrated( const std::string &drf, int32_t nchannel )
{
  if( (drf == g_gad_drf) && (nchannel == g_gad_nchannel) && (g_gad_calibrated == true) )
    return 0;
  
  const int32_t rval = initialize_isotope_id_calibrated( g_gad_app_folder.c_str(), drf.c_str(), nchannel );
  
  if( rval == 0 )
  {
    g_gad_drf = drf;
    g_gad_nchannel = nchannel;
    g_gad_calibrated = true;
  }else
  {
    g_gad_drf = "";
    g_gad_nchannel = -1;
    g_gad_calibrated = false;
    
    Wt::log("error") << "Failed call to initialize_isotope_id_calibrated(\""
           << g_gad_app_folder << "\", \"" << drf << "\", " << nchannel << " );";
  }
  
  return rval;
}//void init_gadras_drf_calibrated( const std::string &drf, int32_t nchannel )



void do_simple_analysis( Analysis::AnalysisInput input )
{
  std::lock_guard<std::mutex> ana_lock( g_gad_mutex );
 
  const double start_time = SpecUtils::get_wall_time();
  
  const string drf_folder = SpecUtils::append_path( "drfs" , input.drf_folder );
  
  // Right now input will have exactly two records, one foreground, and one background.
  //  We will create nee object for input_file to point to if we adjust energy calibration
  shared_ptr<SpecUtils::SpecFile> input_file = input.input;
  
  Analysis::AnalysisOutput result;
  result.ana_number = input.ana_number;
  result.drf_used = input.drf_folder;
  
  try
  {
    assert( input.analysis_type == Analysis::AnalysisType::Simple );
    
    if( !input_file )
      throw runtime_error( "Invalid input SpecUtils::SpecFile ptr" );
    
    // For right now we are limiting things to the simple case of two measurements.
    const size_t nmeas = input_file->num_measurements();
    if( (nmeas != 1) && (nmeas != 2) )
      throw runtime_error( "Invalid number of measurements" );
    
    vector<shared_ptr<const SpecUtils::Measurement>> backgrounds, foregrounds;
    const size_t nchannel = input_file->num_gamma_channels();
    for( const auto &m : input_file->measurements() )
    {
      if( m->num_gamma_channels() != nchannel )
        throw runtime_error( "Measurements somehow have different number of channels." );
      
      switch( m->source_type() )
      {
        case SpecUtils::SourceType::IntrinsicActivity:
        case SpecUtils::SourceType::Calibration:
          throw runtime_error( "Somehow an intrinsic or calibration spectrum made it to analysis" );
          break;
          
        case SpecUtils::SourceType::Foreground:
        case SpecUtils::SourceType::Unknown:
          foregrounds.push_back( m );
          break;
          
        case SpecUtils::SourceType::Background:
          backgrounds.push_back( m );
          break;
      }//switch( m->source_type() )
    }//for( const auto &m : input_file->measurements() )
    
    if( (foregrounds.size() != 1) )
      throw runtime_error( "Somehow we didnt send a single foreground to analysis" );
    
    if( backgrounds.size() > 1 )
      throw runtime_error( "Somehow sent more than one background to analysis" );
    
    if( nchannel < 32 || nchannel > 64*1024 )
      throw runtime_error( "Invalid number of channels (" + std::to_string(nchannel) + ")" );
    
    const double start_drf_init_time = SpecUtils::get_wall_time();
    
    const int32_t init_code = init_gadras_drf_calibrated( drf_folder, static_cast<int32_t>(nchannel) );
    result.gadras_intialization_error = init_code;
    
    const double finish_drf_init_time = SpecUtils::get_wall_time();
    
    // Check initi code and throw exception if error
    check_init_results( init_code );
    
    // We are inited here
    
    assert( foregrounds.size() == 1 );
    assert( backgrounds.size() <= 1 );
    
    shared_ptr<const SpecUtils::Measurement> foreground, background;
    foreground = foregrounds[0];
    assert( foreground );
    if( !foreground->gamma_counts() )  //shouldnt ever happen
      throw runtime_error( "Foreground is missing a spectrum???" );
    
    
    if( !backgrounds.empty() )
    {
      background = backgrounds[0];
      assert( background );
      if( !background->gamma_counts() )  //shouldnt ever happen
        throw runtime_error( "Background is missing a spectrum???" );
      
      if( foreground->gamma_counts()->size() != background->gamma_counts()->size() ) //shouldnt ever happen
        throw runtime_error( "Somehow foreground and background have different number of channels" );
    }//if( !backgrounds.empty() )
    
    shared_ptr<const SpecUtils::EnergyCalibration> forecal, backcal;
    forecal = foreground->energy_calibration();
    assert( forecal );
    
    if( !foreground->energy_calibration()->valid() )
      throw runtime_error( "Foreground energy calibration was invalid" );
    
    if( background )
    {
      backcal = background->energy_calibration();
      assert( backcal );
      
      if( !background->energy_calibration()->valid() )
        throw runtime_error( "Background energy calibration was invalid" );
    }//if( background )
    
    // Make sure foreground and background have the same energy calibration.  We'll only do a
    //  comparison of coefficients and such if the EnergyCalibration objects themselves differ
    //  (which happens usually if foreground/background are from different files )
    if( backcal && (forecal != backcal) )
    {
      if( (forecal->type() != backcal->type())
         || (forecal->coefficients() != backcal->coefficients())
         || (forecal->deviation_pairs() != backcal->deviation_pairs()) )
      {
        assert( foreground->num_gamma_channels() == background->num_gamma_channels() );
        
        input_file->rebin_measurement( forecal, background );
      }
    }//if( foreground->energy_calibration() != background->energy_calibration() )
    
    
    // Note: we will pass GADRAS copies of all the inputs since its arguments are not const, and
    //       it may actually make changes to the inputs (at least for ROSA it does, so be safe here)
    
    const float fore_livetime = foreground->live_time();
    const float fore_realtime = foreground->real_time();
    const float fore_neutrons = static_cast<float>( foreground->neutron_counts_sum() );
    vector<float> fore_spectrum = *foreground->gamma_counts();
    
    const float back_livetime = background ? background->live_time() : 0.0f;
    const float back_realtime = background ? background->real_time() : 0.0f;
    const float back_neutrons = background ? static_cast<float>(background->neutron_counts_sum()) : 0.0f;
    vector<float> back_spectrum( nchannel, 0.0f );
    if( background )
      back_spectrum = *background->gamma_counts(); // TODO: check this just turns into a memcpy
    
    assert( forecal->channel_energies() );
    if( !forecal->channel_energies() )
      throw runtime_error( "Somehow energy calibration doesnt have channel energies???" );
    
    vector<float> channel_energies = *forecal->channel_energies();
    assert( channel_energies.size() == (fore_spectrum.size() + 1) );
    
    if( channel_energies.size() != (fore_spectrum.size() + 1) )
      throw runtime_error( "Somehow channels energies didnt have the correct number of channels." );
    
    const double setup_finished_time = SpecUtils::get_wall_time();
    
    int32_t call_stat = 0;
    
    if( !background )
    {
      result.analysis_warnings.push_back( "The background is being synthesized; this yields"
                                          " non-optimal results, and also prevents the energy"
                                          " calibration check.  It is recommended to upload a"
                                          " representative background." );
    }else
    {
      //Begin code block to calibrate using K40
      assert( g_RebinUsingK40 );
      const bool highres = (channel_energies.size() > 5000);
      const double ncounts_region = background->gamma_integral( 1260, 1660 );
        
      // There is a test in the GADRAS code, that will change in the future, that scuttles our
      //  efforts often, so just skip the cases we know will faile
      const double ncounts_above_1MeV = background->gamma_integral( 1000, 3000 );
      
        // Threshold numbers are totally pulled out of error
        // TODO: better determine candidate numbers for recalibration
      const bool try_to_adjust = ((back_livetime > 60) && (ncounts_region > (highres ? 200 : 400))
                                  && ( (forecal->type() == SpecUtils::EnergyCalType::FullRangeFraction)
                                      || (forecal->type() == SpecUtils::EnergyCalType::Polynomial)
                                      || (forecal->type() == SpecUtils::EnergyCalType::UnspecifiedUsingDefaultPolynomial)
                                      )
                                  && ((ncounts_above_1MeV / back_livetime) < 6)
                                  );
        
      if( try_to_adjust )
      {
        const float true_k40_energy = 1460.75f;
        
        float centroid_K40 = true_k40_energy;
        vector<float> rebinned_spectrum( nchannel + 2, 0.0f );
        vector<float> spectrum = back_spectrum;
        vector<float> energies = channel_energies;
        call_stat = g_RebinUsingK40( nchannel, back_livetime, &(energies[0]),
                                       &(spectrum[0]), &(rebinned_spectrum[0]), &centroid_K40 );
        
        Wt::log("debug") << "Calibration using K40 on background yielded rval=" << call_stat
                             << " and centroid " << centroid_K40 << " keV";
        
        if( call_stat == 1 )
        {
          //We seem to get a call_stat == 1 a lot at the moment, so we'll display a gentle message
          result.analysis_warnings.push_back( "Energy calibration check was skipped.  You may want"
                                              " to manually make sure energy calibration is about"
                                              " correct (ex, the K40 peak is around 1460 keV).");
        }else if( call_stat != 0 )
        {
          result.analysis_warnings.push_back( "Checking energy calibration from K40 peak failed: "
                                             + k40_fit_fail_reason(call_stat)
                                             + "<br />You may want to manually make sure energy"
                                               " calibration is about correct (ex, the K40 peak is"
                                               " around 1460 keV).");
        }
        
        if( (call_stat == 0) && (fabs(centroid_K40 - true_k40_energy) > 0.5) )
        {
          try
          {
            EnergyCal::RecalPeakInfo peak;
            peak.peakMean = centroid_K40;
            peak.peakMeanUncert = 1.0;
            peak.peakMeanBinNumber = forecal->channel_for_energy(centroid_K40);;
            peak.photopeakEnergy = true_k40_energy;

            assert( forecal->coefficients().size() > 1 );
            vector<bool> fitfor( forecal->coefficients().size(), false );
            fitfor[1] = true;  //only fit for linear coefficient
            vector<float> coefs = forecal->coefficients();
            vector<float> coefs_uncert( forecal->coefficients().size(), 0.0f );
            const auto &devpairs = forecal->deviation_pairs();
            
            auto newcal = make_shared<SpecUtils::EnergyCalibration>();
            
            if( forecal->type() == SpecUtils::EnergyCalType::FullRangeFraction )
            {
              EnergyCal::fit_energy_cal_frf( {peak}, fitfor, nchannel, devpairs, coefs, coefs_uncert );
              newcal->set_full_range_fraction( nchannel, coefs, devpairs );
            }else
            {
              EnergyCal::fit_energy_cal_poly( {peak}, fitfor, nchannel, devpairs, coefs, coefs_uncert );
              newcal->set_polynomial( nchannel, coefs, devpairs );
            }
            
            // Set the energy calibrations in the SpecFile
            for( auto &m : input_file->measurements() )
            {
              if( m && (m->num_gamma_channels() >= 32) )
                input_file->set_energy_calibration( newcal, m );
            }
            
            // Create a new SpecFile object so the display GUI will know to re-display the histogram
            input_file = make_shared<SpecUtils::SpecFile>( *input_file );
            result.spec_file = input_file;
            
            // Update 'channel_energies' that will be passed to GADRAS
            assert( newcal->channel_energies() );
            assert( channel_energies.size() == newcal->channel_energies()->size() );
            channel_energies = *newcal->channel_energies();
        
            Wt::log("debug") << "Energy calibration was updated based on K40 peak, moving channel "
                                 << peak.peakMeanBinNumber << " from " << peak.peakMean << " to "
                                 << peak.photopeakEnergy << " keV "
                                 << "(" << newcal->energy_for_channel(peak.peakMeanBinNumber) << ")";
          }catch( std::exception &e )
          {
            // We really dont expect to get here, for any reasonable input
            //
            // I dont know what this excpetion might say, so dont display its content to the user
            result.analysis_warnings.push_back( "Performing energy recalibration hit an unexpected error, so was skipped." );
            
            Wt::log("error") << "Caught exception setting new energy calibration"
                                 << " for simple analysis: " << e.what();
          }//try / catch to fit for new gain
        }//if( we fit for K40 peak and will adjust energy cal )
      }else  //if( it is worth trying to adjust energy cal ) / else
      {
        result.analysis_warnings.push_back( "Skipped checking energy calibration - you may want to manually check the K40 peak is near 1460 keV." );
        
        Wt::log("debug") << "Will not try to adjust energy calibration using the 1460 keV peak";
      }
      
      //End code block to calibrate using K40
    }//if( !background ) / else (calibrate using k40)
    
    
    
    char *isotopeString = nullptr;
    float rateNotNorm = 0.0f, stuffOfInterest = 0.0f;
    
    Wt::log("debug") << "Will call into StaticIsotopeID for wt session '" << input.wt_app_id << "'";
    
    call_stat = static_isotope_id( fore_livetime, fore_realtime, &(fore_spectrum[0]),
                                 back_livetime, back_realtime, &(back_spectrum[0]),
                                 &stuffOfInterest, &isotopeString,
                                 &(channel_energies[0]),
                                 fore_neutrons, back_neutrons,
                                 &rateNotNorm );
    
    const double call_finished_time = SpecUtils::get_wall_time();
    result.gadras_analysis_error = call_stat;
    
    const string isostr = (isotopeString ? (const char *)isotopeString : "");

    Wt::log("debug") << "StaticIsotopeID returned code " << call_stat
                     << " and isotope string '" << isostr.c_str() << "'";
    
#ifndef _WIN32
    // Freeing isotopeString crashes on windows for some reason.
    if( isotopeString )
      free( isotopeString );
    isotopeString = nullptr;
    
    Wt::log("debug") << "Have freed isotopeString";
#endif
    
    if( call_stat < 0)
      throw runtime_error( "An analysis error occurred or template database was not found." );
    
    result.stuff_of_interest = stuffOfInterest;
    result.rate_not_norm = rateNotNorm;
    
    
    //isostr looks something like: "Cs137(H)", "Cs137(H)+Ba133(F)", "None", etc.
    result.isotopes = isostr;
    
    if( call_stat >= 0)
    {
      struct IsotopeIDResult idResult;
      get_current_isotope_id_results( &idResult );
      
      result.chi_sqr = idResult.chiSqr;
      result.alarm_basis_duration = idResult.alarmBasisDuration;
      
      if( idResult.nIsotopes > 0 )
      {
        map<string,string> iso_to_conf = get_iso_to_conf( isostr.c_str() );
        
        
        if( idResult.listOfIsotopeStrings )
          SpecUtils::split( result.isotope_names, idResult.listOfIsotopeStrings, "," );
        
        if( idResult.listOfIsotopeTypes )
          SpecUtils::split( result.isotope_types, idResult.listOfIsotopeTypes, "," );
        
        result.isotope_count_rates.resize( idResult.nIsotopes, -1.0f );
        result.isotope_confidences.resize( idResult.nIsotopes, -1.0f );
        result.isotope_confidence_strs.resize( idResult.nIsotopes, "" );
        
        
        for( int32_t i = 0; i < idResult.nIsotopes; ++i )
        {
          result.isotope_count_rates[i] = idResult.isotopeCountRates[i];
          result.isotope_confidences[i] = idResult.isotopeConfidences[i];
          
          // This next check should always be true, but JIC
          const string name = (i < result.isotope_names.size()) ? result.isotope_names[i] : "";
          if( iso_to_conf.count(name) )
            result.isotope_confidence_strs[i] = iso_to_conf[name];
        }
      }//if( idResult.nIsotopes )
      
      assert( result.isotope_names.size() == idResult.nIsotopes );
      assert( result.isotope_names.size() == result.isotope_types.size() );
      assert( result.isotope_names.size() == result.isotope_count_rates.size() );
      assert( result.isotope_names.size() == result.isotope_confidences.size() );
      assert( result.isotope_names.size() == result.isotope_confidence_strs.size() );
      
      
      const size_t nresult = result.isotope_names.size();
      if( (nresult != result.isotope_types.size())
         || (nresult != result.isotope_count_rates.size()) )
        throw runtime_error( "An analysis error occurred; there was an internal mis-match in"
                             " number of isotopes and their categories" );

#ifndef _WIN32
      if( idResult.listOfIsotopeStrings )
        free( idResult.listOfIsotopeStrings );
      
      if( idResult.listOfIsotopeTypes )
        free( idResult.listOfIsotopeTypes );
      
      idResult.listOfIsotopeStrings = idResult.listOfIsotopeTypes = nullptr;
#endif
      
      clear_isotope_id_results();
    }//if( call_stat >= 0)
    
    
    Wt::log("debug") << "Finished with analysis: '" << isostr.c_str() << "'";
    
    const double finished_time = SpecUtils::get_wall_time();
    
    const double total_time = finished_time - start_time;
    const double drf_init_time = finish_drf_init_time - start_drf_init_time;
    const double setup_time = setup_finished_time - start_time;
    const double gadras_time = call_finished_time - setup_finished_time;
    
    Wt::log("debug") << "Analysis took\n"
    << "\t\tSetup Time:    " << setup_time << "\n"
    << "\t\tDRF init Time: " << drf_init_time << "\n"
    << "\t\tAna Time:      " << gadras_time << "\n"
    << "\t\tTotal Time:    " << total_time << "\n"
    ;
    
    Wt::log("info") << "Analysis took\n"
         << "\t\tSetup Time: " << setup_time << "\n"
       //<< "\t\tDRF init Time: " << drf_init_time << "\n"
         << "\t\tAna Time:   " << gadras_time << "\n"
         << "\t\tTotal Time: " << total_time << "\n";
  }catch( std::exception &e )
  {
    result.error_message = e.what();
    Wt::log("error") << "Analysis failed due to: " << e.what();
  }//try / catch
  
  const string &wt_app_id = input.wt_app_id;
  function<void(Analysis::AnalysisOutput)> callback = input.callback;
  
  if( callback )
  {
    auto server = Wt::WServer::instance();
    if( server && !wt_app_id.empty() )
    {
      server->post( wt_app_id, [result,callback](){
        callback( result );
        wApp->triggerUpdate();
      
        Wt::log("debug") << "Update should have triggered to GUI";
      } );
    }else if( wt_app_id.empty() )
    {
      Wt::log("debug") << "wt_app_id is empty...";
      
      callback( result );
    }else //if( !wt_app_id.empty() )
    {
      Wt::log("error") << "Error: got non empty Wt session ID ('" << wt_app_id << "'), but there is no"
             << " WServer instance - not calling result callback!";
    }
  }//if( callback )
}//void do_simple_analysis( Analysis::AnalysisInput input )



void do_search_analysis( Analysis::AnalysisInput input )
{
  std::lock_guard<std::mutex> ana_lock( g_gad_mutex );
  
  const double start_time = SpecUtils::get_wall_time();
  
  const string drf_folder = SpecUtils::append_path( "drfs" , input.drf_folder );
  
  // Note, if we do any energy recalibration or rebinning or anything, we will change input_file
  //  to point to a new object with the new values
  shared_ptr<SpecUtils::SpecFile> input_file = input.input;
  
  // As of 20210923 we wont ever get portals in this function, but leaving the
  //  portal logic in incase we want to offer search mode analysis as an option
  //  for RPMs at a later point.
  const bool is_portal = (input.analysis_type == Analysis::AnalysisType::Portal);
  
  Analysis::AnalysisOutput result;
  result.ana_number = input.ana_number;
  result.drf_used = input.drf_folder;
  result.chi_sqr = -1.0f;
  result.alarm_basis_duration = -1.0f;
  result.spec_file = input.input;
  
  
  // As of 20210224, we dont really have how results from portals or searches should be listed or
  //  retrieved or given or whatever figured out, so we'll do a little bit of hackery for the
  //  moment.
  map<string,set<int>> medium_conf_isotopes, high_conf_isotopes;
  
  try
  {
    if( !input_file )
      throw runtime_error( "Invalid input SpecUtils::SpecFile ptr" );
  
    
    auto real_time_of_sample = [input_file]( const int sample ) -> float {
      float rt = 0.0;
      for( const auto &m : input_file->sample_measurements(sample) )
      {
        if( m
           && m->energy_calibration()
           && m->energy_calibration()->valid()
           && (m->num_gamma_channels() >= 32) )
          rt = (std::max)( rt, m->real_time() );
      }//for( const auto &m : input_file->sample_measurements(sample) )
      
      return rt;
    };//real_time_of_sample lamda
    
    
    int32_t nchannels = 0;
    set<int> background_samples;
    const set<int> &sample_numbers = input_file->sample_numbers();
    const vector<string> &det_names = input_file->detector_names();
    
    // We'll first go through and figure out the number of detectors, the number of channels, and
    //  energy binning we'll use for calling into GADRAS.
    //  - For portals, we'll only analyze detectors that have a background.
    //  - For search-mode, we'll only analyze detectors in the first time-slice of data.
    set<string> neutron_detector_names;
    
    map<string,shared_ptr<const SpecUtils::EnergyCalibration>> energy_cals;
    
    for( const int sample : sample_numbers )
    {
      for( const string &name : det_names )
      {
        const shared_ptr<const SpecUtils::Measurement> m = input_file->measurement( sample, name );
        if( !m )
          continue;
        
        // If we're dealing with a portal, we only want to analyze detectors we have data for.
        if( is_portal
           && !((m->occupied() == SpecUtils::OccupancyStatus::NotOccupied)
                || (m->source_type() == SpecUtils::SourceType::Background)) )
        {
          continue;
        }
        
        if( m->contained_neutron() )
          neutron_detector_names.insert( name );
        
        background_samples.insert( sample );
        
        shared_ptr<const SpecUtils::EnergyCalibration> cal = m->energy_calibration();
        
        if( cal && cal->valid() && (m->num_gamma_channels() >= 32) )
        {
          auto pos = energy_cals.find(name);
          if( pos == end(energy_cals) )
            pos = energy_cals.insert( {name,cal} ).first;
          
          // Use the energy calibration with the largest energy range for the detector
          assert( pos != end(energy_cals) );
          assert( pos->second && pos->second->valid() );
          
          if( pos->second != cal )
          {
            const float this_lower_energy = cal->lower_energy();
            const float this_upper_energy = cal->upper_energy();
            const float this_range = fabs( this_upper_energy - this_lower_energy );
            
            const float prev_lower_energy = pos->second->lower_energy();
            const float prev_upper_energy = pos->second->upper_energy();
            const float prev_range = fabs( prev_upper_energy - prev_lower_energy );
            
            if( this_range > prev_range )
              energy_cals[name] = cal;
          }//if( pos->second != cal )
          
          nchannels = (std::max)( nchannels, static_cast<int32_t>(pos->second->num_channels()) );
        }//if( energy calibration is valid and a spectrum )
      }//for( const string &name : det_names )
      
      // If search-mode, we will only analyze detectors present in first sample.
      //  We could probably also break here for portals as well, but we'll be conservative
      //  just in case the background sample numbers are a bit screwy, which can happen with
      //  some one-off portals I think.
      if( !is_portal && !energy_cals.empty() )
        break;
    }//for( const int sample : sample_numbers )
    
    
    if( energy_cals.empty() || (nchannels < 32) || background_samples.empty() )
      throw runtime_error( "No gamma spectra with valid energy calibrations found." );
    
    const size_t ndet = energy_cals.size();
    const bool use_raw_search = true; //(ndet > 1);
    
    int32_t init_code;
    if( use_raw_search )
      init_code = init_gadras_drf_raw( drf_folder, nchannels, static_cast<int32_t>(ndet), AutoGainAdjustType::K40 );
    else
      init_code = init_gadras_drf_calibrated( drf_folder, nchannels );
    
    result.gadras_intialization_error = init_code;
    
    // Check init code and throw exception if error
    check_init_results( init_code );
    
    const double finish_drf_init_time = SpecUtils::get_wall_time();
    
    // If we have a background sample that is like a minute or longer, and has data from all of the
    if( background_samples.size() > 1 )
    {
      //Lets find the longest background sample
      int longest_background_sample = -999999;
      float longest_background_rt = -999.9f;
      for( const int sample : background_samples )
      {
        const float rt = real_time_of_sample(sample);
        if( rt > 0 && !(isnan)(rt) && !(isinf)(rt) && rt > longest_background_rt )
        {
          longest_background_rt = rt;
          longest_background_sample = sample;
        }
      }//for( const int sample : background_samples )
      
      Wt::log("debug") << "Longest background was " << longest_background_rt << " seconds";
      
      if( longest_background_rt > 55.0f ) //55 seconds is arbitrary, but we just want something like a minute
      {
        auto meass = input_file->sample_measurements(longest_background_sample);
        size_t nmeas_back = 0;
        for( const auto &m : meass )
        {
          if( m->num_gamma_channels() >= 64
             && m->energy_calibration()
             && m->energy_calibration()->valid() )
            nmeas_back += 1;
        }
        
        // And if this sample has data for all of the gamma detectors, we will just this one
        if( nmeas_back == ndet )
        {
          background_samples.clear();
          background_samples.insert( longest_background_sample );
          Wt::log("debug") << "Setting background sample to only sample " << longest_background_rt
          << " which had real time " << static_cast<double>(longest_background_rt);
        }else
        {
          Wt::log("debug") << "Not setting background sample to only sample nmeas_back="
          << nmeas_back << " while ndet=" << ndet;
        }
      }
    }//if( background_samples.size() > 1 )
    
    
    //Try to fit energy cal now
    //  Note: due to GADRAS having a CPS above 1 MeV limit of 6, this code has not been tested, so
    //        we will not even attempt to use
    const bool do_energy_cal_on_background = false;
    if( do_energy_cal_on_background )
    {
      assert( g_RebinUsingK40 );
      
      bool did_recalibrate = false;
      const bool highres = (nchannels > 5000);
      
      for( const auto &name : input_file->detector_names() )
      {
        try
        {
          auto h = input_file->sum_measurements(background_samples, {name}, nullptr );
          if( !h
             || (h->num_gamma_channels() < 32)
             || !h->energy_calibration()
             || !h->energy_calibration()->valid()
             || ( (h->energy_calibration()->type() != SpecUtils::EnergyCalType::Polynomial)
                 && (h->energy_calibration()->type() != SpecUtils::EnergyCalType::UnspecifiedUsingDefaultPolynomial)
                 && (h->energy_calibration()->type() != SpecUtils::EnergyCalType::FullRangeFraction)
                 )
             || (h->live_time() < 60)
             )
          {
            continue;
          }
          
          const double ncounts_above_1MeV = h->gamma_integral( 1000, 3000 );
          if( (ncounts_above_1MeV / h->live_time()) >= 6 )
            continue;
        
          const bool highRes = (nchannels > 5000);
          const double ncounts_region = h->gamma_integral( 1260, 1660 );
          if( ncounts_region < (highRes ? 200 : 500) )
            continue;
          
          float centroid_K40 = 1460.0f;
          vector<float> rebinned_spectrum( nchannels + 2, 0.0f );
          vector<float> spectrum = *h->gamma_counts();
          vector<float> energies = *h->channel_energies();
          int32_t rval = g_RebinUsingK40( nchannels, h->live_time(), &(energies[0]),
                                         &(spectrum[0]), &(rebinned_spectrum[0]), &centroid_K40 );
          Wt::log("debug") << "For detector '" << name << "', got rval=" << rval << ", and centroid_K40=" << centroid_K40;
          
          if( rval == 0 )
          {
            auto cal = h->energy_calibration();
            const float true_k40_energy = 1460.75f;
            
            EnergyCal::RecalPeakInfo peak;
            peak.peakMean = centroid_K40;
            peak.peakMeanUncert = 1.0;
            peak.peakMeanBinNumber = cal->channel_for_energy(centroid_K40);
            peak.photopeakEnergy = true_k40_energy;
            
            assert( cal->coefficients().size() > 1 );
            vector<bool> fitfor( cal->coefficients().size(), false );
            fitfor[1] = true;  //only fit for linear coefficient
            vector<float> coefs = cal->coefficients();
            vector<float> coefs_uncert( cal->coefficients().size(), 0.0f );
            const auto &devpairs = cal->deviation_pairs();
            
            auto newcal = make_shared<SpecUtils::EnergyCalibration>();
            
            if( cal->type() == SpecUtils::EnergyCalType::FullRangeFraction )
            {
              EnergyCal::fit_energy_cal_frf( {peak}, fitfor, nchannels, devpairs, coefs, coefs_uncert );
              newcal->set_full_range_fraction( nchannels, coefs, devpairs );
            }else
            {
              EnergyCal::fit_energy_cal_poly( {peak}, fitfor, nchannels, devpairs, coefs, coefs_uncert );
              newcal->set_polynomial( nchannels, coefs, devpairs );
            }
            
            // There is a loop before where where if( energy_cals[name]->num_channels() != nchannels )
            //  then this will be fixed up
            //if( energy_cals.count(name)
            //   && energy_cals[name]
            //   && (energy_cals[name]->num_channels() == newcal->num_channels()) )
            energy_cals[name] = newcal;
            
            
            // Set the energy calibrations in the SpecFile
            //  TODO: see if instead we should call EnergyCal::propogate_energy_cal_change(...)
            for( auto &m : input_file->measurements() )
            {
              if( m && (m->num_gamma_channels() >= 32) && (m->detector_name() == name) )
              {
                did_recalibrate = true;
                input_file->set_energy_calibration( newcal, m );
              }
            }
            
            Wt::log("debug") << "Energy calibration was updated based on K40 peak for detector '"
            << name << "', moving channel "
            << peak.peakMeanBinNumber << " from " << peak.peakMean << " to "
            << peak.photopeakEnergy << " keV "
            << "(" << newcal->energy_for_channel(peak.peakMeanBinNumber) << ")";
          }else
          {
            Wt::log("debug") << "Not warning user energy calibration failed for detector '" << name << "'.";
          }
        }catch( std::exception &e )
        {
          Wt::log("error") << "Got exception recalibrating from background: " << e.what() ;
        }
      }//for( const auto &name : input_file->detector_names() )
      
      // If we did recalibration, make new result.spec_file so GUI knows to replot
      if( did_recalibrate )
        result.spec_file = make_shared<SpecUtils::SpecFile>(*input_file);
    }//if( do_energy_cal_on_background )

    
    
    // Now go through and get energy calibration to all have the same number of channels.
    //  Note that if a single detector, this will have no effect.
    for( auto &nv : energy_cals )
    {
      auto oldcal = nv.second;
      assert( oldcal && oldcal->valid() );
      if( oldcal->num_channels() == nchannels )
        continue;
      
      auto newcal = make_shared<SpecUtils::EnergyCalibration>();
      
      switch( oldcal->type() )
      {
        case SpecUtils::EnergyCalType::Polynomial:
        case SpecUtils::EnergyCalType::UnspecifiedUsingDefaultPolynomial:
        {
          newcal->set_polynomial( nchannels, oldcal->coefficients(), oldcal->deviation_pairs() );
          break;
        }//case Polynomial
          
        case SpecUtils::EnergyCalType::FullRangeFraction:
        {
          const vector<float> polycoefs
              = SpecUtils::fullrangefraction_coef_to_polynomial( oldcal->coefficients(),
                                                                 oldcal->num_channels() );
          newcal->set_polynomial( nchannels, polycoefs, oldcal->deviation_pairs() );
          break;
        }//case SpecUtils::EnergyCalType::FullRangeFraction:
          
        case SpecUtils::EnergyCalType::LowerChannelEdge:
        {
          assert( oldcal->channel_energies() );
          assert( nchannels >= oldcal->num_channels() );
          const size_t nprevchan = oldcal->num_channels();
          const float prev_upper_energy = oldcal->upper_energy();
          vector<float> channel_energies = *oldcal->channel_energies();
          channel_energies.resize(nchannels + 1);
          const float upper_energy = oldcal->upper_energy()
                                         * static_cast<double>(nchannels)
                                           / static_cast<double>(nprevchan);
          
          if( nchannels > nprevchan )  // This should always be true, but jic
          {
            assert( channel_energies[nprevchan] == prev_upper_energy );
            const float delta = (upper_energy - prev_upper_energy) / (nchannels - nprevchan);
            for( size_t i = nprevchan + 1; i <= nchannels; ++i )
              channel_energies[i] = prev_upper_energy + delta*(i - nprevchan);
            assert( fabs(channel_energies.back() - upper_energy) < (std::max)(0.01,upper_energy*1.0E-4) );
          }
          
          newcal->set_lower_channel_energy( nchannels, channel_energies );
          break;
        }//case SpecUtils::EnergyCalType::LowerChannelEdge:
          
        case SpecUtils::EnergyCalType::InvalidEquationType:
          assert(0);
          throw runtime_error( "Totally wack energy cal" );
          break;
      }//switch( oldcal->type() )
      
      assert( newcal && newcal->valid() );
      
      //now update.
      nv.second = newcal;
    }//for( auto &nv : energy_cals )
    

    // If we have multiple detectors (ex, a portal), we will use the "raw" function calls
    //  (InitializeIsotopeIdRaw and StreamingSearch).  These functions make us pass in channel
    //  counts as int32_t's, and also fit for energy calibration as it goes along in time; they also
    //  accommodate multiple detectors.
    //
    // If we have a single detector then we will use the "calibrated" (InitializeIsotopeIdCalibrated
    //  and SearchIsotopeID) routines.  These only allow using with a single detector, but take in
    //  float channel counts.
    
    
    
    // TODO: figure out if we should linearize the data from the file when doing raw_search
    // TODO: figure out how to decide if we should use K40 or Th232 peak for energy calibration
    
    vector<float> energy_max( ndet, 0.0f );
    vector<string> gamma_det_names( ndet );
    vector<float> energy_binning_of_summed( nchannels + 1 );
    shared_ptr<const SpecUtils::EnergyCalibration> cal_of_summed;
    
    size_t detindex = 0;
    for( const auto &nv : energy_cals )
    {
      assert( detindex < gamma_det_names.size() );
      assert( nv.second && nv.second->valid() && nv.second->channel_energies() );
      assert( (nchannels + 1) == nv.second->channel_energies()->size() );
      
      gamma_det_names[detindex] = nv.first;
      energy_max[detindex] = nv.second->upper_energy();
      energy_binning_of_summed = *nv.second->channel_energies();
      cal_of_summed = nv.second;
      
      ++detindex;
    }//for( const auto &nv : energy_cals )
  
    
    int call_stat = 0;
    vector<float> live_times( ndet, 0.0f ), real_times( ndet, 0.0f );
    vector<int32_t> spectrum_buffer( ndet*nchannels, 0 ); //for raw search
    
    float summed_live_time = 0.0f, summed_real_time = 0.0f;
    vector<float> channel_counts_summed( nchannels, 0.0f ); //for calibrated search
    
    float stuff_of_interest = 0.0f, rate_not_norm = 0.0f;
    std::vector<int> det_stat(ndet, 0);
    char *isotope_string = nullptr;
    int32_t neutrons = 0;
    
    
    auto fill_inputs = [&]( const set<int> &samples_to_get ){
      // To initialize the search we will firsts sum
      
      summed_live_time = summed_real_time = 0.0f;
      for( float &f : channel_counts_summed )
        f = 0.0f;
      
      for( size_t det_index = 0; det_index < ndet; ++det_index )
      {
        const string &name = gamma_det_names[det_index];
        assert( energy_cals.count(name) );
        assert( energy_cals[name] );
        
        std::shared_ptr<const SpecUtils::Measurement> h;
        
        if( samples_to_get.size() > 1 )
          h = input_file->sum_measurements(samples_to_get, {name}, energy_cals[name] );
        else if( samples_to_get.size() == 1 )
          h = input_file->measurement(*begin(samples_to_get), name);
        else
          throw runtime_error( "Logic error - now samples specified for fill_inputs lambda" );
        
        if( !h
           || !h->gamma_channel_contents()
           || (h->num_gamma_channels() < 64)
           || !h->gamma_channel_energies()
           || !h->energy_calibration()
           || !h->energy_calibration()->valid() )
        {
          string samplestr;
          for( const auto &s : samples_to_get )
            samplestr += (samplestr.empty() ? "" : ", ") + s;
          
          Wt::log("debug") << "Missing samples {" << samplestr << "} for detector '" << name << "'";
          continue;
        }//if( this isnt a gamma spectrum ).
        
        assert( energy_cals.count(name) );
        
        const auto cal = energy_cals[name];
        assert( cal && cal->channel_energies() );
        
        vector<float> countsv_indiv = *h->gamma_channel_contents();
        vector<float> countsv_sum = *h->gamma_channel_contents();
        
        if( h->energy_calibration() != cal_of_summed )
        {
          SpecUtils::rebin_by_lower_edge( *h->gamma_channel_energies(),
                                         *h->gamma_channel_contents(),
                                         energy_binning_of_summed,
                                         countsv_sum );
        }
        
        if( h->energy_calibration() != cal )
        {
          SpecUtils::rebin_by_lower_edge( *h->gamma_channel_energies(),
                                         *h->gamma_channel_contents(),
                                         *cal->channel_energies(),
                                         countsv_indiv );
        }//if( h->energy_calibration() != cal )
        
        assert( countsv_sum.size() == nchannels );
        assert( countsv_indiv.size() == nchannels );
      
        summed_live_time += h->live_time();
        summed_real_time += h->real_time();
        
        for( size_t i = 0; i < nchannels; ++i )
        {
          const float counts_for_sum = ((i < countsv_sum.size()) ? countsv_sum[i] : 0.0f);
          if( i < countsv_sum.size() && !(std::isnan)(counts_for_sum) && !(std::isinf)(counts_for_sum) )
          {
            channel_counts_summed[i] += counts_for_sum;
          }else
          {
            //channel_counts_summed[i] += 0.0f;
          }
          
          const float counts_indiv = ((i < countsv_indiv.size()) ? countsv_indiv[i] : 0.0f);
          if( i < countsv_indiv.size() && !(std::isnan)(counts_indiv) && !(std::isinf)(counts_indiv) )
          {
            spectrum_buffer[det_index*nchannels + i] = static_cast<int32_t>( std::round(counts_indiv) );
          }else
          {
            spectrum_buffer[det_index*nchannels + i] = 0;
          }
        }//for( size_t i = 0; i < nchannels; ++i )
        
        live_times[det_index] = std::max( h->live_time(), 0.0f );
        real_times[det_index] = std::max( h->real_time(), 0.0f );
      }//for( size_t i = 0; i < ndet; ++i )
      
      
      double sum_neutrons = 0.0;
      for( const int sample : samples_to_get )
      {
        for( auto m : input_file->sample_measurements(sample) )
        {
          if( m->contained_neutron() )  //Probably not needed
            sum_neutrons += m->neutron_counts_sum();
        }
      }//for( loop over to get neutron counts )
      
      if( (sum_neutrons < 0.0) || (std::isinf)(sum_neutrons) || (std::isnan)(sum_neutrons) )
        sum_neutrons = 0.0;
      neutrons = static_cast<int32_t>( std::round(sum_neutrons) );
    };//fill_inputs lambda
    
    
    auto zero_inputs = [&](){
      neutrons = 0;
      summed_live_time = summed_real_time = 0.0f;
      for( float &f : live_times )        f = 0.0f;
      for( float &f : real_times )        f = 0.0f;
      for( int32_t &c : spectrum_buffer ) c = 0;
      for( float &f : channel_counts_summed )    f = 0.0f;
#ifndef _WIN32 //crashes on Windows for some reason
      if( isotope_string )
        free( isotope_string );
      isotope_string = nullptr;
#endif
    };//zero_inputs lambda
    
    
    // Get the background info.
    fill_inputs( background_samples );
    
    if( use_raw_search )
    {
      assert( g_StreamingSearch );
      call_stat = g_StreamingSearch( &(live_times[0]), &(real_times[0]), &(spectrum_buffer[0]),
                                     &stuff_of_interest, &isotope_string, &(energy_max[0]),
                                     AnalysisMode::INITIALIZE, &(det_stat[0]), neutrons,
                                     &rate_not_norm );
      

      if( call_stat < 0 )
      {
        result.gadras_analysis_error = call_stat;
        throw std::runtime_error( "Failed to initialize StreamingSearch: "
                                  + stream_search_result_str(call_stat) );
      }
      
      //For simple: StaticSearch does energy cal,
      Wt::log("debug") << "Initialization call for StreamingSearch returned: "
                           << stream_search_result_str(call_stat);
      
      // Now call in to actually use the background
      call_stat = g_StreamingSearch( &(live_times[0]), &(real_times[0]), &(spectrum_buffer[0]),
                                    &stuff_of_interest, &isotope_string, &(energy_max[0]),
                                    AnalysisMode::ANALYZE, &(det_stat[0]), neutrons,
                                    &rate_not_norm );
      
      //For simple: StaticSearch does energy cal,
      Wt::log("debug") << "Background analysis call for StreamingSearch returned: "
                           << stream_search_result_str(call_stat);
      
      if( call_stat < 0 )
      {
        result.gadras_analysis_error = call_stat;
        throw std::runtime_error( "Failed to analyze background in StreamingSearch: "
                                  + stream_search_result_str(call_stat) );
      }
    }else
    {
      assert( ndet == 1 );
      assert( g_SearchIsotopeID );
      call_stat = g_SearchIsotopeID( summed_live_time, summed_real_time, &(channel_counts_summed[0]),
                                     &stuff_of_interest, &isotope_string, AnalysisMode::INITIALIZE,
                                     &(energy_binning_of_summed[0]), neutrons, &rate_not_norm );
       
      // TODO: Lee free()'s energy_binning_of_summed after each call to SearchIsotopeID - do we have to? Should we?
      
      Wt::log("debug") << "Initialization call for SearchIsotopeID returned code " << call_stat;
      
      if( call_stat < 0 )
      {
        result.gadras_analysis_error = call_stat;
        throw std::runtime_error( "Failed to initialize StreamingSearch: "
                                  + stream_search_result_str(call_stat) );
      }
      
      // Now call in to actually analyze the background
      call_stat = g_SearchIsotopeID( summed_live_time, summed_real_time, &(channel_counts_summed[0]),
                                    &stuff_of_interest, &isotope_string, AnalysisMode::ANALYZE,
                                    &(energy_binning_of_summed[0]), neutrons, &rate_not_norm );
      
      Wt::log("debug") << "First analysis call (background) for SearchIsotopeID returned code " << call_stat;
      
      if( call_stat < 0 )
      {
        result.gadras_analysis_error = call_stat;
        throw std::runtime_error( "First analysis call (background) for SearchIsotopeID: "
                                  + stream_search_result_str(call_stat) );
      }
    }//if( use_raw_search ) / else
    
    // Zero everything out, JIC.
    zero_inputs();
    
    // We've iniitalized gadras analysis, so we need to make sure to reset it, even if things fail
    DoWorkOnDestruct do_reset_ana( [=](){
      char *dummy_str = nullptr;
      int32_t call_stat = 0, neutrons_nummy = 0;
      float soi_dummy = 0.0f, notnorm_dumy = 0.0f;
      
      if( use_raw_search )
      {
        std::vector<int> det_stat_dummy(ndet, 0);
        vector<float> energy_max_dummy( ndet, 0.0f );
        vector<float> lt_dummy( ndet, 0.0f ), rt_dummy( ndet, 0.0f );
        vector<int32_t> spectrum_dummy( ndet*nchannels, 0 ); //for raw search
        
        call_stat = g_StreamingSearch( &(lt_dummy[0]), &(rt_dummy[0]), &(spectrum_dummy[0]),
                                      &soi_dummy, &dummy_str, &(energy_max_dummy[0]),
                                      AnalysisMode::RESET, &(det_stat_dummy[0]), neutrons_nummy,
                                      &notnorm_dumy );
      }else
      {
        vector<float> channel_counts_dummy( nchannels, 0.0f ); //for calibrated search
        vector<float> energy_binning_dummy( nchannels + 1 );
        
        call_stat = g_SearchIsotopeID( summed_live_time, summed_real_time, &(channel_counts_dummy[0]),
                                      &soi_dummy, &dummy_str, AnalysisMode::RESET,
                                      &(energy_binning_dummy[0]), neutrons_nummy, &notnorm_dumy );
      }//if( use_raw_search ) / else
      
      Wt::log("debug") << "Have RESET analysis with returned code " << call_stat;
    } );
    
    
    
    vector<pair<float,set<int>>> real_time_and_samples;
    
    // Now loop over and analyze the data
    for( auto sample_iter = begin(sample_numbers); sample_iter != end(sample_numbers); ++sample_iter )
    {
      float real_time = 0.0f;
      set<int> samples;
      
      // While sum time-segments until we get to about 0.5seconds.  So for portals with 0.1s time
      //  intervals, we will sum about five of these.  This is based on Dean telling me at some
      //  point that he used 0.5s intervals to optimize the algorithms - I could have mis-understood
      //  or be mis-remembering though!
      for( auto sample_iter = begin(sample_numbers);
           ((real_time < 0.425f) && (sample_iter != end(sample_numbers)));
          ++sample_iter )
      {
        if( background_samples.count(*sample_iter) )
          continue;
        
        real_time += real_time_of_sample(*sample_iter);
        samples.insert( *sample_iter );
      }//for( find samples to sum up )
      
      if( samples.empty() )
        throw runtime_error( "Logic-error: did the file only contain background?" );
      
      if( real_time <= 0.00001f )
      {
        if( sample_iter != end(sample_numbers) )
          throw runtime_error( "Logic-error: zero-second time interval sum, but we didnt reach end of samples" );
        break;
      }
      
      real_time_and_samples.push_back( {real_time,samples} );
      
      // Grab the data
      fill_inputs( samples );
      
      if( use_raw_search )
      {
        call_stat = g_StreamingSearch( &(live_times[0]), &(real_times[0]), &(spectrum_buffer[0]),
                                      &stuff_of_interest, &isotope_string, &(energy_max[0]),
                                      AnalysisMode::ANALYZE, &(det_stat[0]), neutrons,
                                      &rate_not_norm );
        if( call_stat < 0 )
          Wt::log("debug") << "ANALYZE call to StreamingSearch returned: "
                               << stream_search_result_str(call_stat);
        
        if( call_stat < 0 )
        {
          result.gadras_analysis_error = call_stat;
          throw std::runtime_error( "Failed to initialize StreamingSearch: " + stream_search_result_str(call_stat) );
        }
      }else
      {
        call_stat = g_SearchIsotopeID( summed_live_time, summed_real_time, &(channel_counts_summed[0]),
                                      &stuff_of_interest, &isotope_string, AnalysisMode::ANALYZE,
                                      &(energy_binning_of_summed[0]), neutrons, &rate_not_norm );
        
        // TODO: Lee free()'s energy_binning_of_summed after each call to SearchIsotopeID - do we have to? Should we?
        
        if( call_stat < 0 )
          Wt::log("debug") << "ANALYZE call for SearchIsotopeID returned code " << call_stat;
        
        if( call_stat < 0 )
        {
          result.gadras_analysis_error = call_stat;
          throw std::runtime_error( "Failed to initialize StreamingSearch: " + stream_search_result_str(call_stat) );
        }
      }//if( use_raw_search ) / else
      
      //if( isotope_string )
      //  Wt::log("debug") << "Analysis call returned " << std::string(isotope_string);
      //else
      //  Wt::log("debug") << "Analysis call returned NULL";
      
      // Lets get analysis results
      assert( g_GetCurrentIsotopeIDResults && g_ClearIsotopeIDResults );
      IsotopeIDResult id_result;
      
      g_GetCurrentIsotopeIDResults( &id_result );
      
      // I dont know how to access if the current detector is low, medium, or high resolution,
      //  so to tell the ID confidence, we will parse the isotope_string, and use this, instead of
      //  the numerical confidence stuff
      map<string,string> iso_to_conf = get_iso_to_conf( isotope_string );
      
      for( const auto &r : iso_to_conf )
      {
        if( r.second == "H" )
          high_conf_isotopes[r.first].insert( begin(samples), end(samples) );
        else if( r.second == "F" )
          medium_conf_isotopes[r.first].insert( begin(samples), end(samples) );
        else if( (r.second != "L") && !(r.second=="" && r.first=="NONE") )
          Wt::log("debug") << "Unknown confidence '" << r.second << "' from isostr='" << r.first << "'";
      }//for( const auto &r : iso_to_conf )
        
      if( id_result.nIsotopes > 0 )
      {
        vector<string> isotope_names, isotope_types, isotope_confidences_str;
        std::vector<float> isotope_count_rates, isotope_confidences;
        
        if( id_result.listOfIsotopeStrings )
          SpecUtils::split( isotope_names, id_result.listOfIsotopeStrings, "," );
        
        if( id_result.listOfIsotopeTypes )
          SpecUtils::split( isotope_types, id_result.listOfIsotopeTypes, "," );
        
        isotope_count_rates.resize( id_result.nIsotopes, -1.0f );
        isotope_confidences.resize( id_result.nIsotopes, -1.0f );
        isotope_confidences_str.resize( id_result.nIsotopes, "" );
        
        for( int32_t i = 0; i < id_result.nIsotopes; ++i )
        {
          isotope_count_rates[i] = id_result.isotopeCountRates[i];
          isotope_confidences[i] = id_result.isotopeConfidences[i];
          if( (i < isotope_names.size()) && iso_to_conf.count(isotope_names[i]) )
            isotope_confidences_str[i] = iso_to_conf[isotope_names[i]];
        }
        
        assert( static_cast<size_t>(id_result.nIsotopes) == isotope_names.size() );
        assert( static_cast<size_t>(id_result.nIsotopes) == isotope_types.size() );
        const size_t nisos = std::min(static_cast<size_t>(id_result.nIsotopes), isotope_names.size());
        
        for( size_t i = 0; i < nisos; ++i )
        {
          const string &name = isotope_names[i];
          const string type = (i < isotope_types.size()) ? isotope_types[i] : string("");
          
          auto pos = std::find( begin(result.isotope_names), end(result.isotope_names), name );
          if( pos == end(result.isotope_names) )
          {
            result.isotope_names.push_back( name );
            result.isotope_types.push_back( type );
            result.isotope_count_rates.push_back( -1.0f );
            result.isotope_confidences.push_back( -1.0f );
            result.isotope_confidence_strs.push_back( "" );
            pos = end(result.isotope_names) - 1;
          }//if( pos == end(result.isotope_names) )
          
          const auto result_index = pos - begin(result.isotope_names);
          assert( result_index < result.isotope_names.size() );
          assert( result_index < result.isotope_types.size() );
          assert( result_index < result.isotope_count_rates.size() );
          assert( result_index < result.isotope_confidences.size() );
          assert( result_index < result.isotope_confidence_strs.size() );
          
          result.isotope_count_rates[result_index] = std::max( result.isotope_count_rates[result_index],
                                                        id_result.isotopeCountRates[i] );
          
          if( id_result.isotopeConfidences[i] > result.isotope_confidences[result_index] )
          {
            result.isotope_confidences[result_index] = id_result.isotopeConfidences[i];
            if( iso_to_conf.count(name) )
              result.isotope_confidence_strs[result_index] = iso_to_conf[name];
          }
        }//for( loop over isotopes )
        
#ifndef _WIN32
        if (id_result.listOfIsotopeStrings )
          free(id_result.listOfIsotopeStrings);
        
        if( id_result.listOfIsotopeTypes )
          free(id_result.listOfIsotopeTypes);
#endif
      }//if( idResults.nIsotopes > 0 )
      
        
      g_ClearIsotopeIDResults();
      
      // Zero everything out
      zero_inputs();
    }//for( const int sample : sample_numbers )
    
    
    
    result.isotopes = "";
    for( const auto &iso : high_conf_isotopes )
      result.isotopes += (result.isotopes.empty() ? "" : "+") + iso.first + "(H)";
      
    for( const auto &iso : medium_conf_isotopes )
    {
      if( !high_conf_isotopes.count(iso.first) )
        result.isotopes += (result.isotopes.empty() ? "" : "+") + iso.first + "(M)";
    }
    
    for( int i = 0; i < static_cast<int>(result.isotope_names.size()); ++i )
    {
      const string name = result.isotope_names[i];
      const float confidence = result.isotope_confidences[i];
      
      const bool highRes = (nchannels > 5000);
      const float fairThreshold = highRes ? 2.3f : 1.9;
      
      {//begin debug code
        string conf;
        if( high_conf_isotopes.count(name) )
          conf += "H";
        if( medium_conf_isotopes.count(name) )
          conf += "M";
        
        Wt::log("debug") << "Got '" << name << "' with confidence " << conf << " and "
                             << confidence << " that is of category " << result.isotope_types[i]
                             << " and count rate " << result.isotope_count_rates[i];
      }//end debug code
      
      if( !high_conf_isotopes.count(name)
         && !medium_conf_isotopes.count(result.isotope_names[i])
         && (confidence < fairThreshold) )
      {
        Wt::log("debug") << "Removing isotope " << result.isotope_names[i]
                             << " with confidence " << confidence << " from results, since it wasnt"
                             << " medium or high confidence";
        
        result.isotope_names.erase( begin(result.isotope_names) + i );
        result.isotope_types.erase( begin(result.isotope_types) + i );
        result.isotope_count_rates.erase( begin(result.isotope_count_rates) + i );
        result.isotope_confidences.erase( begin(result.isotope_confidences) + i );
        result.isotope_confidence_strs.erase( begin(result.isotope_confidence_strs) + i );
      }
    }//for( loop over isotopes
    
    assert( result.isotope_types.size() == result.isotope_names.size() );
    assert( result.isotope_count_rates.size() == result.isotope_names.size() );
    assert( result.isotope_confidences.size() == result.isotope_names.size() );
    assert( result.isotope_confidence_strs.size() == result.isotope_names.size() );
    
    if( input.analysis_type == Analysis::AnalysisType::Portal )
      result.analysis_warnings.push_back( "The search-mode analysis algorithm was used for this RPM"
                                         " data, pending proper RPM replay implementation" );
    
    if( !do_energy_cal_on_background )
      result.analysis_warnings.push_back( "The displayed data has not been updated to the fit energy"
                                          " calibration, pending implementation." );
    
    result.gadras_analysis_error = 0;
  }catch( std::exception &e )
  {
    result.error_message = e.what();
    Wt::log("error") << "Analysis failed due to: " << e.what();
    
    
    // We may need to call StreamingSearch( ... AnalysisMode::RESET ... ) or
    //  SearchIsotopeID( ... AnalysisMode::RESET ... ) to clean up...
    
  }//try - catch do the analysis

  
  const string &wt_app_id = input.wt_app_id;
  function<void(Analysis::AnalysisOutput)> callback = input.callback;
  
  if( callback )
  {
    auto server = Wt::WServer::instance();
    if( server && !wt_app_id.empty() )
    {
      server->post( wt_app_id, [result,callback](){
        callback( result );
        wApp->triggerUpdate();
        
        Wt::log("debug") << "Update should have triggered to GUI from search/portal analysis";
      } );
    }else if( wt_app_id.empty() )
    {
      Wt::log("debug") << "wt_app_id is empty from search/portal analysis...";
      
      callback( result );
    }else //if( !wt_app_id.empty() )
    {
      Wt::log("error") << "Error: got non empty Wt session ID ('" << wt_app_id
      << "'), but there is no WServer instance - not calling result callback for search/portal!";
    }
  }//if( callback )
}//void do_search_analysis( Analysis::AnalysisInput input )



void do_portal_analysis( Analysis::AnalysisInput input )
{
  std::lock_guard<std::mutex> ana_lock( g_gad_mutex );
  
  assert( g_PortalIsotopeIDCInterface );
  
  const double start_time = SpecUtils::get_wall_time();
  
  string ana_tmp_pcf_path;
  
  // Note, if we do any energy recalibration or rebinning or anything, we will change input_file
  //  to point to a new object with the new values
  shared_ptr<SpecUtils::SpecFile> input_file = input.input;
  
  Analysis::AnalysisOutput result;
  result.ana_number = input.ana_number;
  result.drf_used = input.drf_folder;
  result.chi_sqr = -1.0f;
  result.alarm_basis_duration = -1.0f;
  result.spec_file = input.input;
  
  /// TODO: is there any analysis_warnings we should look for and fill out?
  result.analysis_warnings.push_back( "Portal analysis is still under development - interpret results with care." );
  
  try
  {
    const double start_drf_init_time = SpecUtils::get_wall_time();
   
    int32_t nchannels_dummy = 0;
    
    const string drf_rel_path = SpecUtils::append_path( "drfs", input.drf_folder );
    const string drf_full_folder = SpecUtils::append_path( g_gad_app_folder, drf_rel_path );
    const int32_t init_code = init_gadras_drf_calibrated( drf_rel_path, nchannels_dummy );
    
    result.gadras_intialization_error = init_code;
    
    const double finish_drf_init_time = SpecUtils::get_wall_time();
    
    // Check init code and throw exception if error
    check_init_results( init_code );
    
    string db_path = SpecUtils::append_path( drf_full_folder, "DB.pcf" );
    
    if( !SpecUtils::is_file( db_path ) )
    {
      Wt::log("error") << "GADRAS database doesnt appear to exist at '" << db_path << "'";
      throw runtime_error( "Issue finding database for analysis." );
    }
    
    if( !input_file )
    {
      Wt::log("error") << "Somehow no input SpecFile was specified for RPM analysis";
      throw runtime_error( "Issue with input file to analysis." );
    }
    
    {// begin write temporary PCF file for analysis
      // Note, the filename MUST end in ".pcf" or else analysis will silently
      const string ana_filename = SpecUtils::temp_file_name( "rpm_ana_tmp_" + input.wt_app_id,
                                                            SpecUtils::temp_dir() )
                                  + ".pcf";
      
      std::ofstream tmp_pcf( ana_filename.c_str(), ios::out | ios::binary );
    
      if( !tmp_pcf )
      {
        Wt::log("error") << "Failed to open temporary file '" << ana_filename << "'.";
        throw runtime_error( "Could not create temporary file for analysis." );
      }
      
      ana_tmp_pcf_path = ana_filename;
      if( !input_file->write_pcf( tmp_pcf ) )
      {
        Wt::log("error") << "Failed to write PCF file to temp file '" << ana_filename << "'.";
        throw runtime_error( "Error creating file for analysis." );
      }
    }// End write temporary PCF file for analysis
    
    assert( !db_path.empty() );
    assert( !ana_tmp_pcf_path.empty() );
    
    // Instead of malloc'ing the buffers GADRAS will write to, we will just use
    //  fixed size arrays so we dont have to worry about cleaning up memory.
    //  We will allocate one more character than needed by GADRAS, jic.
    char energyCalibratorOverrideTag[2] = { '\0' };
    char dateTime[24] = { '\0' };
    char snmProbabilityString[13] = { '\0' };
    char threatProbabilityString[13] = { '\0' };
    char eventType[17] = { '\0' };
    char alarmColor[17] = { '\0' };
    char alarmDescription[17] = { '\0' };
    char isotopeString[129] = { '\0' };
    char message[1025] = { '\0' };
    
    
    struct PortalIsotopeIDOptions portalIsotopeIDOptions;
    //energy calibration types: NO_ECAL_ROUTINE, BASIC_ECAL_ROUTINE, DEFAULT_ECAL_ROUTINE, GAIN_ADJUST_ECAL_ROUTINE
    // but only sanity-tested with BASIC_ECAL_ROUTINE
    portalIsotopeIDOptions.energyCalibrator = BASIC_ECAL_ROUTINE;
    
    energyCalibratorOverrideTag[0] = 'k'; // could also be 't' for BASIC_ECAL_ROUTINE
    portalIsotopeIDOptions.energyCalibratorOverrideTag = energyCalibratorOverrideTag;
    
    // TODO: figure out if we should make the following options available to the user
    portalIsotopeIDOptions.gammaRateAlarm = 10.0; // default in the gui
    portalIsotopeIDOptions.RDDActivity = 1.0; // default in the gui
    portalIsotopeIDOptions.falseAlarmParameter = 1.0; // default in the gui
    portalIsotopeIDOptions.allowBackgroundScalingFlag = 1; // default to true
    portalIsotopeIDOptions.showActivityEstimateFlag = 0; // default to false
    portalIsotopeIDOptions.showNumericalConfidencesFlag = 0; //default to false, I think Will is using the (H)(F)(L) from IsotopeString
    portalIsotopeIDOptions.simpleModeFlag = 0; // default to false
    
    struct PortalPlotOptions portalPlotOptions;
    portalPlotOptions.graphNumber = 0;
    portalPlotOptions.fillTemplatesFlag = 1;
    portalPlotOptions.stripBackgroundFlag = 1;
    
    
    struct PortalIsotopeIDOutput ana_out;
    // greg requires all of the strings to have MAXLEN values for each string
    // which is kind of a mess
    ana_out.dateTime = dateTime;
    ana_out.foregroundTotalTime = 0.0;
    ana_out.backgroundTotalTime = 0.0;
    ana_out.netGammaRate = 0.0;
    ana_out.netNeutronRate = 0.0;
    ana_out.sigmaGamma = 0.0;
    ana_out.sigmaNeutron = 0.0;
    ana_out.chiSquare = 0.0;
    ana_out.snmProbabilityIndex = 0.0;
    ana_out.snmProbabilityString = snmProbabilityString;
    ana_out.threatProbabilityIndex = 0;
    ana_out.threatProbabilityString = threatProbabilityString;
    ana_out.eventType = eventType;
    ana_out.alarmColor = alarmColor;
    ana_out.alarmDescription = alarmDescription;
    ana_out.isotopeString = isotopeString;
    
    const double setup_finished_time = SpecUtils::get_wall_time();
    
    int writePlotFlag = 0; // don't try writing just yet
    const int call_stat = g_PortalIsotopeIDCInterface( &(db_path[0]), &(ana_tmp_pcf_path[0]),
                                                  &portalIsotopeIDOptions,
                                                  writePlotFlag,
                                                  &portalPlotOptions,
                                                  &ana_out,
                                                  message);
    
    
    // We'll double make sure we wont have any buffer overflows.  Shouldnt actually be necessary.
    null_terminate_static_str( energyCalibratorOverrideTag );
    null_terminate_static_str( dateTime );
    null_terminate_static_str( snmProbabilityString );
    null_terminate_static_str( threatProbabilityString );
    null_terminate_static_str( eventType );
    null_terminate_static_str( alarmColor );
    null_terminate_static_str( alarmDescription );
    null_terminate_static_str( isotopeString );
    null_terminate_static_str( message );
    
    const double call_finished_time = SpecUtils::get_wall_time();
    
    printf("Isotope String: %s\n", ana_out.isotopeString);
    
    const string isostr = ana_out.isotopeString;
    
    Wt::log("debug") << "Portal analysis returned code " << call_stat
                     << " and isotope string '" << isostr.c_str() << "'";
    
    if( call_stat < 0)
      throw runtime_error( "An analysis error occurred or template database was not found." );
    
    result.stuff_of_interest = -1.0f;
    result.rate_not_norm = -1.0f;
    
    //isostr looks something like: "Cs137(H)", "Cs137(H)+Ba133(F)", "None", etc.
    result.isotopes = isostr;
    
    result.gadras_analysis_error = call_stat;
    
    if( call_stat >= 0)
    {
      result.chi_sqr = ana_out.chiSquare;
      result.stuff_of_interest = -1;
      result.rate_not_norm = -1;
      result.alarm_basis_duration = -1;
      
      cout << "\n\nPortal analysis quantities currently not used:\n"
      "\tDateTime: " << ana_out.dateTime << "\n"
      "\tforegroundTotalTime=" << ana_out.foregroundTotalTime << "\n"
      "\tbackgroundTotalTime=" << ana_out.backgroundTotalTime << "\n"
      "\tnetGammaRate=" << ana_out.netGammaRate << "\n"
      "\tnetNeutronRate=" << ana_out.netNeutronRate << "\n"
      "\tsigmaGamma=" << ana_out.sigmaGamma << "\n"
      "\tsigmaNeutron=" << ana_out.sigmaNeutron << "\n"
      "\tsnmProbabilityIndex=" << ana_out.snmProbabilityIndex << "\n"
      "\tsnmProbabilityString=" << ana_out.snmProbabilityString << "\n"
      "\tthreatProbabilityIndex=" << ana_out.threatProbabilityIndex << "\n"
      "\tthreatProbabilityString=" << ana_out.threatProbabilityString << "\n"
      "\teventType=" << ana_out.eventType << "\n"
      "\talarmColor=" << ana_out.alarmColor << "\n"
      "\talarmDescription=" << ana_out.alarmDescription << "\n"
      "\tisotopeString=" << ana_out.isotopeString << "\n"
      "\n\n" << endl;
      
      
      const map<string,string> iso_to_conf = get_iso_to_conf( isostr.c_str() );
                
      for( const auto isoconf : iso_to_conf )
      {
        result.isotope_names.push_back( isoconf.first );
        result.isotope_confidence_strs.push_back( isoconf.second );
        
        // It appears we dont have these next three quantities, but we still need to put something
        //  in.
        
        // For isotope_types we'll use alarmDescription, which is something like "Industrial",
        //  since thats all we have
        if( iso_to_conf.size() == 1 )
          result.isotope_types.push_back( alarmDescription );
        else
          result.isotope_types.push_back( "[" + string(alarmDescription) + "]" );
        
        result.isotope_count_rates.push_back( -1.0f ); //netGammaRate?
        result.isotope_confidences.push_back( -1.0f );
      }//for( const auto isoconf : iso_to_conf )
    }else
    {
      result.error_message = "Portal-style analysis failed with error code " + std::to_string(call_stat);
    }//if( call_stat >= 0) / else
    
    
    Wt::log("debug") << "Finished with analysis: '" << isostr.c_str() << "'";
    
    const double finished_time = SpecUtils::get_wall_time();
    
    const double total_time = finished_time - start_time;
    const double drf_init_time = finish_drf_init_time - start_drf_init_time;
    const double setup_time = setup_finished_time - start_time;
    const double gadras_time = call_finished_time - setup_finished_time;
    
    Wt::log("debug") << "Analysis took\n"
    << "\t\tSetup Time:    " << setup_time << "\n"
    << "\t\tDRF init Time: " << drf_init_time << "\n"
    << "\t\tAna Time:      " << gadras_time << "\n"
    << "\t\tTotal Time:    " << total_time << "\n"
    ;
    
    Wt::log("info") << "Analysis took\n"
    << "\t\tSetup Time: " << setup_time << "\n"
    //<< "\t\tDRF init Time: " << drf_init_time << "\n"
    << "\t\tAna Time:   " << gadras_time << "\n"
    << "\t\tTotal Time: " << total_time << "\n";
  }catch( std::exception &e )
  {
    result.error_message = e.what();
    Wt::log("error") << "Analysis failed due to: " << e.what();
  }
  
  if( !ana_tmp_pcf_path.empty() )
  {
    if( !SpecUtils::remove_file(ana_tmp_pcf_path) )
      Wt::log("error") << "Failed to remove temporary RPM analysis PCF file '"
                       << ana_tmp_pcf_path << "'";
  }
  
  
  const string &wt_app_id = input.wt_app_id;
  function<void(Analysis::AnalysisOutput)> callback = input.callback;
  
  if( callback )
  {
    auto server = Wt::WServer::instance();
    if( server && !wt_app_id.empty() )
    {
      server->post( wt_app_id, [result,callback](){
        callback( result );
        wApp->triggerUpdate();
        
        Wt::log("debug") << "Update should have triggered to GUI";
      } );
    }else if( wt_app_id.empty() )
    {
      Wt::log("debug") << "wt_app_id is empty...";
      
      callback( result );
    }else //if( !wt_app_id.empty() )
    {
      Wt::log("error") << "Error: got non empty Wt session ID ('" << wt_app_id << "'), but there is no"
                       << " WServer instance - not calling result callback!";
    }
  }//if( callback )
}//void do_portal_analysis( Analysis::AnalysisInput input )



void do_analysis()
{
  do
  {
    vector<Analysis::AnalysisInput> ana_to_do;
    
    {
      std::unique_lock<std::mutex> queue_lock( g_ana_queue_mutex );
      
      if( !g_keep_analyzing && g_simple_ana_queue.empty() )
      {
        Wt::log("info") << "Will stop analyzing";
        break;
      }
      
      Wt::log("info") << "Will wait for next analysis";
      g_ana_queue_cv.wait( queue_lock );
    
      Wt::log("info") << "Received notification to do analysis";
      
      ana_to_do.insert( end(ana_to_do), begin(g_simple_ana_queue), end(g_simple_ana_queue) );
      g_simple_ana_queue.clear();
    }
    
    Wt::log("info") << "Will do " << ana_to_do.size() << " analysis's.";
    
    for( const Analysis::AnalysisInput &input : ana_to_do )
    {
      switch( input.analysis_type )
      {
        case Analysis::AnalysisType::Simple:
          do_simple_analysis( input );
          break;
        
        case Analysis::AnalysisType::Search:
          do_search_analysis( input );
          break;
          
        case Analysis::AnalysisType::Portal:
          do_portal_analysis( input );
          break;
      }//switch( input.analysis_type )
    }//for( const Analysis::AnalysisInput &input : ana_to_do )
    
    {
      //cout << "Will check if we should keep analyzing..." << endl;
      std::lock_guard<std::mutex> queue_lock( g_ana_queue_mutex );
      if( !g_keep_analyzing && g_simple_ana_queue.empty() )
      {
        Wt::log("info") << "Will stop analyzing";
        break;
      }
      //cout << "...will keep analyzing" << endl;
    }
  }while( 1 );
  
  g_ana_queue_cv.notify_all();
  
  Wt::log("info") << "Have finished in do_analysis() - closing analysis thread.";
}//void do_analysis()

}//namespace


namespace Analysis
{

void set_gadras_app_dir( const std::string &dir )
{
  std::lock_guard<std::mutex> ana_lock( g_gad_mutex );
  
  if( !g_gad_drf.empty() || (g_gad_nchannel != -1) )
    throw runtime_error( "set_gadras_app_dir must be called before any GADRAS routines are called." );
  
  if( !SpecUtils::is_directory(dir) )
    throw runtime_error( "set_gadras_app_dir: invalid directory ('" + dir + "')." );
    
  g_gad_app_folder = dir;
}//void set_gadras_app_dir( const std::string &dir )

#if( !STATICALLY_LINK_TO_GADRAS )
bool load_gadras_lib( const std::string lib_name )
{
  assert( !g_gadras_dll_handle );
  
#if( defined(_WIN32) )
  g_gadras_dll_handle = LoadLibrary( lib_name.c_str() );
#else
  g_gadras_dll_handle = dlopen( lib_name.c_str(), RTLD_LAZY );
#endif
  
  if( !g_gadras_dll_handle )
  {
    string error_msg;
#if( defined(_WIN32) )
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();
    FormatMessage(
                  FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  dw,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &lpMsgBuf,
                  0, NULL );
    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
                                      (lstrlen((LPCTSTR)lpMsgBuf) + 40) * sizeof(TCHAR));
    
    StringCchPrintf((LPTSTR)lpDisplayBuf,
                    LocalSize(lpDisplayBuf) / sizeof(TCHAR),
                    TEXT("Failed with error %d: %s"),
                    dw, lpMsgBuf);
    
    error_msg = (LPCTSTR)lpDisplayBuf;
    
    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
#else
    const char *msg = dlerror();
    if( msg )
      error_msg = msg;
    else
      error_msg = "no error msg available.";
#endif
    
    Wt::log("error") << "Failed to load the dynamic library '" << lib_name << "', reason: "
                     << error_msg;
    return false;
  }

#if( defined(_WIN32) )
  g_gadrasversionnumber = (fctn_gadrasversionnumber_t)GetProcAddress(g_gadras_dll_handle, "gadrasversionnumber");
#else
  g_gadrasversionnumber = (fctn_gadrasversionnumber_t)dlsym(g_gadras_dll_handle, "gadrasversionnumber");
#endif
  if( !g_gadrasversionnumber )
  {
    Wt::log("error") << "could not locate the function 'gadrasversionnumber'";
    return false;
  }
  
#if( defined(_WIN32) )
  g_InitializeIsotopeIdCalibrated = (fctn_InitializeIsotopeIdCalibrated_t)GetProcAddress(g_gadras_dll_handle, "InitializeIsotopeIdCalibrated");
#else
  g_InitializeIsotopeIdCalibrated = (fctn_InitializeIsotopeIdCalibrated_t)dlsym(g_gadras_dll_handle, "InitializeIsotopeIdCalibrated");
#endif
  if( !g_InitializeIsotopeIdCalibrated )
  {
    Wt::log("error") << "could not locate the function 'InitializeIsotopeIdCalibrated'";
    return false;
  }
  
#if( defined(_WIN32) )
  g_InitializeIsotopeIdRaw = (fctn_InitializeIsotopeIdRaw_t)GetProcAddress(g_gadras_dll_handle, "InitializeIsotopeIdRaw");
#else
  g_InitializeIsotopeIdRaw = (fctn_InitializeIsotopeIdRaw_t)dlsym(g_gadras_dll_handle, "InitializeIsotopeIdRaw");
#endif
  if( !g_InitializeIsotopeIdRaw )
  {
    Wt::log("error") << "could not locate the function 'InitializeIsotopeIdCalibrated'";
    return false;
  }
  
#if( defined(_WIN32) )
  g_StaticIsotopeID = (fctn_StaticIsotopeID_t)GetProcAddress(g_gadras_dll_handle, "StaticIsotopeID");
#else
  g_StaticIsotopeID = (fctn_StaticIsotopeID_t)dlsym(g_gadras_dll_handle, "StaticIsotopeID");
#endif
  if( !g_StaticIsotopeID )
  {
    Wt::log("error") << "could not locate the function 'StaticIsotopeID'";
    return false;
  }

  
#if( defined(_WIN32) )
  g_SearchIsotopeID = (fctn_SearchIsotopeID_t)GetProcAddress(g_gadras_dll_handle, "SearchIsotopeID");
#else
  g_SearchIsotopeID = (fctn_SearchIsotopeID_t)dlsym(g_gadras_dll_handle, "SearchIsotopeID");
#endif
  if( !g_StaticIsotopeID )
  {
    Wt::log("error") << "could not locate the function 'SearchIsotopeID'";
    return false;
  }

#if( defined(_WIN32) )
  g_StreamingSearch = (fctn_StreamingSearch_t)GetProcAddress(g_gadras_dll_handle, "StreamingSearch");
#else
  g_StreamingSearch = (fctn_StreamingSearch_t)dlsym(g_gadras_dll_handle, "StreamingSearch");
#endif
  if( !g_StreamingSearch )
  {
    Wt::log("error") << "could not locate the function 'StreamingSearch'";
    return false;
  }
  
#if( defined(_WIN32) )
  g_GetCurrentIsotopeIDResults = (fctn_GetCurrentIsotopeIDResults_t)GetProcAddress(g_gadras_dll_handle, "GetCurrentIsotopeIDResults");
#else
  g_GetCurrentIsotopeIDResults = (fctn_GetCurrentIsotopeIDResults_t)dlsym(g_gadras_dll_handle, "GetCurrentIsotopeIDResults");
#endif
  if( !g_GetCurrentIsotopeIDResults )
  {
    Wt::log("error") << "could not locate the function 'GetCurrentIsotopeIDResults'";
    return false;
  }

#if( defined(_WIN32) )
  g_ClearIsotopeIDResults = (fctn_ClearIsotopeIDResults_t)GetProcAddress(g_gadras_dll_handle, "ClearIsotopeIDResults");
#else
  g_ClearIsotopeIDResults = (fctn_ClearIsotopeIDResults_t)dlsym(g_gadras_dll_handle, "ClearIsotopeIDResults");
#endif
  if( !g_ClearIsotopeIDResults )
  {
    Wt::log("error") << "could not locate the function 'ClearIsotopeIDResults'";
    return false;
  }
  
  
  
#if( defined(_WIN32) )
  g_RebinUsingK40 = (fctn_RebinUsingK40_t)GetProcAddress(g_gadras_dll_handle, "RebinUsingK40");
#else
  g_RebinUsingK40 = (fctn_RebinUsingK40_t)dlsym(g_gadras_dll_handle, "RebinUsingK40");
#endif
  if( !g_RebinUsingK40 )
  {
    Wt::log("error") << "could not locate the function 'RebinUsingK40'";
    return false;
  }
  
  
#if( defined(_WIN32) )
  g_PortalIsotopeIDCInterface = (fctn_PortalIsotopeIDCInterface_t)GetProcAddress(g_gadras_dll_handle, "PortalIsotopeIDCInterface");
#else
  g_PortalIsotopeIDCInterface = (fctn_PortalIsotopeIDCInterface_t)dlsym(g_gadras_dll_handle, "PortalIsotopeIDCInterface");
#endif
  if( !g_PortalIsotopeIDCInterface )
  {
    Wt::log("error") << "could not locate the function 'PortalIsotopeIDCInterface'";
    return false;
  }

  
  Wt::log("info") << "Loaded '" << lib_name << "'";
  
  return true;
}//bool load_gadras_lib( const std::String lib_name )
#endif  //#if( !STATICALLY_LINK_TO_GADRAS )


std::vector<std::string> available_drfs()
{
  const string drfs_path = SpecUtils::append_path(g_gad_app_folder, "drfs");
  const vector<string> drfs = SpecUtils::recursive_ls( drfs_path, "Detector.dat" );
  
  vector<string> answer;
  for( auto path : drfs )
  {
    string drf_path = SpecUtils::parent_path(path);
    string drf_name = SpecUtils::fs_relative( drfs_path, drf_path );
    
    // Make sure "DB.pcf" also exists (do we need "Response.win"?)
    const string dbfilename = SpecUtils::append_path(drf_path, "DB.pcf");
    const bool has_db = SpecUtils::is_file( dbfilename );

    if( has_db )
      answer.push_back( drf_name );
  }//for( auto path : drfs )
  
  std::sort( begin(answer), end(answer) );
  
  return answer;
}//std::vector<std::string> available_drfs()


string get_drf_name( const shared_ptr<SpecUtils::SpecFile> &spec )
{
  using SpecUtils::contains;
  using SpecUtils::icontains;
  
  if( !spec )
    return "";
  
  const string model_str = spec->instrument_model();
  
  //Get the detector description of the first gamma Measurement
  auto get_det_desc = [=]() -> string {
    for( auto meas : spec->measurements() )
    {
      if( meas && (meas->num_gamma_channels() > 7) )
        return meas->detector_type();
    }//for( auto meas : spec->measurements() )
    
    return "";
  };//get_det_desc(...)
  
  const SpecUtils::DetectorType type = spec->detector_type();
  
  switch( type )
  {
    case SpecUtils::DetectorType::Exploranium:
    {
      //GR130 or GR135 v1 or v2 systems.
      // TODO: I am assuming v2==Plus system, but need to check this
      // TODO: break up DetectorType::Exploranium for the different models.
      const string detdesc = get_det_desc();
      const bool has_130 = (contains(detdesc, "130") || contains(model_str, "130"));
      const bool has_135 = (contains(detdesc, "135") || contains(model_str, "135"));
      const bool has_v2 = (contains(detdesc, "v2") || contains(model_str, "v2")
                           || icontains(detdesc, "plus") || icontains(model_str, "plus") );
      
      if( has_135 && has_v2 )
        return "GR135Plus";
      else if( has_135 )
        return "GR135";
      else if( has_130 )
        return "GR130";
      break;
    }//case SpecUtils::DetectorType::Exploranium:
      
    case SpecUtils::DetectorType::IdentiFinder:
      return "IdentiFINDER-N";
      
    case SpecUtils::DetectorType::IdentiFinderNG:
    case SpecUtils::DetectorType::IdentiFinderUnknown:
      return "IdentiFINDER-NG";
      
    case SpecUtils::DetectorType::IdentiFinderLaBr3:
      return "IdentiFINDER-LaBr3";
      
    case SpecUtils::DetectorType::IdentiFinderR500NaI:
      return "IdentiFINDER-R500-NaI";
      
    case SpecUtils::DetectorType::DetectiveEx:
      return "Detective-EX";
      
    case SpecUtils::DetectorType::DetectiveEx100:
      return "Detective-EX100";
      
    case SpecUtils::DetectorType::DetectiveEx200:
      return "Detective-EX200";
      
    case SpecUtils::DetectorType::DetectiveX:
      return "Detective-X";
      
    case SpecUtils::DetectorType::Falcon5000:
      return "Falcon 5000";
      
    case SpecUtils::DetectorType::MicroDetective:
      return "Detective-Micro";
        
    case SpecUtils::DetectorType::OrtecRadEagleNai:
      return "RadEagle";
      
    case SpecUtils::DetectorType::Sam945:
      return "SAM-945";
    
    case SpecUtils::DetectorType::RIIDEyeNaI:
      return "RIIDEyeX-GN1";
      
    case SpecUtils::DetectorType::RadSeekerNaI:
      return "RadSeeker-NaI";
      
    case SpecUtils::DetectorType::RadSeekerLaBr:
      return "Radseeker-LaBr3";
      
    case SpecUtils::DetectorType::MicroRaider:
      return "Raider";
      
    case SpecUtils::DetectorType::Interceptor:
      return "Interceptor";
      
    case SpecUtils::DetectorType::VerifinderNaI:
      return "Verifinder";
      
    //Detectors we dont have DRFs for
    case SpecUtils::DetectorType::IdentiFinderTungsten:
    case SpecUtils::DetectorType::IdentiFinderR500LaBr:
    case SpecUtils::DetectorType::RIIDEyeLaBr:
    case SpecUtils::DetectorType::Sam940LaBr3:
    case SpecUtils::DetectorType::Sam940:
    case SpecUtils::DetectorType::OrtecRadEagleCeBr2Inch:
    case SpecUtils::DetectorType::OrtecRadEagleCeBr3Inch:
    case SpecUtils::DetectorType::OrtecRadEagleLaBr:
    case SpecUtils::DetectorType::RadHunterNaI:
    case SpecUtils::DetectorType::RadHunterLaBr3:
    case SpecUtils::DetectorType::Srpm210:
    case SpecUtils::DetectorType::DetectiveUnknown:
    case SpecUtils::DetectorType::SAIC8:
    case SpecUtils::DetectorType::Rsi701:
    case SpecUtils::DetectorType::Rsi705:
    case SpecUtils::DetectorType::AvidRsi:
    case SpecUtils::DetectorType::VerifinderLaBr:
    case SpecUtils::DetectorType::Unknown:
      break;
  }//switch( spec->detector_type() )
  
  // TODO: Add this detector to SpecUtils::DetectorType
  if( ((model_str == "ARIS") || (model_str == "ASP LRIP"))
     && SpecUtils::icontains(spec->manufacturer(), "Thermo") )
    return "Thermo ARIS Portal";
  
  // \TODO: look for 3x3, 1x1, etc
  
  return "";
}//string get_drf_name( shared_ptr<SpecUtils::SpecFile> specfile )



int32_t gadras_version_number()
{
  return ::gadras_version_number();
}

std::string gadras_version_string()
{
  int32_t vrsn = ::gadras_version_number();
  
  return to_string( vrsn/10000 )
         + "." + to_string( (vrsn % 10000) / 100 )
         + "." + to_string( vrsn % 100 );
}


/** Result for a simple analysis of single foreground and background */
AnalysisOutput::AnalysisOutput()
:  gadras_intialization_error( -999 ),
   gadras_analysis_error( -999 ),
   stuff_of_interest( 0.0f ),
   rate_not_norm( 0.0f ),
   isotopes{},
   chi_sqr( -1.0f ),
   alarm_basis_duration( -1.0f ),
   isotope_names{},
   isotope_types{},
   isotope_count_rates{},
   isotope_confidences{},
   isotope_confidence_strs{}
{
}


Wt::Json::Object AnalysisOutput::toJson() const
{
  Wt::Json::Object resultjson;
  
  resultjson["analysisError"] = this->gadras_analysis_error;
  if( !this->error_message.empty() )
    resultjson["errorMessage"] = Wt::WString::fromUTF8( this->error_message );
  
  if( (this->gadras_intialization_error < 0) || (this->gadras_analysis_error < 0) )
  {
    resultjson["code"] = 6;
    
    if( this->gadras_intialization_error < 0 )
      resultjson["initializationError"] = this->gadras_intialization_error;
    
    return resultjson;
  }//if( error doing analysis )
  
  // A result code of 1
  resultjson["code"] = 0;
  
  if( !this->analysis_warnings.empty() )
  {
    Wt::Json::Array &warnings = resultjson["analysisWarnings"] = Wt::Json::Array();
    for( const string &s : this->analysis_warnings )
      warnings.emplace_back( Wt::WString::fromUTF8(s) );
  }//if( !this->analysis_warnings.empty() )
  
  resultjson["drf"] = Wt::WString::fromUTF8( this->drf_used );
  resultjson["stuffOfInterest"] = this->stuff_of_interest;
  //this->rate_not_norm; //float
  resultjson["isotopeString"] = Wt::WString::fromUTF8( this->isotopes );
  resultjson["chi2"] = this->chi_sqr;
  resultjson["alarmBasisDuration"] = this->alarm_basis_duration;
  
  Wt::Json::Array &isotopes = resultjson["isotopes"] = Wt::Json::Array();
  
  // All these isotope arrays should be the same size, but we'll be careful, just in case
  size_t num_isotopes = this->isotope_names.size();
  assert( num_isotopes == this->isotope_types.size() );
  assert( num_isotopes == this->isotope_count_rates.size() );
  assert( num_isotopes == this->isotope_confidences.size() );
  assert( num_isotopes == this->isotope_confidence_strs.size() );
  
  num_isotopes = std::min( num_isotopes, this->isotope_names.size() );
  num_isotopes = std::min( num_isotopes, this->isotope_types.size() );
  num_isotopes = std::min( num_isotopes, this->isotope_count_rates.size() );
  num_isotopes = std::min( num_isotopes, this->isotope_confidences.size() );
  num_isotopes = std::min( num_isotopes, this->isotope_confidence_strs.size() );
  
  for( size_t i = 0; i < num_isotopes; ++i )
  {
    const string &isotope_name = this->isotope_names[i];
    const string &isotope_type = this->isotope_types[i];
    const float isotope_count_rate = this->isotope_count_rates[i];
    const float isotope_confidence = this->isotope_confidences[i];
    const string &isotope_confidence_str = this->isotope_confidence_strs[i];
    
    isotopes.push_back( Wt::Json::Object() );
    Wt::Json::Object &iso = isotopes.back();
    
    iso["name"] = Wt::WString::fromUTF8( isotope_name );
    iso["type"] = Wt::WString::fromUTF8( isotope_type );
    iso["countRate"] = isotope_count_rate;
    iso["confidence"] = isotope_confidence;
    iso["confidenceStr"] = Wt::WString::fromUTF8( isotope_confidence_str );
  }//for( size_t i = 0; i < num_isotopes; ++i )
  
  return resultjson;
}//Wt::Json::Object AnalysisOutput::toJson() const


std::string AnalysisOutput::briefTxtSummary() const
{
  string answer;
  
  auto append_line = [&answer]( const string &line ){
    if( !answer.empty() )
      answer += "\n";
    answer += line;
  };
  
  if( !this->error_message.empty() )
    append_line( "Error: " + this->error_message );
  
  if( this->gadras_intialization_error < 0 )
    append_line( "\nGadras Initialization Error:" + std::to_string(this->gadras_intialization_error) );
  
  if( this->gadras_analysis_error < 0 )
    append_line( "\nAnalysis Error Code: " + std::to_string(this->gadras_analysis_error) );
  
  if( (this->gadras_intialization_error < 0) || (this->gadras_analysis_error < 0) )
    return answer;
  
  if( this->isotope_names.empty() )
    append_line( "No isotopes identified" );
  
  char buffer[1024] = { '\0' };
  if( !this->isotopes.empty() )
  {
    snprintf( buffer, sizeof(buffer), "%s, Chi2=%.3f", this->isotopes.c_str(), this->chi_sqr );
    append_line( buffer );
  }else
  {
    snprintf( buffer, sizeof(buffer), "Chi2=%.3f", this->chi_sqr );
    append_line( buffer );
  }
  
  return answer;
}//std::string AnalysisOutput::briefTxtSummary() const



std::string AnalysisOutput::fullTxtSummary() const
{
  string answer;
  
  auto append_line = [&answer]( const string &line ){
    if( !answer.empty() )
      answer += "\n";
    answer += line;
  };
 
  char buffer[512] = { '\0' };
  
  if( !this->error_message.empty() )
  {
    snprintf( buffer, sizeof(buffer), "%-12s: %s", "Error", this->error_message.c_str() );
    append_line( buffer );
  }
  
  if( this->gadras_intialization_error < 0 )
  {
    snprintf( buffer, sizeof(buffer), "%-12s: %d", "Init Error", this->gadras_intialization_error );
    append_line( buffer );
  }
  
  if( this->gadras_analysis_error < 0 )
  {
    snprintf( buffer, sizeof(buffer), "%-12s: %d", "Ana Error", this->gadras_analysis_error );
    append_line( buffer );
  }
  
  if( (this->gadras_intialization_error < 0) || (this->gadras_analysis_error < 0) )
    return answer;
  
  
  if( this->analysis_warnings.size() == 1 )
  {
    snprintf( buffer, sizeof(buffer), "%-12s: %s", "Warning", this->analysis_warnings.front().c_str() );
    append_line( buffer );
  }else if( !this->analysis_warnings.empty() )
  {
    append_line( "Warnings: " + this->isotopes );
    for( const string &s : this->analysis_warnings )
      append_line( "\t" + s );
  }//if( !this->analysis_warnings.empty() )
  
  
  snprintf( buffer, sizeof(buffer), "%-12s: %s", "Isotopes", this->isotopes.c_str() );
  append_line( buffer );
  
  snprintf( buffer, sizeof(buffer), "%-12s: %s", "DRF Used", this->drf_used.c_str() );
  append_line( buffer );
  
  snprintf( buffer, sizeof(buffer), "%-12s: %.3f", "SOI", this->stuff_of_interest );
  append_line( buffer );
  
  snprintf( buffer, sizeof(buffer), "%-12s: %.3fs", "Alarm Basis", this->alarm_basis_duration );
  append_line( buffer );
  
  snprintf( buffer, sizeof(buffer), "%-12s: %.3f", "Chi2", this->chi_sqr );
  append_line( buffer );
  
  
  if( this->isotope_names.empty() )
  {
    append_line( "No isotopes identified" );
  }else
  {
    append_line( "Isotopes:" );
    
    snprintf( buffer, sizeof(buffer), "\t%-10s%-12s%-12s%-10s",
              "Isotope", "Type", "Confidence", "Count Rate" );
    append_line( buffer );
    
    
    const size_t num_isotopes = this->isotope_names.size();
    
    for( size_t i = 0; i < num_isotopes; ++i )
    {
      const string &isotope_name = this->isotope_names[i];
      const string &isotope_type = this->isotope_types[i];
      const float isotope_count_rate = this->isotope_count_rates[i];
      const float isotope_confidence = this->isotope_confidences[i];
      const string &isotope_confidence_str = this->isotope_confidence_strs[i];
      
      snprintf( buffer, sizeof(buffer), "\t%-10s%-12s%-3.1f (%s)      %-12.3f",
               isotope_name.c_str(), isotope_type.c_str(), isotope_confidence,
               isotope_confidence_str.c_str(), isotope_count_rate );
      
      append_line( buffer );
    }//for( size_t i = 0; i < num_isotopes; ++i )
  }//if( this->isotope_names.empty() )
    
  
  
  return answer;
}//std::string AnalysisOutput::fullTxtSummary() const


void start_analysis_thread()
{
  {
    std::lock_guard<std::mutex> queue_lock( g_ana_queue_mutex );
    g_keep_analyzing = true;
  }
  
  Wt::log("info") << "Will start analysis thread";
  std::lock_guard<std::mutex> lock( g_analysis_thread_mutex );
  
  if( g_analysis_thread )
    throw runtime_error( "start_analysis_thread(): Analysis thread already running." );

  g_analysis_thread = make_unique<thread>( &do_analysis );
  
  Wt::log("info") << "Have started analysis thread";
}//void start_analysis_thread()


void stop_analysis_thread()
{
  Wt::log("info") << "Will stop analysis thread";
  
  std::lock_guard<std::mutex> lock( g_analysis_thread_mutex );
  
  if( !g_analysis_thread )
    throw runtime_error( "stop_analysis_thread(): No analysis thread running." );
  
  Wt::log("debug") << "Set to keep analyzing to false";
  
  {
    std::lock_guard<std::mutex> queue_lock( g_ana_queue_mutex );
    g_keep_analyzing = false;
    
    Wt::log("debug") << "Have set keep analyzing to false";
  }
  
  g_ana_queue_cv.notify_all();
  
  Wt::log("debug") << "Have notified analysis thread to stop; will wait to finish up";
  
  {
    std::unique_lock<std::mutex> queue_lock( g_ana_queue_mutex );
    g_ana_queue_cv.wait( queue_lock, [&]() { return g_simple_ana_queue.empty(); } );
  }
  
  Wt::log("info") << "Analysis thread has finished";
  
  g_analysis_thread->join();
  
  g_analysis_thread.reset();
}//void stop_analysis_thread()


void post_analysis( const AnalysisInput &input )
{
  Wt::log("info") << "Will post analysis for session " << input.wt_app_id;
  
  {//begin lock on g_ana_queue_mutex
    std::lock_guard<std::mutex> lk( g_ana_queue_mutex );
    
    if( !g_keep_analyzing )
      throw runtime_error( "post_analysis(): Analysis thread not currently running" );
    
    g_simple_ana_queue.push_back( input );
  }//end lock on g_ana_queue_mutex
  
  Wt::log("debug") << "Have posted analysis, and will notify";
  
  g_ana_queue_cv.notify_all();
  
  Wt::log("debug") << "Have notified analysis thread";
}//void post_analysis( const AnalysisInput &input )


size_t analysis_queue_length()
{
  std::lock_guard<std::mutex> lk( g_ana_queue_mutex );
  return g_simple_ana_queue.size();
}

}//namespace Analysis
