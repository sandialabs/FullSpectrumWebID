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

#include <Wt/Utils.h>
#include <Wt/WText.h>
#include <Wt/WLabel.h>
#include <Wt/WSpinBox.h>
#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>


#include "SpecUtils/SpecFile.h"
#include "SpecUtils/DateTime.h"
#include "SpecUtils/StringAlgo.h"
#include "SpecUtils/EnergyCalibration.h"

#include "FullSpectrumId/SampleSelect.h"

using namespace std;
using namespace Wt;

SampleSelect::SampleSelect( const SpecUtils::SourceType type, const std::string &type_desc )
  : Wt::WContainerWidget(),
    m_type( type ),
    m_spec( nullptr ),
    m_sampleChangedSignal(),
    m_sampleSelect( nullptr ),
    m_totalSamples( nullptr ),
    m_desc( nullptr )
{
#if( USE_MINIFIED_JS_CSS )
  wApp->useStyleSheet( "SampleSelect.min.css" );
#else
  wApp->useStyleSheet( "SampleSelect.css" );
#endif
  addStyleClass( "SampleSelect" );
  
  auto label = addNew<WLabel>( WString::tr("select-sample-for").arg(type_desc + ":") );
  label->addStyleClass( "SampleSelectLabel" );
  
  auto selectHolder = addNew<WContainerWidget>();
  
  m_sampleSelect = selectHolder->addNew<WSpinBox>();
  m_sampleSelect->setWrapAroundEnabled(true);
  m_sampleSelect->valueChanged().connect( this, &SampleSelect::userChangedValue );
  m_sampleSelect->enterPressed().connect( this, &SampleSelect::userChangedValue );
  m_sampleSelect->addStyleClass( "SampleSelectSpinBox" );
  
  m_totalSamples = selectHolder->addNew<WText>();
  m_totalSamples->addStyleClass( "SampleSelectNumSamples" );
  
  m_desc = addNew<WText>();
  m_desc->addStyleClass( "SampleSelectDesc" );
}//SampleSelect constructor
  

