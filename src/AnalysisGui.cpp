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

#include <fstream>

#include <Wt/Utils.h>
#include <Wt/WText.h>
#include <Wt/WDate.h>
#include <Wt/WTime.h>
#include <Wt/WLabel.h>
#include <Wt/WServer.h>
#include <Wt/WLogger.h>
#include <Wt/WComboBox.h>
#include <Wt/WIOService.h>
#include <Wt/WWidgetItem.h>
#include <Wt/WFileUpload.h>
#include <Wt/WPushButton.h>
#include <Wt/WEnvironment.h>
#include <Wt/WApplication.h>
#include <Wt/WLocalDateTime.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WFileDropWidget.h>
#include <Wt/WContainerWidget.h>

//#include <boost/algorithm/hex.hpp>
//#include <boost/uuid/detail/md5.hpp>
#include <boost/filesystem.hpp>

#include "SpecUtils/SpecFile.h"
#include "SpecUtils/StringAlgo.h"
#include "SpecUtils/Filesystem.h"

#include "FullSpectrumId/Analysis.h"
#include "FullSpectrumId/AnalysisGui.h"
#include "FullSpectrumId/D3TimeChart.h"
#include "FullSpectrumId/SimpleDialog.h"
#include "FullSpectrumId/SampleSelect.h"
#include "FullSpectrumId/AnalysisFromFiles.h"
#include "FullSpectrumId/D3SpectrumDisplayDiv.h"


#define USE_PROGRESS_BAR 1
#if( USE_PROGRESS_BAR )
// I dont like the Wt progress bar; its server side, not the native HTML widget, and
//  hides the file upload, by default forever.
//  If the WFileUpload owns the upload (e.g., setProgressBar( make_unique<WProgressBar>()), then
//  I cant figure out how to unhide the upload widget (without a JS hack) when the upload
//  finishes.
//  If the progress bar is separate (owned by another widget) - the showing/hiding of upload and
//  progress bar doesnt happen at the same time, and it looks all janky, and I dont like it
//  However - leaving implementation in for the moment
#include <Wt/WProgressBar.h>
#endif

using namespace std;
using namespace Wt;

// Namespace for static functions only used in this file
namespace
{
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


/** Parses spectrum file from file system and returns result.  Will return nullptr on error. */
std::shared_ptr<SpecUtils::SpecFile> parseFile( Wt::WFileUpload *upload )
{
  if( !upload || upload->empty() )
    return nullptr;
  
  WString fname = upload->clientFileName();
  Wt::Utils::removeScript( fname );  //I dont think this is strictly necassary
  fname = Wt::Utils::htmlEncode( fname );
  
  const string filepath = upload->spoolFileName();
  const string username = fname.toUTF8();
  
  auto spec = AnalysisFromFiles::parse_file( filepath, username );
  
  // For the moment I'm not totally sure if we display a filename somewhere or something,, or if
  //  maybe we should just set it blank
  if( spec )
    spec->set_filename( username );
  
  return spec;
}//std::shared_ptr<SpecUtils::SpecFile> parseFile( const std::string &filepath )

}//namespace


AnalysisGui::AnalysisGui( const string &data_base_dir, const bool save_spec_files )
 : Wt::WContainerWidget(),
  m_foreUploadLabel( nullptr ),
  m_foregroundUpload( nullptr ),
  m_foreSelectForeSample( nullptr ),
  m_foreSelectBackSample( nullptr ),
  m_backUploadLabel( nullptr ),
  m_backgroundUploadStack( nullptr ),
  m_backgroundUploadHolder( nullptr ),
  m_synthBackgroundHolder( nullptr ),
  m_backSelectBackSample( nullptr ),
  m_backgroundUpload( nullptr ),
  m_drfSelectorLabel( nullptr ),
  m_drfSelector( nullptr ),
  m_drfWarning( nullptr ),
  m_instructions( nullptr ),
  m_parseError( nullptr ),
  m_result( nullptr ),
  m_analysisError( nullptr ),
  m_analysisWarning( nullptr ),
  m_chartHolder( nullptr ),
  m_ana_number( 0 ),
  m_chart( nullptr ),
  m_timeline( nullptr ),
  m_numUploadsTotal( 0 ),
  m_numUploadsParsed( 0 ),
  m_numBytesUploaded( 0 )
#if( ENABLE_SESSION_DETAIL_LOGGING )
  , m_startTime( WLocalDateTime::currentServerDateTime() )
  , m_data_base_dir( data_base_dir )
  , m_data_dir("")
  , m_save_spectrum_files( save_spec_files )
  , m_uploadedFileNumber( 0 )
#endif
{
#if( USE_MINIFIED_JS_CSS )
  wApp->useStyleSheet( "AnalysisGui.min.css" );
#else
  wApp->useStyleSheet( "AnalysisGui.css" );
#endif
  
  addStyleClass( "AnalysisGui" );
  
  setAttributeValue("role", "main");
  
#if( FOR_WEB_DEPLOYMENT )
  WContainerWidget *holder = this;
#else
  WContainerWidget *holder = addNew<WContainerWidget>();
  holder->setWidth( WLength(100,Wt::LengthUnit::Percentage) );
  setOverflow( Overflow::Hidden, Orientation::Horizontal );
  setOverflow( Overflow::Auto, Orientation::Vertical );
#endif
  
  auto txt = holder->addNew<WText>( WString::tr("ana-title") );
  txt->addStyleClass( "AppTitle" );
  txt->setInline( false );
  
  auto row = holder->addNew<WContainerWidget>();
  row->addStyleClass( "AppRow" );
  m_foreUploadLabel = row->addNew<WLabel>( WString::tr("foreground-label") );
  //m_foreUploadLabel = row->addNew<WText>();
  m_foreUploadLabel->addStyleClass( "FileUploadLabel" );
  m_foregroundUpload = row->addNew<WFileUpload>();
  m_foregroundUpload->addStyleClass( "FileUpload" );
  
  // WFileUpload is not a WFormWidget, so can not be set as a buddy.
  //    (e.g., m_foreUploadLabel->setBuddy( m_foregroundUpload ); )
  // We could do a hacky workaround, but actually the <input /> we need to get the ID of, and we
  //  dont easily have that here in the C++, so we'll skip for now
  //m_foreUploadLabel->setText( "<label for=\"" + m_foregroundUpload->id() + "\">" + WString::tr("foreground-label") + "</label>" );
  
  // The next two calls dont seem to help
  // m_foregroundUpload->setCanReceiveFocus( true );
  // m_foregroundUpload->setFirstFocus();
  //  And actually just not setting tabindex seems to work okay; if we set tabindex, then hitting
  //  enter the first time you highlight the upload element doesnt work..
  //m_foregroundUpload->setAttributeValue( "tabindex", "0" );
  // However, the below does seem to work okay
  //if( wApp->environment().javaScript() )
  //  m_foregroundUpload->doJavaScript( "$('#" + m_foregroundUpload->id() + " > input').attr(\"tabindex\",\"0\");" );
  
  m_foregroundUpload->setAttributeValue( "aria-labelledby", m_foreUploadLabel->id() );
  m_foregroundUpload->setAttributeValue( "aria-describedby", m_foreUploadLabel->id() );
  
  m_foregroundUpload->changed().connect( m_foregroundUpload, &WFileUpload::upload);
  m_foregroundUpload->uploaded().connect( [=](){ fileUploaded(SpecUploadType::Foreground); } );
  m_foregroundUpload->fileTooLarge().connect( [=]( const ::int64_t fileSize ){
    uploadToLarge( fileSize, SpecUploadType::Foreground );
  } );
  
  if( wApp->environment().javaScript() )
  {
#if( USE_PROGRESS_BAR )
    m_foregroundUpload->setProgressBar( make_unique<WProgressBar>() );
#endif
  }else
  {
    Wt::WPushButton *uploadButton = row->addNew<WPushButton>( "Upload" );
    uploadButton->clicked().connect( m_foregroundUpload, &Wt::WFileUpload::upload );
  }//if( wApp->environment().javaScript() )
  
  
  m_foreSelectForeSample = row->addNew<SampleSelect>(SpecUtils::SourceType::Foreground, "foreground");
  m_foreSelectForeSample->sampleChanged().connect( this, &AnalysisGui::sampleNumberToUseChanged );
  m_foreSelectForeSample->hide();
  
  m_foreSelectBackSample = row->addNew<SampleSelect>(SpecUtils::SourceType::Background, "background");
  m_foreSelectBackSample->sampleChanged().connect( this, &AnalysisGui::sampleNumberToUseChanged );
  m_foreSelectBackSample->hide();
  
  row = holder->addNew<WContainerWidget>();
  row->addStyleClass( "AppRow" );
  m_backUploadLabel = row->addNew<WLabel>( WString::tr("background-label") );
  m_backUploadLabel->addStyleClass( "FileUploadLabel" );
  
  m_backgroundUploadStack = row->addNew<WStackedWidget>();
  m_backgroundUploadStack->addStyleClass( "BackgroundStack" );
    
  auto uploadOther = m_backgroundUploadStack->addWidget( make_unique<WText>( WString::tr("upload-other-background") ) );
  uploadOther->setAttributeValue( "tabindex", "0" );
  uploadOther->clicked().connect( this, &AnalysisGui::showBackgroundUpload );
  uploadOther->enterPressed().connect( this, &AnalysisGui::showBackgroundUpload );

  
  // Note, we are creating the SampleSelect as a foreground, because we expect if users are
  //  uploading a background file that has multiple samples, with one maybe being marked background,
  //  the marked background is probably the system background, where the user probably took an
  //  explicit spectrum to use as background - hopefully that makes some sense.
  m_backSelectBackSample = row->addNew<SampleSelect>(SpecUtils::SourceType::Foreground, "background");
  m_backSelectBackSample->sampleChanged().connect( this, &AnalysisGui::sampleNumberToUseChanged );
  m_backSelectBackSample->hide();
  
  
  row = holder->addNew<WContainerWidget>();
  row->addStyleClass( "AppRow" );
  m_drfSelectorLabel = row->addNew<WLabel>( WString::tr("drf-to-use") );
  m_drfSelectorLabel->addStyleClass( "FileUploadLabel" );
  
  m_drfSelector = row->addNew<WComboBox>();
  m_drfSelector->addStyleClass( "DrfSelect" );
  m_drfSelector->addItem( WString::tr("unknown-drf") );
  for( const string &drf : Analysis::available_drfs() )
    m_drfSelector->addItem( drf );
  m_drfSelector->changed().connect( this, &AnalysisGui::drfSelectionChanged );
  
  m_drfWarning = holder->addNew<WText>();
  m_drfWarning->addStyleClass( "AppRow DrfWarning" );
  m_drfWarning->setInline( false );
  m_drfWarning->setHidden( true );
  
  m_parseError = holder->addNew<WText>();
  m_parseError->addStyleClass( "AppRow ParseError" );
  m_parseError->setAttributeValue( "role", "alert" );
  m_parseError->setInline( false );
  m_parseError->setHidden( true );
  
  m_instructions = holder->addNew<WText>( WString::tr("begin-instructions") );
  m_instructions->addStyleClass( "AppRow AnaInstructions" );
  m_instructions->setAttributeValue( "aria-live", "polite" );
  m_instructions->setInline( false );
  
  
  m_result = holder->addNew<WText>( "" );
  m_result->addStyleClass( "AppRow Result" );
  m_result->setAttributeValue( "aria-live", "polite" );
  m_result->setInline( false );
  m_result->hide();

  m_analysisError = holder->addNew<WText>( "" );
  m_analysisError->addStyleClass( "AppRow AnaError" );
  m_analysisError->setInline( false );
  m_analysisError->setAttributeValue( "aria-live", "polite" );
  m_analysisError->setAttributeValue( "role", "alert" );
  m_analysisError->hide();

  m_analysisWarning = holder->addNew<WText>( "" );
  m_analysisWarning->addStyleClass( "AppRow AnaWarnings" );
  m_analysisWarning->setInline( false );
  m_analysisWarning->setAttributeValue( "aria-live", "polite" );
  m_analysisWarning->setAttributeValue( "role", "alert" );
  m_analysisWarning->hide();
  
  m_chartHolder = holder->addNew<WContainerWidget>();
  m_chartHolder->addStyleClass( "ChartHolder" );
  
  m_backUploadLabel->setHidden( true );
  m_backgroundUploadStack->setHidden( true );
  m_drfSelectorLabel->setHidden( true );
  m_drfSelector->setHidden( true );
}//AnalysisGui