void SampleSelect::setSpecFile( std::shared_ptr<SpecUtils::SpecFile> spec )
{
  if( m_spec == spec )
    return;
  
  m_spec = spec;
  m_samples.clear();
  m_desc->setText( "&nbsp;" );
  m_totalSamples->setText( "&nbsp;" );
  if( !m_spec || m_spec->sample_numbers().empty() )
  {
    m_sampleSelect->setRange(0, 0);
    m_sampleSelect->setValue( 0 );
    return;
  }//if( !m_spec )
  
  double definiteRealTime = -9999.0f;
  vector<int> valid_samples, other_samples;
  int defaultSampleNumber = -9999999, lastSeenMeas = -9999999, definiteUseSample = -9999999;
  
  const set<int> &samplenums = m_spec->sample_numbers();
  const vector<string> &detnames = m_spec->detector_names();
  for( const int sample : samplenums )
  {
    bool is_spectroscopic_sample = false;
    bool is_non_cal_intrinsic = false;
    
    for( const string &detname : detnames )
    {
      auto m = m_spec->measurement( sample, detname );
      
      const uint32_t derived_prop = m ? m->derived_data_properties() : 0;
      using ddp = SpecUtils::Measurement::DerivedDataProperties;
      const bool is_derived = (derived_prop);
      const bool is_derived_ioi = (derived_prop & static_cast<uint32_t>(ddp::ItemOfInterestSum));
      const bool is_derived_backsub = (derived_prop & static_cast<uint32_t>(ddp::BackgroundSubtracted));
      const bool is_derived_bckgrnd = (derived_prop & static_cast<uint32_t>(ddp::IsBackground));
      const bool is_derived_processed = (derived_prop & static_cast<uint32_t>(ddp::ProcessedFurther));
      
      
      if( m
         && (m->num_gamma_channels() >= 32)
         && m->energy_calibration()
         && m->energy_calibration()->valid() )
      {
        is_spectroscopic_sample = true;
        
        switch( m_type )
        {
          case SpecUtils::SourceType::Background:
            if( (m->source_type() == SpecUtils::SourceType::Background)
                || SpecUtils::icontains( m->title(), "back" ) )
            {
              if( m->real_time() > definiteRealTime )
              {
                definiteUseSample = sample;
                definiteRealTime = m->real_time();
              }
              
              defaultSampleNumber = sample;
            }
            break;
          
          case SpecUtils::SourceType::Foreground:
          case SpecUtils::SourceType::Unknown:
            if( (m->source_type() == SpecUtils::SourceType::Foreground)
               || (!is_derived && (m->source_type() == SpecUtils::SourceType::Unknown))
               || SpecUtils::icontains( m->title(), "ioi" )
               || SpecUtils::icontains( m->title(), "inter" )
               || SpecUtils::icontains( m->title(), "primary" )
               || SpecUtils::icontains( m->title(), "fore" )
               || SpecUtils::icontains( m->title(), "item" )
               || SpecUtils::icontains( m->title(), "side" )
               || (is_derived && !is_derived_backsub && !is_derived_processed && !is_derived_bckgrnd)
               )
            {
              defaultSampleNumber = sample;
              
              if( m->real_time() > definiteRealTime )
              {
                definiteUseSample = sample;
                definiteRealTime = m->real_time();                
              }
            }
            break;
            
          case SpecUtils::SourceType::IntrinsicActivity:
          case SpecUtils::SourceType::Calibration:
            break;
        }//switch( m_type )
        
        switch( m->source_type() )
        {
          case SpecUtils::SourceType::IntrinsicActivity:
          case SpecUtils::SourceType::Calibration:
            break;
            
          case SpecUtils::SourceType::Background:
          case SpecUtils::SourceType::Foreground:
          case SpecUtils::SourceType::Unknown:
            lastSeenMeas = sample;
            is_non_cal_intrinsic = true;
            break;
        }//switch( m->source_type() )
      }//if( this is a valid measurement )
      
      // We could probably break a little early here and save some time, occasionally, but whatever
      //  for now.
      //if( (sample == lastSeenMeas) && is_non_cal_intrinsic && is_spectroscopic_sample )
      //  break;
    }//for( const string &detname : detnames )
    
    if( is_spectroscopic_sample )
    {
      if( is_non_cal_intrinsic )
        valid_samples.push_back( sample );
      else
        other_samples.push_back( sample );
    }//if( is_spectroscopic_sample )
  }//for( const int sample : m_spec->sample_numbers() )
  
  
  if( valid_samples.empty() )
    valid_samples = other_samples;  //We probably wont ever see this
  
  if( samplenums.count(definiteUseSample) )
    defaultSampleNumber = definiteUseSample;
  
  if( !valid_samples.empty() && !samplenums.count(defaultSampleNumber) )
  {
    switch( m_type )
    {
      case SpecUtils::SourceType::Background:
        defaultSampleNumber = valid_samples.front();
        break;
        
      case SpecUtils::SourceType::IntrinsicActivity:
      case SpecUtils::SourceType::Calibration:
      case SpecUtils::SourceType::Foreground:
      case SpecUtils::SourceType::Unknown:
        if( samplenums.count(lastSeenMeas) )
          defaultSampleNumber = lastSeenMeas;
        else
          defaultSampleNumber = valid_samples.back();
        break;
    }//switch( m_type )
  }//if( we dont have a default sample number )
  
  if( valid_samples.empty() )
  {
    m_sampleSelect->setRange(0, 0);
    m_sampleSelect->setValue( 0 );
    return;
  }//if( !m_spec )
  
  m_samples = valid_samples;
  
  m_sampleSelect->setRange( 1,  static_cast<int>(m_samples.size()) );
  const auto pos = std::find( begin(m_samples), end(m_samples), defaultSampleNumber );
  if( pos != end(m_samples) )
  {
    const auto index = pos - begin(m_samples);
    m_sampleSelect->setValue( 1 + index );
  }else
  {
    m_sampleSelect->setValue( 1 );
    cerr << "Unexpectedly couldnt find default sample number (" << defaultSampleNumber << ") in m_samples={";
    for( auto i : m_samples )
      cerr << i << ", ";
    cerr << "};" << endl;
  }
  
  m_totalSamples->setText( "&nbsp;of " + std::to_string(m_samples.size()) );
  
  updateDescription();
}//setSpecFile( std::shared_ptr<SpecUtils::SpecFile> spec )
  

void SampleSelect::userChangedValue()
{
  try
  {
    updateDescription();
    const int sample = currentSample();
    m_sampleChangedSignal.emit( sample );
  }catch( std::exception &e )
  {
    cerr << "Caught exception getting sample number after user changed it: " << e.what();
  }
}//void userChangedValue()


void SampleSelect::updateDescription()
{
  try
  {
    if( !m_spec )
      throw runtime_error( "No spectrum" );
      
    const int sample = currentSample();
    vector<shared_ptr<const SpecUtils::Measurement>> meass = m_spec->sample_measurements( sample );
    
    if( meass.empty() )
      throw runtime_error( "No meas for sample" );
    
    // Just grab the first title from the measurements
    string title;
    for( const auto &m : meass )
    {
      title = m->title();
      if( !title.empty() )
        break;
    }
    
    shared_ptr<const SpecUtils::Measurement> summed;
    if( meass.size() == 1 )
      summed = meass[0];
    else
      summed = m_spec->sum_measurements( {sample}, m_spec->detector_names(), nullptr );
    
    if( !summed )
      throw runtime_error( "Unexpected invalid summed measurement!" );
    
    char buffer[128] = { '\0' };
    
    string desc;
    
    if( !summed->start_time().is_special() )
    {
      if( !desc.empty() )
        desc += ", ";
      desc += SpecUtils::to_common_string( summed->start_time(), true );
    }
    
    //Get gamma cps, neutron cps, start time, real time, title
    if( !isinf(summed->gamma_count_sum())
       && !isnan(summed->gamma_count_sum())
       && !isinf(summed->live_time())
       && !isnan(summed->live_time())
       && (summed->live_time() > 0.00001f) )
    {
      if( !desc.empty() )
        desc += ", ";
      
      const double gamma_cps = summed->gamma_count_sum() / summed->live_time();
      snprintf( buffer, sizeof(buffer), "%.4g &gamma; cps", gamma_cps );
      desc += buffer;
    }
    
    if( summed->contained_neutron()
       && !isinf(summed->neutron_counts_sum())
       && !isnan(summed->neutron_counts_sum())
       && !isinf(summed->real_time())
       && !isnan(summed->real_time())
       && (summed->real_time() > 0.00001f) )
    {
      const double gamma_cps = summed->neutron_counts_sum() / summed->real_time();
      snprintf( buffer, sizeof(buffer), "%.4g n cps", gamma_cps );
      if( !desc.empty() )
        desc += ", ";
      desc += buffer;
    }
    
    
    if( !isinf(summed->real_time())
       && !isnan(summed->real_time())
       && (summed->real_time() > 0.00001f) )
    {
      snprintf( buffer, sizeof(buffer), "real time: %.1f s", summed->real_time() );
      if( !desc.empty() )
        desc += ", ";
      desc += buffer;
    }
    
    if( title.length() )
    {
      WString titlewstr = WString::fromUTF8(title);
      Wt::Utils::removeScript( titlewstr );
      titlewstr = Wt::Utils::htmlEncode( titlewstr );
      title = titlewstr.toUTF8();
      desc = "<div><em>Info:&nbsp;</em>" + desc + "</div>";
      desc += "<div><em>Title:</em> &quot;" + title + "&quot;</div>";
    }
    
    m_desc->setText( WString::fromUTF8(desc) );
  }catch( std::exception &e )
  {
    m_desc->setText( "" );
  }
}//void updateDescription()


int SampleSelect::currentSample()
{
  assert( !m_samples.empty() );
  
  if( m_samples.empty() )
    throw runtime_error( "SampleSelect::currentSample(): no measurement currently" );
  
  switch( m_sampleSelect->validate() )
  {
    case Wt::ValidationState::Invalid:
    case Wt::ValidationState::InvalidEmpty:
      throw runtime_error( "User entered value is invalid" );
    case Wt::ValidationState::Valid:
      break;
  }//switch( m_sampleSelect->validate() )
  
  const int value = m_sampleSelect->value() - 1;
  
  if( (value < 0) || (value >= static_cast<int>(m_samples.size())) )
    throw runtime_error( "Some how entered value is out of range" );
  
  return m_samples[value];
}
  

Wt::Signal<int> &SampleSelect::sampleChanged()
{
  return m_sampleChangedSignal;
}