AnalysisGui::UserActionLogEntry::UserActionLogEntry( const std::string &tag, AnalysisGui *gui )
: stringstream(),
  m_tag(tag)
#if( ENABLE_SESSION_DETAIL_LOGGING )
  , m_dir( ((gui && gui->check_session_data_dir()) ? gui->m_data_dir : string()) )
#endif
{
  const auto currentTime = WLocalDateTime::currentServerDateTime();
  
  // We'll always log time, session id, and IP address, since output potentially goes to both
  //  the Wt log, and the user_action_log.xml log file.
  
  (*this) << "<" << m_tag << ">\n"
          << "\t<Time>" << currentTime.toString("yyyy-MM-ddTHH:mm:ss.zzz").toUTF8() << "</Time>\n"
          << "\t<WtSessionId>" << wApp->sessionId() << "</WtSessionId>\n"
          << "\t<UserIpAddress>" << wApp->environment().clientAddress() << "</UserIpAddress>\n";
}//UserActionLogEntry constructor


AnalysisGui::UserActionLogEntry::~UserActionLogEntry()
{
  (*this) << "</" << m_tag << ">\n";
  
  const string content = str();
  Wt::log("info:app") << "\n" << content;

#if( ENABLE_SESSION_DETAIL_LOGGING )
  if( m_dir.empty() )
    return;

  const string logname = SpecUtils::append_path(m_dir, "user_action_log.xml");
  ofstream dolog( logname.c_str(), ios::out | ios::binary | ios::app );
  if( dolog )
    dolog << content;
  else
    Wt::log("error:app") << "Failed to open '" << logname << "' for writing.";
#endif
}//~UserActionLogEntry()


#if( ENABLE_SESSION_DETAIL_LOGGING )
bool AnalysisGui::check_session_data_dir()
{
  if( m_data_base_dir.empty() )
    return false;
  
  if( !m_data_dir.empty() )
    return true;
  
  const string year = m_startTime.date().toString("yyyy").toUTF8();
  const string month = m_startTime.date().toString("MM").toUTF8();
  const string day = m_startTime.date().toString("dd").toUTF8();
  const string timestr = m_startTime.time().toString("HH_mm_ss").toUTF8();
  
  string final_dir = SpecUtils::append_path( m_data_base_dir, year );
  if( !SpecUtils::is_directory(final_dir) )
    SpecUtils::create_directory(final_dir);
  
  final_dir = SpecUtils::append_path( final_dir, month );
  if( !SpecUtils::is_directory(final_dir) )
    SpecUtils::create_directory(final_dir);
  
  final_dir = SpecUtils::append_path( final_dir, day );
  if( !SpecUtils::is_directory(final_dir) )
    SpecUtils::create_directory(final_dir);
  
  final_dir = SpecUtils::append_path( final_dir, timestr + "_" + wApp->sessionId() );
  if( !SpecUtils::is_directory(final_dir) )
    SpecUtils::create_directory(final_dir);
  
  if( !SpecUtils::is_directory(final_dir) )
  {
    m_data_base_dir = m_data_dir = "";
    m_save_spectrum_files = false;
    Wt::log("error:app") << "Could not create user data directory ('" << final_dir << "'); will"
    << " not log information in detail or store uploaded spectrum files";
  }else
  {
    m_data_dir = final_dir;
    Wt::log("info:app") << "Will log session information in directory: '" << m_data_dir << "'";
    if( m_save_spectrum_files )
      Wt::log("info:app") << "Will save user-uploaded files into directory: '" << m_data_dir << "'";
  }
  
  return !m_data_dir.empty();
}//bool check_session_data_dir()
#endif //ENABLE_SESSION_DETAIL_LOGGING


void AnalysisGui::initSpectrumChart()
{
  if( m_chart )
    return;
  
  const WEnvironment &env = wApp->environment();
  
  //We could use almost env.agentIsIElt(9), but there are a few features SpectrumChartD3.js that use
  //  features IE doesn't support at all, so we'll not put a chart for any IE.
  if( !env.javaScript() )
    return;
  
  // env.agentIsIE() will return true for Edge
  bool isIE = env.agentIsIE();
  
  // Edge will be ID'd as IE for some reason so check on that.
  // EdgeHTML: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.79 Safari/537.36 Edge/14.14393'
  // Edge Chrome: 'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.74 Safari/537.36 Edg/79.0.309.43'
  if( isIE )
    isIE = (!SpecUtils::icontains( env.userAgent(), "Chrome/")
            || (!SpecUtils::icontains( env.userAgent(), "Edge/")
                && !SpecUtils::icontains( env.userAgent(), "Edg/")));

  if( isIE )
    return;
  
  m_chart = m_chartHolder->addNew<D3SpectrumDisplayDiv>();
  m_chart->setAttributeValue( "aria-label", "Foreground and background spectra used for analysis" );
  
  m_chart->setMinimumSize( 250, 250 );
  m_chart->setHidden( true );
  
  
  // We will watch for resizes, client side, and then appropriately resize the chart
  // TODO: set the size only if visible, and then correspondingly set size when shown
  const string resizejs = "let resizefcn = function(){"
  "var w = $(" + m_chartHolder->jsRef() + ").innerWidth();"
  "var h = $(" + jsRef() + ").innerHeight();"
  "var chartWidth = Math.max( w, 250.0 );"
  "var chartHeight = Math.max( Math.min(0.4*chartWidth,0.9*h), 250.0 );"
  "$(" + m_chart->jsRef() + ").width(chartWidth);"
  "$(" + m_chart->jsRef() + ").height(chartHeight);"
  //"console.log( 'resizing to', chartWidth, chartHeight );"
  + "if(" + m_chart->jsRef() + ".chart){"
  + m_chart->jsRef() + ".chart.handleResize();"
  //"console.log( 'Called into handleResize' );"
  "}"
  //This next bit of JS is from D3SpectrumDisplayDiv::resetLegendPosition()
  "try{"
    "let w = d3.select('#" + m_chart->id() + " > svg > g').attr('width');"
    "let lw = d3.select('#" + m_chart->id() + " .legend')[0][0].getBoundingClientRect().width;"
    "let x = Math.max(0, w - lw - 15);"
    "d3.select('#" + m_chart->id() + " .legend').attr('transform','translate(' + x + ',15)');"
  "}catch(e){"
    "console.log( 'Error setting legend pos: ' + e );"
  "}"
  "};"
  ""
  "resizefcn();"
  "let resizeObserver = new ResizeObserver( resizefcn );"
  "resizeObserver.observe(" + jsRef() + ");"
  ;
  
  m_chart->doJavaScript( resizejs );
  
  
  m_chart->setForegroundSpectrumColor( WColor("#4C566A") );
  m_chart->setBackgroundSpectrumColor( WColor("#81A1C1") );
  m_chart->setSecondarySpectrumColor( WColor("#8FBCBB") );
  m_chart->setTextColor( WColor("#3B4252") );
  m_chart->setAxisLineColor( WColor("#3B4252") );
  m_chart->setChartMarginColor( WColor("#ECEFF4") );
  m_chart->setChartBackgroundColor( WColor("#ECEFF4") );
  m_chart->setXAxisTitle( "Energy (keV)" );
  m_chart->setYAxisTitle( "Counts" );
  m_chart->setCompactAxis( true );
}//void initSpectrumChart()


void AnalysisGui::initTimeChart()
{
  if( m_timeline || !m_chart )
    return;
  
  const WEnvironment &env = wApp->environment();
  
  //We could use almost env.agentIsIElt(9), but there are a few features SpectrumChartD3.js that use
  //  features IE doesn't support at all, so we'll not put a chart for any IE.
  if( !env.javaScript() )
    return;
  
  // env.agentIsIE() will return true for Edge
  bool isIE = env.agentIsIE();
  
  // Edge will be ID'd as IE for some reason so check on that.
  // EdgeHTML: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.79 Safari/537.36 Edge/14.14393'
  // Edge Chrome: 'User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.74 Safari/537.36 Edg/79.0.309.43'
  if( isIE )
    isIE = (!SpecUtils::icontains( env.userAgent(), "Chrome/")
            || (!SpecUtils::icontains( env.userAgent(), "Edge/")
                && !SpecUtils::icontains( env.userAgent(), "Edg/")));
  
  if( isIE )
    return;
  
  m_timeline = m_chartHolder->addNew<D3TimeChart>();
  m_timeline->addStyleClass( "TimeLineChart" );
  m_timeline->setMinimumSize( 250, 250 );
  m_timeline->setHidden( true );
  
  m_timeline->setAttributeValue( "aria-label", "Gross counts over time plot." );
  
  // We will watch for resizes, client side, and then appropriately resize the chart
  // TODO: set the size only if visible, and then correspondingly set size when shown
  const string resizejs = "let timeResizer = function(){"
    "var w = $(" + m_chartHolder->jsRef() + ").innerWidth();"
    "var h = $(" + jsRef() + ").innerHeight();"
    "var chartWidth = Math.max( w, 250.0 );"
    "var chartHeight = Math.max( Math.min(0.25*chartWidth,0.9*h), 250.0 );"
    "$(" + m_timeline->jsRef() + ").width(chartWidth);"
    "$(" + m_timeline->jsRef() + ").height(chartHeight);"
    //"console.log( 'resizing to', chartWidth, chartHeight );"
    + "if(" + m_timeline->jsRef() + ".chart)"
      + m_timeline->jsRef() + ".chart.handleResize();"
  "};" //timeResizer
  "timeResizer();"
  "let timeResizeObserver = new ResizeObserver( timeResizer );"
  "timeResizeObserver.observe(" + jsRef() + ");"
  ;
  
  m_timeline->doJavaScript( resizejs );
  
  
  m_timeline->setGammaLineColor( WColor("#4C566A") );
  m_timeline->setNeutronLineColor( WColor("#81A1C1") );
  //m_timeline->setSecondarySpectrumColor( WColor("#8FBCBB") );
  m_timeline->setTextColor( WColor("#3B4252") );
  m_timeline->setAxisLineColor( WColor("#3B4252") );
  m_timeline->setChartMarginColor( WColor("#ECEFF4") );
  m_timeline->setChartBackgroundColor( WColor("#ECEFF4") );
  m_timeline->setY1AxisTitle( "Gamma Counts" );
  m_timeline->setY2AxisTitle( "Neut. Counts" );
  m_timeline->setXAxisTitle( "Measurement Time (s)" );
  m_timeline->setCompactAxis( true );
}//void initTimeChart()


bool AnalysisGui::synthesizingBackground() const
{
  if( !m_synthBackgroundHolder
     || !m_backgroundUploadStack
     || !m_backgroundUploadHolder )
    return false;
  
  const int index = m_backgroundUploadStack->indexOf( m_synthBackgroundHolder );
  return (index == m_backgroundUploadStack->currentIndex());
}//bool synthesizingBackground()


void AnalysisGui::showBackgroundUpload()
{
  m_background.reset();
  
  if( m_backgroundUpload )
  {
    assert( m_backgroundUploadHolder );
  }
  
  if( m_backgroundUpload && m_backgroundUpload->empty() )
  {
    const int index = m_backgroundUploadStack->indexOf( m_backgroundUploadHolder );
    assert( index == 1 );
    m_backgroundUploadStack->setCurrentIndex( index );
    return;
  }
  
  if( !m_backgroundUploadHolder )
  {
    auto holder = make_unique<WContainerWidget>();
    m_backgroundUploadHolder = holder.get();
    m_backgroundUploadStack->insertWidget( 1, std::move(holder) );
  }
  
  assert( m_backgroundUploadHolder );
  
  m_backgroundUploadHolder->clear();

  m_backgroundUpload = m_backgroundUploadHolder->addNew<WFileUpload>();
  m_backgroundUpload->changed().connect( m_backgroundUpload, &WFileUpload::upload );
  m_backgroundUpload->uploaded().connect( [this](){ fileUploaded(SpecUploadType::Background); } );
  m_backgroundUpload->fileTooLarge().connect( [=]( const ::int64_t fileSize ){
    uploadToLarge( fileSize, SpecUploadType::Background );
  } );
  
  assert( m_backUploadLabel );
  //m_backgroundUpload->setAttributeValue( "tabindex", "0" );
  //if( wApp->environment().javaScript() )
  //  doJavaScript( "$('#o1e3f43 > input').attr(\"tabindex\",\"0\");" );
  
  m_backgroundUpload->setAttributeValue( "aria-labelledby", m_backUploadLabel->id() );
  m_backgroundUpload->setAttributeValue( "aria-describedby", m_backUploadLabel->id() );
  m_backgroundUpload->setAttributeValue( "aria-required", "true" );
  m_backgroundUpload->setFocus( true );
  
  if( wApp->environment().javaScript() )
  {
#if( USE_PROGRESS_BAR )
    m_backgroundUpload->setProgressBar( make_unique<WProgressBar>() );
#endif
  }else
  {
    Wt::WPushButton *uploadButton = m_backgroundUploadHolder->addNew<WPushButton>( "Upload" );
    uploadButton->clicked().connect( m_backgroundUpload, &Wt::WFileUpload::upload );
  }
  
  m_backgroundUploadStack->setCurrentIndex( 1 );
  
  Wt::WPushButton *synthBackground = m_backgroundUploadHolder->addNew<WPushButton>( "Synthesize Background" );
  synthBackground->clicked().connect( this, &AnalysisGui::showBackgroundBeingSynthesized );
}//void showBackgroundUpload()


void AnalysisGui::showBackgroundBeingSynthesized()
{
  UserActionLogEntry logentry( "UserSelectedBackgroundSynth", this );
  
  // Clear out the current background, if there is one.
  showBackgroundUpload();
  
  if( !m_synthBackgroundHolder )
  {
    m_synthBackgroundHolder = m_backgroundUploadStack->addNew<WContainerWidget>();
    WPushButton *btn = m_synthBackgroundHolder->addNew<WPushButton>( "Upload a background" );
    btn->clicked().connect( this, &AnalysisGui::showBackgroundUpload );
  }//if( !m_synthBackgroundHolder )
  
  const int index = m_backgroundUploadStack->indexOf( m_synthBackgroundHolder );
  assert( index == 2 );
  m_backgroundUploadStack->setCurrentIndex( index );
  
  checkInputState();
}//void showBackgroundBeingSynthesized()


void AnalysisGui::fileUploaded( const SpecUploadType type )
{
  const bool isForeground = (type == SpecUploadType::Foreground);
  const WString typeName = (isForeground ? WString::tr("Foreground") : WString::tr("Background"));
  Wt::WFileUpload *upload = (isForeground ? m_foregroundUpload : m_backgroundUpload);
  
  assert( upload );
  if( !upload )
  {
    Wt::log("error:app") << "Somehow failed to identify upload - how could background upload be null?";
    return;
  }
  
#if( USE_PROGRESS_BAR )
  //See comments in constructor about the WProgressBar code not currently being used
  // If we used a progress bar (for JS enabled clients), lets hide it, and show the upload again.
  WProgressBar *progressBar = upload->progressBar();
  if( progressBar )
  {
    // Hack to make the progress bar disappear, and the upload widget show again...
    upload->show();
    upload->setProgressBar( make_unique<WProgressBar>() );
    upload->doJavaScript( "$(" + upload->jsRef() + ").find('input').css('display', '');" );
  }//if( progressBar )
#endif
  
  m_numUploadsTotal += 1;
  
  shared_ptr<SpecUtils::SpecFile> &specfile = (isForeground ? m_foreground : m_background);
  specfile.reset();
  
  if( upload->empty() )
  {
    m_parseError->setHidden( false );
    m_parseError->setText( typeName + WString::fromUTF8(" file didnt upload") );
    
    checkInputState();
    return;
  }//if( upload->empty() )
  
  
  // Make sure we're not just being spammed with junk
  if( (m_numUploadsTotal > 10) && m_numUploadsParsed < (m_numUploadsTotal/2) )
  {
    UserActionLogEntry entry( "SessionTerminate", this );
    entry << "\t<Reason>ToManyInvalidUploads</Reason>\n";

    // TODO: figure out if we should quit better.
    wApp->quit("To many invalid spectrum files have been uploaded.");
  }//if( (m_numUploadsTotal > 12) && m_numUploadsParsed < (m_numUploadsTotal/2) )
  
  
  
  /// \TODO: hash the file contents and log this
  const string session_id = wApp->sessionId();
  const string spool_name = upload->spoolFileName();
  const WString client_name = upload->clientFileName();
  
  //const string md5_hash = file_content_md5_hash( spool_name );
  Wt::log("info:app") << "File (" << typeName.toUTF8() << ") uploaded for app session '"
  << session_id << "' to '" << spool_name << "' that has"
  << " a client file name of '" << client_name << "'";
  //" and hash md5(" << md5_hash << ")";
  
  const size_t upload_file_size = SpecUtils::file_size(spool_name);
  
  m_numBytesUploaded += upload_file_size;
  m_uploadedFileNumber += 1;
  

  if( !wApp->environment().javaScript()
      || ((upload_file_size < 512*1024)
           && !SpecUtils::iends_with(client_name.toUTF8(), ".csv")
           && !SpecUtils::iends_with(client_name.toUTF8(), ".txt")) )
  {
    fileUploadWorker( type, nullptr, wApp );
  }else
  {
    auto dialog = wApp->root()->addChild(Wt::cpp14::make_unique<SimpleDialog>("Parsing File", "May take a moment"));
    //WServer::instance()->ioService().post( boost::bind( &AnalysisGui::fileUploadWorker, this, type, dialog, wApp ) );
    auto app = WApplication::instance();
    WServer::instance()->ioService().schedule( std::chrono::milliseconds(10),
                                              [this,type,dialog,app](){
      fileUploadWorker( type, dialog, app );
    } );
  }
}//AnalysisGui::fileUploaded(...)
  

void AnalysisGui::fileUploadWorker( const SpecUploadType type, SimpleDialog *dialog, Wt::WApplication *app )
{
  WApplication::UpdateLock lock( app );
  
  if( !lock ) {
    Wt::log("error:app") << "Unable to get an UpdateLock on app";
    return;
  }
  
  const bool isForeground = (type == SpecUploadType::Foreground);
  const WString typeName = (isForeground ? WString::tr("Foreground") : WString::tr("Background"));
  Wt::WFileUpload *upload = (isForeground ? m_foregroundUpload : m_backgroundUpload);
  
  assert( upload );
  if( !upload )
  {
    Wt::log("error:app") << "Somehow failed to identify upload (2) - how could background upload be null?";
    return;
  }
  
  const string session_id = wApp->sessionId();
  const string spool_name = upload->spoolFileName();
  const WString client_name = upload->clientFileName();
  
  const size_t upload_file_size = SpecUtils::file_size(spool_name);
  
  shared_ptr<SpecUtils::SpecFile> &specfile = (isForeground ? m_foreground : m_background);
  specfile.reset();
  
  
  WString parseErrMsg;
  DoWorkOnDestruct cleanup( [this, dialog, &parseErrMsg](){
    
    if( dialog )
      dialog->accept();
    
    if( parseErrMsg.empty() != m_parseError->isHidden() )
      m_parseError->setHidden( parseErrMsg.empty() );
    
    if( parseErrMsg != m_parseError->text() )
      m_parseError->setText( parseErrMsg );
    
    checkInputState();
    
    wApp->triggerUpdate();
  } );//
  
  
  UserActionLogEntry logentry( "FileUpload", this );
  
  const bool valid_save_dir = check_session_data_dir();
  
#if( ENABLE_SESSION_DETAIL_LOGGING )
  const bool too_much_data = (m_numBytesUploaded > 10*1024*1024);
#endif
  
  if( valid_save_dir )
  {
    auto sanitize_filename = []( string name ) -> string {
      const std::string forbidden_chars =
      "<>:\"/\\|?*"          //file system reserved https://en.wikipedia.org/wiki/Filename#Reserved_characters_and_words
      //"\x00 through \x1F"  //non-printing characters DEL, NO-BREAK SPACE, SOFT HYPHEN
      "\x7F\xA0\xAD"         // non-printing characters DEL, NO-BREAK SPACE, SOFT HYPHEN
      "#[]@!$&'()+,;="       //URI reserved https://tools.ietf.org/html/rfc3986#section-2.2
      "{}^~`"                //URL unsafe characters https://www.ietf.org/rfc/rfc1738.txt
      ;
      
      for(  auto &c : name )
      {
        if( forbidden_chars.find(c) != string::npos )
          c = '-';
        
        //"\x00 through \x1F"
        // NOTE: I dont really understand signed vs unsigned chars, so hopefully this is reasonable
        if( static_cast<unsigned char>(c) < static_cast<unsigned char>('\x1F') )
          c = '-';
      }//for(  auto &c : name )
      
      SpecUtils::utf8_limit_str_size( name, 255 );
      
      return name;
    };//sanitize_filename lambda
    
    const string client_clean_name = sanitize_filename( client_name.toUTF8() );
    
    logentry << "\t<SessionUploadNumber>" << m_uploadedFileNumber << "</SessionUploadNumber>\n"
             << "\t<Type>" << (isForeground ? "Foreground" : "Background") << "</Type>\n"
             << "\t<UserFileName>" << client_clean_name << "</UserFileName>\n"
             << "\t<FileSize>" << upload_file_size << "</FileSize>\n";
    
#if( ENABLE_SESSION_DETAIL_LOGGING )
    // For now, we will just arbitrarily only save up to the first 10 MB users upload.  Past this
    //  point, its probably garbage anyway
    if( !m_save_spectrum_files )
    {
      logentry << "\t<ArchivedStatus>SpecFileSavingRunTimeDisabled</ArchivedStatus>\n";
    }else if( !valid_save_dir )
    {
      logentry << "\t<ArchivedStatus>SpecFileSaveDirectoryInvalid</ArchivedStatus>\n";
    }else if( too_much_data )
    {
      logentry << "\t<ArchivedStatus>SessionFileSizeLimitExceeded</ArchivedStatus>\n";
    }else
    {
      const string ext = SpecUtils::file_extension( client_clean_name );
      
      const string save_to_name = "user_upload_" + std::to_string(m_uploadedFileNumber)
                                  + (isForeground ? "_foreground" : "_background")
                                  + ((ext.size() < 6) ? ext : string());
      
      const string save_to_path = SpecUtils::append_path( m_data_dir, save_to_name );
      
      boost::system::error_code ec;
      boost::filesystem::copy( spool_name, save_to_path, ec );
      
      if( ec )
      {
        logentry << "\t<ArchivedStatus>UnableToCopyFile</ArchivedStatus>\n";
        Wt::log("error:app") << "Unable to copy user upload from '" << spool_name << "' to '"
        << save_to_path << "': " << ec.message();
      }else
      {
        logentry << "\t<ArchivedStatus>Success</ArchivedStatus>\n";
      }
      
      logentry << "\t<RawFileName>" << save_to_name << "</RawFileName>\n";
    }//if( dont save ) / else ( dont save another reason ) / else ( save )
#else
    logentry << "\t<ArchivedStatus>SpecFileSavingCompileTimeDisabled</ArchivedStatus>\n";
#endif //ENABLE_SESSION_DETAIL_LOGGING
  }//if( check_session_data_dir() )
  
  
  auto spec = parseFile( upload );
  

  if( spec )
  {
    const string uuid = Wt::Utils::htmlEncode( spec->uuid() );
    logentry << "\t<ParsedAsSpecFile>True</ParsedAsSpecFile>\n"
             << "\t<SpecFileUuid>" << uuid << "</SpecFileUuid>\n";
  
/*
 // For the moment we wont take up disk-space writing the parsed file out as well
#if( ENABLE_SESSION_DETAIL_LOGGING )
    if( valid_save_dir && !too_much_data )
    {
      const string save_to_name = "user_upload_" + std::to_string(m_uploadedFileNumber)
                                + (isForeground ? "_foreground_parsed.n42" : "_background_parsed.n42");
    
      const string save_to_path = SpecUtils::append_path( m_data_dir, save_to_name );
      ofstream output( save_to_path.c_str(), ios::out | ios::binary );
      if( output )
      {
        if( spec->write_2012_N42( output ) )
          logentry << "\t<ParsedAsN42FileName>" << save_to_name << "</ParsedAsN42FileName>\n";
        else
          logentry << "\t<ParsedAsN42FileName>FailedToWrite</ParsedAsN42FileName>\n";
      }else
      {
        logentry << "\t<ParsedAsN42FileName>FailedToOpenOutput</ParsedAsN42FileName>\n";
      }
    }//if( check_session_data_dir() )
#endif //#if( ENABLE_SESSION_DETAIL_LOGGING )
*/
  }else
  {
    logentry << "\t<ParsedAsSpecFile>False</ParsedAsSpecFile>\n";
  }//if( spec ) / else

  
  if( !spec )
  {
    parseErrMsg = typeName + WString::fromUTF8(" file couldn't be parsed as a spectrum file");
    logentry << "\t<ErrorMsg>Coulnt parse as spectrum file.</ErrorMsg>\n";

    return;
  }
  
  try
  {
    AnalysisFromFiles::filter_energy_cal_variants( spec );
  }catch( std::exception &e )
  {
    m_parseError->setText( typeName + WString::fromUTF8(": " + std::string(e.what())) );
    logentry << "\t<ErrorMsg>Failed filtering energy cal variants.</ErrorMsg>\n";

    return;
  }//try / catch
  
  
  
  const set<int> &samples = spec->sample_numbers();
  if( samples.empty() )
  {
    m_parseError->setText( typeName + WString::fromUTF8( " didn't contain any spectra" ) );
    logentry << "\t<ErrorMsg>File didnt contain any spectra.</ErrorMsg>\n";

    return;
  }
  
  const set<size_t> fore_nchannels = spec->gamma_channel_counts();
  const set<size_t> back_nchannels = m_background ? m_background->gamma_channel_counts() : set<size_t>();
  
  const size_t fore_max_nchannel = (fore_nchannels.empty() ? size_t(0) : (*fore_nchannels.rbegin()));
  const size_t back_max_nchannel = (back_nchannels.empty() ? size_t(0) : (*back_nchannels.rbegin()));
  
  if( fore_max_nchannel < 32 )
  {
    parseErrMsg = typeName + WString::fromUTF8( " didn't contain spectroscopic data" );
    logentry << "\t<ErrorMsg>File didnt contain spectroscopic data.</ErrorMsg>\n";

    return;
  }
  
  m_numUploadsParsed += 1;
  
  // If the user uploaded a new foreground that doesn't match detector type of background, clear
  //  out the background.
  if( isForeground && m_background )
  {
    if( (fore_max_nchannel != back_max_nchannel)
        || (!spec->instrument_id().empty() && !m_background->instrument_id().empty()
            && (spec->instrument_id() != m_background->instrument_id()) )
       )
    {
      m_background.reset();
      if( m_backgroundUpload )
      {
        m_backgroundUploadStack->setCurrentIndex( 0 );
        m_backgroundUploadStack->removeWidget(m_backgroundUpload);
        m_backgroundUpload = nullptr;
      }
    }
  }//if( isForeground && m_background )
  
  //If user uploaded a foreground, and either not DRF is selected, or the detector type is differnt
  //  than before, try to auto select the DRF
  if( isForeground )
  {
    const bool noDrfSelected = (m_drfSelector->currentIndex() == 0);
    const bool sameTypeAsPrev = (specfile && (spec->detector_type() == specfile->detector_type()));
    
    if( noDrfSelected || !sameTypeAsPrev )
    {
      // \TODO: currently relying on string comparison to selefct DRF - need to do somethings more robust
      const string wanted_det = Analysis::get_drf_name(spec);
      
      if( wanted_det.size() )
      {
        for( int i = 1; i < m_drfSelector->count(); ++i )
        {
          const string selector = m_drfSelector->itemText(i).toUTF8();
          if( selector == wanted_det )
          {
            m_drfSelector->setCurrentIndex( i );
            logentry << "\t<AutoSelectedDrf>" << selector << "</AutoSelectedDrf>\n";
            break;
          }
        }
      }//if( wanted_det.size() )
    }//if( noDrfSelected || !sameTypeAsPrev )
  }//if( isForeground )
  
  logentry << "\t<CurrentDrf>" << m_drfSelector->currentText().toUTF8() << "</CurrentDrf>\n";
  
  specfile = spec;
}//void fileUploaded( const SpecUploadType type )


void AnalysisGui::uploadToLarge( const int64_t fileSize, const SpecUploadType type )
{
  const bool isForeground = (type == SpecUploadType::Foreground);
  
  
#if( USE_PROGRESS_BAR )
  Wt::WFileUpload *upload = (isForeground ? m_foregroundUpload : m_backgroundUpload);
  
  assert( upload );
  if( upload )
  {
    // If we used a progress bar (for JS enabled clients), lets hide it, and show the upload again.
    WProgressBar *progressBar = upload->progressBar();
    if( progressBar )
    {
      // Hack to make the progress bar disappear, and the upload widget show again...
      upload->show();
      upload->setProgressBar( make_unique<WProgressBar>() );
      upload->doJavaScript( "$(" + upload->jsRef() + ").find('input').css('display', '');" );
    }//if( progressBar )
  }else
  {
    Wt::log("error:app") << "Somehow failed to identify upload ofr too large of upload - how upload be null?";
    return;
  }
#endif
  
  
  const WString typeName = (isForeground ? WString::tr("Foreground") : WString::tr("Background"));
  
  const int64_t maxSizeAllowed = WApplication::instance()->maximumRequestSize();
  const int64_t uploadKb = (fileSize + 511) / 1024;
  const int64_t maxKb = (maxSizeAllowed + 511) / 1024;
  
  WString msg("Uploaded {1} file size ({2} kb) is larger than max allowed ({3} kb)");
  m_parseError->setText( msg.arg(typeName).arg(uploadKb).arg(maxKb) );
  m_parseError->setHidden( false );
  m_analysisError->setHidden( true );
  m_analysisWarning->setHidden( true );
  m_result->setHidden( true );
  
  UserActionLogEntry logentry( "UploadToLarge", this );
  
  logentry << "\t<UploadSize>" << fileSize << "</UploadSize>\n"
           << "\t<MaxAllowed>" << maxSizeAllowed << "</MaxAllowed>\n";
}//void AnalysisGui::uploadToLarge( int64_t file_size )


void AnalysisGui::checkInputState()
{
  /** \TODO:
   - We should be much more intelligent about finding fore/back, etc; I think I have some notes somewhere about this
   - Check that fore/back times are reasonably similar, counts(fore) > counts(back), ... lots of more error conditions.
   */
  
  // To avoid animation jitter (I havent actually checked if this would be the case, but I think it
  //  would), or doing GUI work we dont need to, we'll setup a cleanup function/object.
  //  Note: we need to define the lambda, and all the variables it references before
  //    DoWorkOnDestruct so they are still valid by the time the DoWorkOnDestruct objects destructor
  //    gets called.
  WString drfWarnTxt, instTxt;
  bool hideSpectrumChart = true, hideTimeChart = true, hideDrf = true, hideDrfWarn = true, hideBack = true;
  bool hideForeSelectFore = true, hideForeSelectBack = true, hideBackSelectBack = true;
  
  auto setWidgetVisibleState = [this, &hideSpectrumChart, &hideTimeChart, &hideDrf, &hideDrfWarn,
                                &drfWarnTxt, &hideBack, &instTxt, &hideForeSelectFore,
                                &hideForeSelectBack, &hideBackSelectBack](){
    
    // Note: I dont think the checking if hidden state differs from the current GUI state does any
    //  real good since I think Wt does this anyway if canOptimizeUpdates() returns true and there
    //  is no animation - but whatever for now
    //WAnimation anim( Wt::AnimationEffect::Fade, TimingFunction::Linear, 500 );
    WAnimation anim;
    
    if( m_chart && (m_chart->isHidden() != hideSpectrumChart) )
    {
      m_chart->setHidden( hideSpectrumChart /*, anim */ );
      if( !hideSpectrumChart )
        m_chart->resetLegendPosition();
    }//if( m_chart->isHidden() != hideSpectrumChart )
    
    if( m_timeline && (m_timeline->isHidden() != hideTimeChart) )
    {
      m_timeline->setHidden( hideTimeChart /*, anim */ );
      if( hideTimeChart )
        m_timeline->setData( nullptr );
    }
    
    if( m_drfSelectorLabel->isHidden() != hideDrf )
      m_drfSelectorLabel->setHidden( hideDrf, anim );
    if( m_drfSelector->isHidden() != hideDrf )
      m_drfSelector->setHidden( hideDrf, anim );
    
    if( m_drfWarning->isHidden() != hideDrfWarn )
      m_drfWarning->setHidden( hideDrfWarn, anim );
    if( m_drfWarning->text() != drfWarnTxt )
      m_drfWarning->setText( drfWarnTxt );
    
    
    if( hideBack )
    {
      m_background.reset();
      if( m_backgroundUploadStack->currentIndex() != 0 )
        m_backgroundUploadStack->setCurrentIndex( 0 );
    }//if( hideBack )
    
    if( m_backUploadLabel->isHidden() != hideBack )
      m_backUploadLabel->setHidden( hideBack, anim );
    if( m_backgroundUploadStack->isHidden() != hideBack )
      m_backgroundUploadStack->setHidden( hideBack, anim );
    
    if( instTxt.empty() )
    {
      if( !m_instructions->text().empty() )
        m_instructions->setText("");
      if( !m_instructions->isHidden() )
        m_instructions->hide();
    }else
    {
      if( m_instructions->isHidden() )
        m_instructions->show();
      if( m_instructions->text() != instTxt )
        m_instructions->setText( instTxt );
    }//if( instTxt.empty() ) / else
    
    if( m_foreSelectForeSample && (m_foreSelectForeSample->isHidden() != hideForeSelectFore) )
      m_foreSelectForeSample->setHidden(hideForeSelectFore);
    if( m_foreSelectForeSample && hideForeSelectFore )
      m_foreSelectForeSample->setSpecFile( nullptr );
    
    if( m_foreSelectBackSample && (m_foreSelectBackSample->isHidden() != hideForeSelectBack) )
      m_foreSelectBackSample->setHidden(hideForeSelectBack);
    if( m_foreSelectBackSample && hideForeSelectBack )
      m_foreSelectBackSample->setSpecFile( nullptr );
    
    if( m_backSelectBackSample && (m_backSelectBackSample->isHidden() != hideBackSelectBack) )
      m_backSelectBackSample->setHidden(hideBackSelectBack);
    if( m_backSelectBackSample && hideBackSelectBack )
      m_backSelectBackSample->setSpecFile( nullptr );
  };//setWidgetVisibleState lamda
  
  DoWorkOnDestruct cleanup( setWidgetVisibleState );
  
  if( !m_result->text().empty() )
    m_result->setText("");
  
  if( !m_result->isHidden() )
    m_result->hide();
  
  if( !m_analysisError->text().empty() )
    m_analysisError->setText("");
  
  if( !m_analysisError->isHidden() )
    m_analysisError->hide();
  
  if( !m_analysisWarning->text().empty() )
    m_analysisWarning->setText("");
  
  if( !m_analysisWarning->isHidden() )
    m_analysisWarning->hide();
  
  
  if( !m_foreground )
  {
    instTxt = WString::tr("begin-instructions");
    return;
  }//if( !m_foreground )
  
  // Figure out which SpecUtils::Measurement we want for foreground and background
  // \TODO: This logic could be way improved!
  bool is_search_data = false, is_portal_data = false;
  set<shared_ptr<const SpecUtils::Measurement>> foreground, background, unknown;
  
  // First we will check foreground if we should treat it as search-mode or portal data, or consider
  //  using its "derived data"
  //
  //
  // Right now we will only use derived data from Verifinder detectors, since they will show
  //  up as searchmode data, but their derived data is what we would sum anyway
  const bool potentially_use_derived_data = AnalysisFromFiles::potentially_analyze_derived_data( m_foreground );
  
  if( !potentially_use_derived_data && m_foreground->passthrough() )
  {
    // We will assume its portal data if it has an explicitly marked background sample of at least
    //  30 seconds, and at least 3 foreground/unknown samples.
    set<int> foregroundSamples, backgroundSamples;
    for( const auto &m : m_foreground->measurements() )
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
    
    if( !backgroundSamples.empty() && (foregroundSamples.size() >= 3) )
    {
      is_portal_data = true;
      Wt::log("debug:app") << "Treating foreground file as RPM data";
    }else
    {
      is_search_data = true;
      Wt::log("debug:app") << "Treating foreground file as search data";
    }
  }//if( m_foreground->passthrough() )
  
  
  // If foreground is search-mode or portal data, we wont use the background, if its set.
  if( is_portal_data || is_search_data )
  {
    m_background.reset();
  }
  
  
  // Check if the user uploaded same file twice.
  // \TODO: actually just figure out if the user needs to upload another file, and if so have the UI
  //        adapt to this, so uploading the same file twice would become an error
  if( (!m_background || (m_foreground->uuid() == m_background->uuid()))
     && !synthesizingBackground() )
  {
    if( potentially_use_derived_data && !is_search_data && !is_portal_data )
    {
      // This check for derived data in the uploaded file is based on the Verifinder, and not tested
      //  with any other models.
      
      AnalysisFromFiles::get_derived_measurements( m_foreground, foreground, background );
      
      if( background.empty() && foreground.empty() )
      {
        try
        {
          if( m_foreground->passthrough() )
          {
            std::set<int> back_sample_nums, fore_sample_nums;
            for( const auto &m : m_foreground->measurements() )
            {
              if( !m || (m->num_gamma_channels() < 32) || m->derived_data_properties() )
                continue;
              
              switch( m->source_type() )
              {
                case SpecUtils::SourceType::Unknown:
                case SpecUtils::SourceType::Foreground:
                  fore_sample_nums.insert( m->sample_number() );
                  break;
                  
                case SpecUtils::SourceType::Background:
                  back_sample_nums.insert( m->sample_number() );
                  break;
                  
                case SpecUtils::SourceType::IntrinsicActivity:
                case SpecUtils::SourceType::Calibration:
                  break;
              }//switch( m->source_type() )
            }//for( const auto &m : m_foreground->measurements() )
            
            if( back_sample_nums.empty() )
              throw runtime_error( "Background samples couldnt be identified in search-mode data" );
            
            if( fore_sample_nums.empty() )
              throw runtime_error( "Foreground samples couldnt be identified in search-mode data" );
            
            const auto &detnames = m_foreground->detector_names();
            auto back_meas = m_foreground->sum_measurements(back_sample_nums, detnames, nullptr );
            auto fore_meas = m_foreground->sum_measurements(fore_sample_nums, detnames, nullptr );
            
            background.insert( back_meas );
            foreground.insert( fore_meas );
          }else
          {
            for( const auto &m : m_foreground->measurements() )
            {
              if( !m || (m->num_gamma_channels() < 32) || m->derived_data_properties() )
                continue;
              
              switch( m->source_type() )
              {
                case SpecUtils::SourceType::Unknown:
                case SpecUtils::SourceType::Foreground:
                  foreground.insert( m );
                  break;
                  
                case SpecUtils::SourceType::Background:
                  background.insert( m );
                  break;
                  
                case SpecUtils::SourceType::IntrinsicActivity:
                case SpecUtils::SourceType::Calibration:
                  break;
              }//switch( m->source_type() )
            }//for( const auto &m : m_foreground->measurements() )
            
            if( foreground.empty() )
              throw runtime_error( "No foreground in non-search-mode data" );
            
            if( background.empty() )
              throw runtime_error( "No background in non-search-mode data" );
            
            if( foreground.size() > 1 )
              throw runtime_error( "More than one foreground sample in non-search-mode data" );
            
            if( background.size() > 1 )
              throw runtime_error( "More than one background sample in non-search-mode data" );
          }//if( m_foreground->passthrough() ) / else
        }catch( std::exception &e )
        {
          foreground.clear();
          background.clear();
          Wt::log("debug:app") << "Couldnt use non-derived data: " << e.what();
        }//try / catch use non-derived data
      }//if( background.empty() && foreground.empty() )
    }//if( potentially_use_derived_data && !is_search_data && !is_portal_data )
    
    
    if( foreground.empty() && unknown.empty() )
    {
      assert( background.empty() );
    }
    
    if( background.empty() )
    {
      assert( synthesizingBackground() || (foreground.empty() && unknown.empty()) );
    }
    
    
    if( !is_search_data
        && !is_portal_data
        && ((foreground.empty() && unknown.empty())
            || (background.empty() && !synthesizingBackground())) ) //if( derived data )
    {
      //If we're here, we didnt have useable derived data, we weren't search-mode data, and we're
      //  not portal data
      
      for( const auto &m : m_foreground->measurements() )
      {
        if( !m || m->num_gamma_channels() < 32 )
          continue;
        
        switch( m->source_type() )
        {
          case SpecUtils::SourceType::IntrinsicActivity:
          case SpecUtils::SourceType::Calibration:
            break;
          case SpecUtils::SourceType::Background:
            background.insert( m );
            break;
          case SpecUtils::SourceType::Foreground:
            foreground.insert( m );
            break;
          case SpecUtils::SourceType::Unknown:
            unknown.insert( m );
            break;
        }//switch( m->source_type() )
      }//for( auto m : m_foreground->measurements() )
    }//if( foreground has derived and non-derived data )
  }else //if( !m_background || (m_foreground->uuid() == m_background->uuid()) )
  {
    // If we're here, the user has uploaded foreground spectrum file and either uploaded the
    //  background spectrum file, or selected to synthesize the background
    
    for( const auto &m : m_foreground->measurements() )
    {
      if( !m || m->num_gamma_channels() < 32 )
        continue;
      
      switch( m->source_type() )
      {
        case SpecUtils::SourceType::IntrinsicActivity:
        case SpecUtils::SourceType::Calibration:
        case SpecUtils::SourceType::Background:
          break;
        case SpecUtils::SourceType::Foreground:
          foreground.insert( m );
          break;
        case SpecUtils::SourceType::Unknown:
          unknown.insert( m );
          break;
      }//switch( m->source_type() )
    }//for( auto m : m_foreground->measurements() )
    
    // TODO: if background has derived data that we probably want to use, like the Verifinder, we
    //       should detect and use that here
    if( !synthesizingBackground() )
    {
      set<shared_ptr<const SpecUtils::Measurement>> bckgrnd_file_bckgrnd, bckgrnd_file_fore;
      for( const auto &m : m_background->measurements() )
      {
        if( !m || m->num_gamma_channels() < 32 )
          continue;
        
        switch( m->source_type() )
        {
          case SpecUtils::SourceType::IntrinsicActivity:
          case SpecUtils::SourceType::Calibration:
            break;
            
          case SpecUtils::SourceType::Background:
            bckgrnd_file_bckgrnd.insert( m );
            break;
            
          case SpecUtils::SourceType::Foreground:
            bckgrnd_file_fore.insert( m );
            break;
          case SpecUtils::SourceType::Unknown:
            if( bckgrnd_file_fore.empty() )
              bckgrnd_file_fore.insert( m );
            break;
        }//switch( m->source_type() )
      }//for( auto m : m_foreground->measurements() )
      
      if( !bckgrnd_file_fore.empty() )
        background = bckgrnd_file_fore;
      else
        background = bckgrnd_file_bckgrnd;
    }//if( !synthesizingBackground() )
  }//if( !m_background || (m_foreground->uuid() == m_background->uuid()) ) / else
  
  
  if( foreground.empty() )
    foreground = unknown;
  
  if( is_portal_data )
  {
    assert( !is_search_data );
    assert( foreground.empty() && unknown.empty() && background.empty() );
  }//if( is_portal_data )
  
  if( is_search_data )
  {
    assert( !is_portal_data );
    assert( foreground.empty() && unknown.empty() && background.empty() );
  }//if( is_search_data )
  
  
  if( !is_portal_data && !is_search_data && foreground.empty() )
  {
    instTxt = WString::tr( "no-foreground-upload-other" );
    return;
  }
  
  auto get_meas_for_sample = []( const int sample, shared_ptr<SpecUtils::SpecFile> spec ) -> shared_ptr<const SpecUtils::Measurement> {
    if( !spec )
      return nullptr;
    
    if( spec->detector_names().size() == 1 )
    {
      const auto mv = spec->sample_measurements( sample );
      if( mv.size() == 1 ) //should always be the case, but we'll just be safe
        return mv[0];
    }
    
    try
    {
      return spec->sum_measurements( {sample}, spec->detector_names(), nullptr );
    }catch( std::exception &e )
    {
      Wt::log("error:app") << "Caught exception summing selected data sample (0): " << e.what();
    }
    
    return nullptr;
  };//get_meas_for_sample lambda
  
  
  if( !is_portal_data && !is_search_data && (foreground.size() != 1) )
  {
    hideForeSelectFore = false;
    
    if( (!m_background || background.empty()) && !synthesizingBackground() )
      hideForeSelectBack = false;
    
    if( m_background && (background.size() > 1) )
      hideBackSelectBack = false;
    
    if( synthesizingBackground() )
    {
      assert( background.empty() );
      background.clear();
      hideBackSelectBack = true;
    }
    
    assert( m_foreSelectForeSample );
    if( m_foreSelectForeSample && !hideForeSelectFore )
      m_foreSelectForeSample->setSpecFile( m_foreground );
    
    assert( m_foreSelectBackSample );
    if( m_foreSelectBackSample && !hideForeSelectBack )
      m_foreSelectBackSample->setSpecFile( m_foreground );
    
    assert( m_backSelectBackSample );
    // m_backSelectBackSample may be null if the background upload hasnt been shown yet
    if( m_backSelectBackSample && !hideBackSelectBack )
      m_backSelectBackSample->setSpecFile( m_background );
    
    
    // Here is were we check for the sample numbers to use, then sum all the measurements for that
    //  sample number, and submit analysis
    shared_ptr<const SpecUtils::Measurement> selectedForeground, selectedBackground;
    
    assert( m_foreSelectForeSample );
    if( m_foreSelectForeSample ) //Should always evaluate to true, but we'll check JIC
    {
      try
      {
        const int foreSample = m_foreSelectForeSample->currentSample();
        selectedForeground = get_meas_for_sample( foreSample, m_foreground );
      }catch( std::exception &e )
      {
        Wt::log("error:app") << "Caught exception getting foreground sample number from foreground select: " << e.what();
      }
    }
    
    if( !selectedForeground )
    {
      instTxt = WString::tr("selected-foreground-error");
      return;
    }
    
    foreground.clear();
    foreground.insert( selectedForeground );
    
    if( !hideForeSelectBack )
    {
      assert( m_foreSelectBackSample );
      if( m_foreSelectBackSample ) //Should always evaluate to true, but we'll check JIC
      {
        try
        {
          const int backSample = m_foreSelectBackSample->currentSample();
          selectedBackground = get_meas_for_sample( backSample, m_foreground );
        }catch( std::exception &e )
        {
          Wt::log("error:app") << "Caught exception getting background sample number from foreground select: " << e.what();
        }
      }//
      
      background.clear();
      if( selectedBackground && (selectedBackground->num_gamma_channels() >= 32) )
      {
        background.insert( selectedBackground );
      }else
      {
        instTxt = WString::tr("selected-background-error");
        return;
      }
    }//if( !hideForeSelectBack )
  }//if( !is_portal_data && !is_search_data && (foreground.size() != 1) )
  
  hideBack = (is_portal_data || is_search_data);
  
  if( !is_portal_data && !is_search_data && background.empty() && !synthesizingBackground() )
  {
    showBackgroundUpload();
    
    if( !m_background )
      instTxt = WString::tr("upload-background");
    else
      instTxt = WString::tr("indeterminate-background");
    return;
  }
  
  if( !is_portal_data && !is_search_data && !synthesizingBackground() && (background.size() != 1) )
  {
    if( !m_background )
    {
      showBackgroundUpload();
      instTxt = WString::tr("non-unique-background").arg( background.size() );
      return;
    }else
    {
      hideBackSelectBack = false;
      
      assert( m_backSelectBackSample );
      if( m_backSelectBackSample )  //JIC, but should be true
        m_backSelectBackSample->setSpecFile( m_background );
      
      assert( m_backSelectBackSample );
      
      if( m_backSelectBackSample ) //should always be true, but check JIC for now
      {
        try
        {
          const int backSample = m_backSelectBackSample->currentSample();
          auto m = get_meas_for_sample( backSample, m_background );
          if( m && (m->num_gamma_channels() >= 32) )
          {
            background.clear();
            background.insert( m );
          }else
          {
            instTxt = WString::tr("selected-background-error");
            return;
          }
        }catch( std::exception &e )
        {
          Wt::log("error:app") << "Caught exception getting background sample number from background select: " << e.what();
        }
      }//if( m_backSelectBackSample )
    }//if( !m_background ) / else
  }//if( !is_portal_data && !is_search_data && (background.size() != 1) )
  
  if( !m_background && !synthesizingBackground() )
    m_backgroundUploadStack->setCurrentIndex( 0 );
  
  const bool isSimpleAna = (!is_portal_data && !is_search_data);
  
  if( isSimpleAna && !synthesizingBackground() )
  {
    assert( background.size() == 1 );
    assert( foreground.size() == 1 );
    
    const auto &origBackMeas = *begin(background);
    const auto &origForeMeas = *begin(foreground);
    
    const size_t nback_chan = origBackMeas->num_gamma_channels();
    const size_t nfore_chan = origForeMeas->num_gamma_channels();
    
    if( nback_chan != nfore_chan )
    {
      instTxt = WString::tr("num-channel-mismatch").arg( nfore_chan ).arg( nback_chan );
      return;
    }
    
    const double backLiveTime = origBackMeas->live_time();
    const double foreLiveTime = origForeMeas->live_time();
    
    if( foreLiveTime <= 0.01 )
    {
      instTxt = WString::tr("no-foreground-live-time");
      return;
    }
    
    if( backLiveTime <= 0.01 )
    {
      instTxt = WString::tr("no-background-live-time");
      return;
    }
  }//if( isSimpleAna )
  
  
  
  hideDrf = false;
  
  if( m_drfSelector->currentIndex() == 0 )
  {
    instTxt = WString::tr("select-drf");
    return;
  }
  
  // If we made it this far - we ARE going to post an analysis.
  m_foregroundUpload->disable();
  m_backgroundUploadStack->disable();
  m_drfSelector->disable();
  
  
  const string recomended_det = Analysis::get_drf_name( m_foreground );
  const string selected_drf = m_drfSelector->currentText().toUTF8();
  
  if( selected_drf != recomended_det )
  {
    if( !recomended_det.empty() )
    {
      // We have the DRF
      hideDrfWarn = false;
      drfWarnTxt = WString::tr("diff-drf-selected").arg( WString::fromUTF8(recomended_det) );
    }else if( m_foreground->detector_type() == SpecUtils::DetectorType::Unknown )
    {
      hideDrfWarn = false;
      drfWarnTxt = WString::tr("couldnt-determine-drf");
    }else
    {
      hideDrfWarn = false;
      const string &dettype = SpecUtils::detectorTypeToString( m_foreground->detector_type() );
      drfWarnTxt = WString::tr( "drf-not-available" ).arg( WString::fromUTF8(dettype) );
    }
  }//if( selected_drf != recomended_det )

  
  m_ana_number += 1;
  
  Analysis::AnalysisInput anainput;
  anainput.wt_app_id = wApp->sessionId();
  anainput.ana_number = m_ana_number;
  anainput.drf_folder = m_drfSelector->currentText().toUTF8();
  
  if( isSimpleAna )
  {
    auto anafore = make_shared<SpecUtils::Measurement>();
    *anafore = **begin(foreground);
    anafore->set_sample_number( 1 );
    anafore->set_source_type( SpecUtils::SourceType::Foreground );
    anafore->set_title( Wt::Utils::htmlEncode( WString::tr("Foreground").toUTF8() ) );
    
   
    shared_ptr<SpecUtils::Measurement> anaback;
    if( !synthesizingBackground() )
    {
      anaback = make_shared<SpecUtils::Measurement>();
      *anaback = **begin(background);
      anaback->set_sample_number( 0 );
      anaback->set_source_type( SpecUtils::SourceType::Background );
      anaback->set_title( Wt::Utils::htmlEncode( WString::tr("Background").toUTF8() ) );
    }
    
    instTxt = WString::tr( "analyzing-simple" );
    hideSpectrumChart = !anafore;
    
    if( !m_chart && anafore )
      initSpectrumChart();
    
    if( m_chart )
    {
      m_chart->setData( anafore );
      m_chart->setBackground( anaback );
    }//if( m_chart )
    
    
    //Check that the background spectrum is from around the same time as the foreground.
    if( anaback )
    {
      const boost::posix_time::ptime &fore_time = anafore->start_time();
      const boost::posix_time::ptime &back_time = anaback->start_time();
      if( !fore_time.is_special() && !back_time.is_special() )
      {
        const auto nSecDiff = abs( (fore_time - back_time).total_seconds() );
        if( nSecDiff > 48*3600 )
        {
          const int ndays = static_cast<int>( std::round( nSecDiff / (48*3600.0) ) );
          anainput.input_warnings.push_back( WString::tr("back-fore-n-days-apart").arg(ndays).toUTF8() );
        }else if( nSecDiff > 2*3600 )
        {
          const int nhours = static_cast<int>( std::round( nSecDiff / 3600.0 ) );
          anainput.input_warnings.push_back( WString::tr("back-fore-n-hours-apart").arg(nhours).toUTF8() );
        }
      }//if( foreground and background differ by time a lot )
      
      
      if( anaback->real_time() < 120 )
        anainput.input_warnings.push_back( WString::tr("recommend-min-background").toUTF8() );
      
      const double foreCps = anafore->gamma_count_sum() / anafore->live_time();
      const double backCps = anaback->gamma_count_sum() / anaback->live_time();
      const double cpsDiff = foreCps - backCps;
      if( cpsDiff <= 0.0 )
        anainput.input_warnings.push_back( WString::tr("background-cps-higher").toUTF8() );
      
      const double foreCpsUncert = std::sqrt( anafore->gamma_count_sum() ) / anafore->live_time();
      const double backCpsUncert = std::sqrt( anaback->gamma_count_sum() ) / anaback->live_time();
      const double cpsSigma = std::sqrt( foreCpsUncert*foreCpsUncert + backCpsUncert*backCpsUncert );
      
      // \TODO: this recommendation for longer foreground is pulled out of the air - need to come up with
      //  a recommendation traceable back to a report written by someone who actually knows something
      if( (anafore->live_time() < 30) || ((cpsDiff > 0.0) && ((cpsDiff/cpsSigma) < 5) && (anafore->real_time() < 120)) )
        anainput.input_warnings.push_back( WString::tr("recommend-longer-foreground").toUTF8() );
    }//if( anaback )
    
    
    auto inputspec = make_shared<SpecUtils::SpecFile>();
    if( anaback )
      inputspec->add_measurement( anaback, false );
    inputspec->add_measurement( anafore, true );
    
    anainput.analysis_type = Analysis::AnalysisType::Simple;
    anainput.input = inputspec;
  }else
  {
    assert( m_foreground );
    assert( is_portal_data || is_search_data );
    assert( is_portal_data != is_search_data );
    
    instTxt = WString::tr( is_portal_data ? "analyzing-portal" : "analyzing-search-mode" );
    
    set<int> foreground_samples, background_samples;
    for( const int sample : m_foreground->sample_numbers() )
    {
      bool is_back = false, is_fore = false, is_other = false, is_occ = false, is_not_occ = false;
      for( const auto &m : m_foreground->sample_measurements(sample) )
      {
        switch( m->source_type() )
        {
          case SpecUtils::SourceType::IntrinsicActivity:
          case SpecUtils::SourceType::Calibration:
            is_other = true;
            break;
            
          case SpecUtils::SourceType::Background:
            is_back = true;
            break;
            
          case SpecUtils::SourceType::Foreground:
          case SpecUtils::SourceType::Unknown:
            is_fore = true;
            break;
        }//switch( m->source_type() )
        
        switch( m->occupied() )
        {
          case SpecUtils::OccupancyStatus::NotOccupied:
            is_not_occ = true;
            break;
            
          case SpecUtils::OccupancyStatus::Occupied:
            is_occ = true;
            break;
            
          case SpecUtils::OccupancyStatus::Unknown:
            break;
        }//switch( m->occupied() )
      }//for( const auto &m : m_foreground->sample_measurements(sample) )
      
      
      // Check if we do know the occupancy state, even if not foreground/background state
      if( !is_back && !is_fore && (is_occ != is_not_occ) )
      {
        is_back = is_not_occ;
        is_fore = is_occ;
      }
    
      if( is_other || (is_back == is_fore) )
        continue;
      
      if( is_back )
        background_samples.insert( sample );
      
      if( is_fore )
        foreground_samples.insert( sample );
    }//for( const int sample_num : m_foreground->sample_numbers() )
    
    
    shared_ptr<SpecUtils::Measurement> anafore, anaback;
    
    try
    {
      const auto &detnames = m_foreground->detector_names();
      if( foreground_samples.size() )
        anafore = m_foreground->sum_measurements( foreground_samples, detnames, nullptr );
      
      if( foreground_samples.size() && background_samples.size() )
        anaback = m_foreground->sum_measurements( background_samples, detnames, nullptr );
    }catch( std::exception &e )
    {
      anafore.reset();
      anaback.reset();
      Wt::log("error:app") << "Caught exception summing search/portal data for display: " << e.what();
    }//try / catch.
    
    
    if( anafore )
      anafore->set_title( is_portal_data ? "Occupied Sum" : "Foreground" );
    
    if( anaback )
      anaback->set_title( "Background" );
    
    hideSpectrumChart = !anafore;
    
    if( !m_chart && anafore )
      initSpectrumChart();
    
    if( m_chart )
    {
      m_chart->setData( anafore );
      m_chart->setBackground( anaback );
    }//if( m_chart )
    
    if( foreground_samples.size() > 3 )
    {
      hideTimeChart = false;
      if( !m_timeline )
        initTimeChart();
      
      if( m_timeline )
        m_timeline->setData( m_foreground );
    }//if( we have a few samples )
    
    anainput.analysis_type = is_portal_data ? Analysis::AnalysisType::Portal : Analysis::AnalysisType::Search;
    anainput.input = make_shared<SpecUtils::SpecFile>( *m_foreground ); //
  }//if( isSimpleAna ) / else
  
  
#if( !ENABLE_SESSION_DETAIL_LOGGING )
  if( anainput.input && (anainput.input->sample_numbers().size() < 10) )
  {
#warning "Not actually saving user uploaded input goging to analysis - need to implement"
    //UserActionLogEntry logentry( "AnalysisPost", this );
    //blah blah blah, put all the spectrum info and stuff here . fix boost::filesystem::copy( spool_name, save_to_path, ec );
    //  Maybe make runtime config option if save original file, or else just spectrum to log, or else both.
  }//if( anainput.input && (anainput.input->sample_numbers().size() < 10) )
#endif
  
  anainput.callback = [this, anainput]( Analysis::AnalysisOutput result ){
    anaResultCallback( anainput, result );
  };
  
// TODO: could save the file we are sending to analysis as N42 file, but then we would want to make
//       sure its unique - i.e., if user changes DRF 50 times, we dont want 50 duplicate files.
//#if( ENABLE_SESSION_DETAIL_LOGGING )
//  if( valid_save_dir && !too_much_data )
//  {
//  ...
//  }
//#endif
  
  if( wApp->environment().javaScript() )
  {
    Analysis::post_analysis( anainput );
  }else
  {
    // If JS isnt supported, then we need to do the analysis now, and update the GUI state since
    //  WApplication::enableUpdates() has no effect.
    std::mutex ana_mutex;
    std::condition_variable ana_cv;
    Analysis::AnalysisOutput result;

    // Clear out the WApp ID so the callback wont be posted into this sessions main thread.
    anainput.wt_app_id = "";
    anainput.callback = [&ana_mutex,&ana_cv,&result]( Analysis::AnalysisOutput output ){
      {
        std::unique_lock<std::mutex> lock( ana_mutex );
        result = output;
      }
      ana_cv.notify_all();
    };
    
    {
      std::unique_lock<std::mutex> lock( ana_mutex );
      Analysis::post_analysis( anainput );
      ana_cv.wait( lock );
    }
    
    anaResultCallback( anainput, result );
    
    // For some reason "Analyzing..." is still showing, but doesnt want to seem to hide... oh well for now
    //m_instructions->setText( "" );
    //m_instructions->setHidden( true );
  }
}//void checkInputState()


void AnalysisGui::drfSelectionChanged()
{
  UserActionLogEntry logentry( "UserChangedDrf", this );
  logentry << "\t<SelectedDrf>" << m_drfSelector->currentText().toUTF8() << "</SelectedDrf>\n";
  
  checkInputState();
}//void drfSelectionChanged()


void AnalysisGui::sampleNumberToUseChanged()
{
  UserActionLogEntry logentry( "UserChangedSampleNumber", this );
  if( m_foreSelectForeSample && m_foreSelectForeSample->isVisible() )
    logentry << "\t<ForegroundSampleNum>" << m_foreSelectForeSample->currentSample() << "</ForegroundSampleNum>\n";
  
  if( m_foreSelectBackSample && m_foreSelectBackSample->isVisible() )
    logentry << "\t<BackgroundSampleNumFromForegroundFile>"
             << m_foreSelectBackSample->currentSample()
             << "</BackgroundSampleNumFromForegroundFile>\n";
  
  if( m_backSelectBackSample && m_backSelectBackSample->isVisible() )
    logentry << "\t<BackgroundSampleNumFromBackgroundFile>"
             << m_backSelectBackSample->currentSample()
             << "</BackgroundSampleNumFromBackgroundFile>\n";
  
  checkInputState();
}//void sampleNumberToUseChanged()


void AnalysisGui::anaResultCallback( const Analysis::AnalysisInput &input,
                                     const Analysis::AnalysisOutput &output )
{
  m_foregroundUpload->enable();
  m_backgroundUploadStack->enable();
  m_drfSelector->enable();
  
  m_instructions->setText( "" );
  m_instructions->setHidden( true );
  
  m_analysisError->setHidden( output.error_message.empty() );
  if( m_analysisError->text().toUTF8() != output.error_message )
    m_analysisError->setText( WString::fromUTF8(output.error_message) );
 
  UserActionLogEntry logentry( "AnalysisResult", this );
  
  logentry << "\t<AnalysisNumber>" << output.ana_number << "</AnalysisNumber>\n"
           << "\t<InitErrorCode>" << output.gadras_intialization_error << "</InitErrorCode>\n"
           << "\t<AnalysisErrorCode>" << output.gadras_analysis_error << "</AnalysisErrorCode>\n";
  
  if( !output.error_message.empty() )
    logentry << "\t<ErrorMsg>" << output.error_message << "</ErrorMsg>\n";
  
  for( const auto &msg : output.analysis_warnings )
    logentry << "\t<AnalysisWarning>" << msg << "</AnalysisWarning>\n";
  
  logentry << "\t<SOI>" << output.stuff_of_interest << "</SOI>\n";
  logentry << "\t<RateNotNorm>" << output.rate_not_norm << "</RateNotNorm>\n";
  logentry << "\t<Isotopes>" << output.isotopes << "</Isotopes>\n";
  logentry << "\t<Chi2>" << output.chi_sqr << "</Chi2>\n";
  
  for( size_t i = 0; i < output.isotope_names.size(); ++i )
  {
    // All these arrays should be same length, but we'll be extra careful since development is
    //  rapid right now.
    logentry << "\t<Isotope>\n";
    if( i < output.isotope_names.size() )
      logentry << "\t\t<Name>" << output.isotope_names[i] << "</Name>\n";
    if( i < output.isotope_types.size() )
      logentry << "\t\t<Type>" << output.isotope_types[i] << "</Type>\n";
    if( i < output.isotope_count_rates.size() )
      logentry << "\t\t<CountRate>" << output.isotope_count_rates[i] << "</CountRate>\n";
    if( i < output.isotope_confidences.size() )
      logentry << "\t\t<Confidence>" << output.isotope_confidences[i] << "</Confidence>\n";
    if( i < output.isotope_confidence_strs.size() )
      logentry << "\t\t<ConfidenceStr>" << output.isotope_confidence_strs[i] << "</ConfidenceStr>\n";
    logentry << "\t</Isotope>\n";
  }//for( size_t i = 0; i < output.isotope_names.size(); ++i )
  
  
  if( (output.gadras_intialization_error < 0)
     || (output.gadras_analysis_error < 0) )
  {
    m_result->setText( "" );
    m_result->setHidden( true );
    return;
  }
  
  
  string warningHtml;
  for( const string &warning : input.input_warnings )
    warningHtml += "<div>" + warning + "</div>";
  
  for( const string &warning : output.analysis_warnings )
    warningHtml += "<div>" + warning + "</div>";
  
  // Check if energy re-calibration happened, and if so update the chart, and also insert a warning.
  if( output.spec_file && (output.spec_file != input.input) )
  {
    warningHtml += "<div>Energy calibration was updated during analysis.</div>";
    if( m_chart )
    {
      shared_ptr<const SpecUtils::Measurement> anaback, anafore;
      
      if( output.spec_file->num_measurements() == 2 )
      {
        for( auto &m : output.spec_file->measurements() )
        {
          switch( m->source_type() )
          {
            case SpecUtils::SourceType::IntrinsicActivity:
            case SpecUtils::SourceType::Calibration:
            case SpecUtils::SourceType::Background:
              anaback = m;
              break;
            case SpecUtils::SourceType::Foreground:
            case SpecUtils::SourceType::Unknown:
              anafore = m;
              break;
          }//switch( m->source_type() )
        }
      }else
      {
        set<int> foregroundSamples, backgroundSamples;
        for( const auto &m : output.spec_file->measurements() )
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
        }//for( const auto &m : m_foreground->measurements() )
        
        try
        {
          anafore = output.spec_file->sum_measurements( foregroundSamples, output.spec_file->detector_names(), nullptr );
          anaback = output.spec_file->sum_measurements( backgroundSamples, output.spec_file->detector_names(), nullptr );
        }catch( std::exception &e )
        {
          Wt::log("error:app") << "Unexpected exception summing foreground/background measurements: " << e.what();
        }
      }//if( two measurements ) / else
      
      if( anaback && anafore )
      {
        // TODO: make so D3SpectrumDisplayDiv::setData and D3SpectrumDisplayDiv::setBackground take const pointers
        auto f = make_shared<SpecUtils::Measurement>( *anafore );
        auto b = make_shared<SpecUtils::Measurement>( *anaback );
        f->set_title( "Foreground" );
        b->set_title( "Background" );
        m_chart->setData( f );
        m_chart->setBackground( b );
      }
    }//if( we are displaying a chart )
  }//if( analysis updated energy calibration )
  
  m_analysisWarning->setHidden( warningHtml.empty() );
  m_analysisWarning->setText( WString::fromUTF8( warningHtml ) );
  
  
  //Wt::log("debug:app") << 
  
  WStringStream rslttxt;
  rslttxt << "<div>\n";
  rslttxt << "<div class=\"ResultLabel\">" << WString::tr("id-result-label").toUTF8() << ":</div>";
  
  cout << "output.chi_sqr=" << output.chi_sqr << endl;
  if( (output.chi_sqr > 0.0001f) && (output.isotope_names.size() > 0) )
  {
    char buffer[64] = { '\0' };
    snprintf( buffer, sizeof(buffer), "%.2f", output.chi_sqr );
    rslttxt << "<div class=\"ResultChi2\">&chi;<sup>2</sup>=" << std::string(buffer) << "</div>";
  }//if( output.chi_sqr > 0.0f )
  rslttxt << "</div>\n";
  
  rslttxt << "<table class=\"ResultTable\"><tbody>\n"
          << "\t<tr>"
          << "\t\t<th>" << WString::tr("Nuclide").toUTF8() << "</th>\n"
          << "\t\t<th>" << WString::tr("Confidence").toUTF8().c_str() << "</th>\n"
          << "\t\t<th>" << WString::tr("Category").toUTF8().c_str() << "</th>\n";
  
  if( input.analysis_type == Analysis::AnalysisType::Simple )
    rslttxt << "\t\t<th>" << WString::tr("CountRate").toUTF8().c_str() << "</th>\n";
  else
    rslttxt << "\t\t<th>" << WString::tr("MaxCountRate").toUTF8().c_str() << "</th>\n";
  
  rslttxt << "\t</tr>";
  
  if( output.isotope_names.empty() )
  {
    rslttxt << "\t<tr>"
            << "\t\t<td colspan=\"4\" style=\"text-align: center; vertical-align: middle;\">"
              << WString::tr("none-found").toUTF8() << "</td>\n"
            << "\t</tr>\n";
  }else
  {
    const size_t nres = output.isotope_names.size();
    assert( nres == output.isotope_confidences.size() );
    assert( nres == output.isotope_types.size() );
    assert( nres == output.isotope_count_rates.size() );
    assert( nres == output.isotope_confidence_strs.size() );
    
    for( size_t i = 0; i < nres; ++i )
    {
      const string &iso = output.isotope_names[i];
      const string &type = output.isotope_types[i];
      string conf = output.isotope_confidence_strs[i];
      const float count_rate = output.isotope_count_rates[i];
      
      if( conf == "H" )
        conf = "High";
      else if( conf == "F" )
        conf = "Fair";
      else if( conf == "L" )
        conf = "Low";
      else
        Wt::log("debug:app") << "Unknown confidence '" << conf << "' for nuclide " << iso;
      
      char buffer[64] = { '\0' };
      if( count_rate < std::numeric_limits<float>::epsilon() )  //FLT_EPSILON is usually 1.19209e-07
        snprintf( buffer, sizeof(buffer), "--" );
      else
        snprintf( buffer, sizeof(buffer), "%.4g", count_rate );
      
      rslttxt << "\t<tr>\n"
      <<"\t\t<td>" << Wt::Utils::htmlEncode(iso) << "</td>\n"
      <<"\t\t<td>" << Wt::Utils::htmlEncode(conf) << "</td>\n"
      <<"\t\t<td>" << Wt::Utils::htmlEncode(type) << "</td>\n"
      <<"\t\t<td>" << std::string(buffer) << "</td>\n"
      <<"\t</tr>\n";
    }
  }//if( output.isotope_names.empty() ) / else
  
  /*
  //isostr looks something like: "Cs137(H)", "Cs137(H)+Ba133(F)", "None", etc.
  vector<string> iso_results;
  SpecUtils::split( iso_results, output.isotopes, "+" );
  for( const string &isostr : iso_results )
  {
    const auto par_start_pos = isostr.find( '(' );
    const auto par_end_pos = isostr.find( ')', par_start_pos );
    //const auto par_end_pos = (par_start_pos == string::npos) ? string::npos : isostr.find( ')', par_start_pos );
    
    if( (par_start_pos == string::npos) || (par_end_pos == string::npos) )
    {
      rslttxt << "<tr><td>" << Wt::Utils::htmlEncode(isostr) << "</td><td></td></tr>";
    }else
    {
      assert( par_end_pos > par_start_pos );
      const string iso = isostr.substr( 0, par_start_pos );
      string conf = isostr.substr( par_start_pos + 1, par_end_pos - par_start_pos - 1 );
      if( conf == "H" )
        conf = "High";
      else if( conf == "F" )
        conf = "Fair";
      else if( conf == "L" )
        conf = "Low";
      else
        Wt::log("debug:app") << "Unknown confidence '" << conf << "' from isostr='" << isostr << "'";
        
      rslttxt << "<tr><td>" << Wt::Utils::htmlEncode(iso) << "</td><td>"
              << Wt::Utils::htmlEncode(conf) << "</td></tr>";
    }//if( we didnt fund the "(H)" part ) / else
  }//for( string isostr : iso_results )
  */
  
  
  rslttxt << "</tbody></table>";
  
  m_result->setText( WString::fromUTF8( rslttxt.str() ) );
  m_result->setHidden( false );
  
  
  //string msg = "Analysis results: ";
  //for( auto res : output.isotopes )
  //  msg += res + ", ";
  //
  //msg += ". SOI=" + to_string(output.stuff_of_interest);
  //msg += ". Rate not NORM=" + to_string(output.rate_not_norm);
  //
  //m_msgToUser->setText( msg );
  
  //output.stuff_of_interest
  //SOI < 1.0:       Diagnostic indicator that is proportional to the square root of the countrate
  //1.0 < SOI < 2.5: It is likely that "Stuff of interest" was detected
  //SOI >= 2.5:      It is highly likely that "Stuff of Interest" was detected
  
  
}//void anaResultCallback( const Analysis::AnalysisOutput &output )
