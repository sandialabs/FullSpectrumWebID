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

#include <Wt/WText.h>
#include <Wt/WAnchor.h>
#include <Wt/WLocale.h>
#include <Wt/WLogger.h>
#include <Wt/WDialog.h>
#include <Wt/WDateTime.h>
#include <Wt/WComboBox.h>
#include <Wt/WTemplate.h>
#include <Wt/WFileUpload.h>
#include <Wt/WGridLayout.h>
#include <Wt/WPushButton.h>
#include <Wt/WEnvironment.h>
#include <Wt/WApplication.h>
#include <Wt/WFileDropWidget.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WMessageResourceBundle.h>
#include <Wt/WOverlayLoadingIndicator.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

//#include <boost/algorithm/hex.hpp>
//#include <boost/uuid/detail/md5.hpp>

#include "SpecUtils/SpecFile.h"
#include "SpecUtils/Filesystem.h"
#include "SpecUtils/StringAlgo.h"

#include "FullSpectrumId/Analysis.h"
#include "FullSpectrumId/AnalysisGui.h"
#include "FullSpectrumId/FullSpectrumApp.h"

using namespace std;
using namespace Wt;

namespace
{
#if( ENABLE_SESSION_DETAIL_LOGGING )
// The base-directory to save user uploaded files to.
//  We will be heavy-handed and and use a mutex and copying - this is a rarely accessed, with
//   probably zero contention variable, so we could probably get away with no locking and references
//   but whatever for now
std::mutex sm_data_dir_mutex;
std::string sm_data_dir;
bool sm_save_user_spec_files = false;
#endif
}//namespace

FullSpectrumApp::FullSpectrumApp( const Wt::WEnvironment& env )
  : Wt::WApplication(env),
#if( BOOST_VERSION <= 106501 )
  m_uuid( to_string( boost::uuids::random_generator()() ) ),    //Need to measure time to construct
#else
  m_uuid( to_string( boost::uuids::random_generator_pure()() ) ), //Can throw entropy_error
#endif
  //m_uuid( std::to_string(std::hash<std::string>{}(sessionId())) ), //is sessionID random?
  m_sessionStart( WDateTime::currentDateTime() ),
  m_gui( nullptr )
{
  enableUpdates();
  
  requireJQuery( "jquery-3.6.0.min.js" );
  
#if( USE_MINIFIED_JS_CSS )
  useStyleSheet( "FullSpectrumApp.min.css" );
#else
  useStyleSheet( "FullSpectrumApp.css" );
#endif
  
  setTitle("Sandia National Laboratories: GADRAS Full-Spectrum Isotope ID");
  root()->addStyleClass( "FullSpectrumApp" );
 
  // We dont have asynchronous parsing of spectrum files and inspecting them, so instead we'll
  //  just make a obvious loading indicator, for the moment
  setLoadingIndicator( make_unique<WOverlayLoadingIndicator>( "LoadingOverlay", "LoadingBackground", "LoadingText") );
  auto loading = loadingIndicator();
  //loading->setMessage( "" );
  
  // Note: it looks like the random_generator gets seeded using the pointer address, which I'm
  //       a little skeptical of (and skeptical that thats actually how the seeding is being done;
  //       boost code is a bit hard to read), but boost authors know way better than me, so I guess
  //       we'll use it for now (not that it matters much I guess anyway).  The alternative would
  //       be to use boost::uuids::random_generator_pure which uses system entropy, but can throw.
  
  Wt::log("info:app") << "New session with SessionId='" << sessionId()
    << "', SessionUUID='" << m_uuid << "'"
    << ", SessionStartTime='" << m_sessionStart.toString("yyyyddmmThh:mm:ss") << "'"
    << ", FromIpAddress='" << env.clientAddress() << "'";
  
  
  // \TODO: add directory files/log will be saved to
  
  
  //const string &path = env.internalPath();
  //if( SpecUtils::istarts_with( path, "/fr") )
  //{
  //  cout << "env.internalPath()='" << env.internalPath() << "'" << endl;
  //  WLocale newlocale("fr");
  //  WApplication::setLocale( newlocale );
  //}
  
#if( FOR_WEB_DEPLOYMENT )
  // SNL corporate required things
  //  TODO: check if we can get rid of fontawesome css file
  //useStyleSheet( "https://www.sandia.gov/_common/fonts/font_awesome/css/fontawesome-all.min.css" );
  useStyleSheet( "https://www.sandia.gov/_common/css/styles.css" );
#endif
  
  
  const string messagePath = SpecUtils::append_path( docRoot(), "messages" );
  const string footerPath = SpecUtils::append_path( docRoot(), "header_footer" );
  auto msgresc = make_shared<WMessageResourceBundle>();
  msgresc->use( messagePath, true );
  msgresc->use( footerPath, true );
  setLocalizedStrings( msgresc );
  
  
#if( FOR_WEB_DEPLOYMENT )
  root()->addStyleClass( "WebVersion" );
  
  auto header = root()->addWidget( std::make_unique<Wt::WTemplate>() );
  header->setTemplateText(WString::tr("app-header-web"));
  
  WContainerWidget *appContent = root()->addWidget( std::make_unique<WContainerWidget>() );
  appContent->addStyleClass( "WebAppContent" );
  
  // Put in support and such links
  WContainerWidget *leftSide = appContent->addWidget( std::make_unique<WContainerWidget>() );
  leftSide->setStyleClass( "WebLeftContent" );
  leftSide->setList( true );
  
  WContainerWidget *item = leftSide->addWidget( std::make_unique<WContainerWidget>() );
  WAnchor *info = item->addWidget( std::make_unique<WAnchor>() );
  info->setText( WString::tr("header-info") );
  info->setAttributeValue( "tabindex", "0" );
  //info->setAttributeValue( "aria-label", "" );
  info->clicked().connect( this, &FullSpectrumApp::showInfoWindow );
  info->enterPressed().connect( this, &FullSpectrumApp::showInfoWindow );
  
  item = leftSide->addWidget( std::make_unique<WContainerWidget>() );
  WAnchor *support = item->addWidget( std::make_unique<WAnchor>() );
  support->setText( WString::tr("web-support") );
  support->setAttributeValue( "tabindex", "0" );
  //support->setAttributeValue( "aria-label", "" );
  support->clicked().connect( this, &FullSpectrumApp::showContactWindow );
  support->enterPressed().connect( this, &FullSpectrumApp::showContactWindow );
  
#if( ENABLE_SESSION_DETAIL_LOGGING )
  const string datadir = data_directory();
  const bool save_spec = save_user_spectrum_files();
  m_gui = appContent->addWidget( std::make_unique<AnalysisGui>(datadir, save_spec) );
#else
  m_gui = contentLayout->addWidget( std::make_unique<AnalysisGui>() );
#endif
  
  
  WContainerWidget *rightSide = appContent->addWidget( std::make_unique<WContainerWidget>() );
  rightSide->setStyleClass( "WebRightContent" );
  
  auto supporthdr = header->bindWidget("support-link", Wt::cpp14::make_unique<Wt::WAnchor>() );
  supporthdr->setText( WString::tr("web-support") );
  supporthdr->setAttributeValue( "tabindex", "0" );
  supporthdr->clicked().connect( this, &FullSpectrumApp::showContactWindow );
  supporthdr->enterPressed().connect( this, &FullSpectrumApp::showContactWindow );
  
  auto infohdr = header->bindWidget( "info-link", Wt::cpp14::make_unique<Wt::WAnchor>() );
  infohdr->setText( WString::tr("header-info") );
  infohdr->setAttributeValue( "tabindex", "0" );
  infohdr->clicked().connect( this, &FullSpectrumApp::showInfoWindow );
  infohdr->enterPressed().connect( this, &FullSpectrumApp::showInfoWindow );
  
  
  auto footer = root()->addWidget( std::make_unique<Wt::WTemplate>() );
  footer->setTemplateText(WString::tr("snl-footer"));
  footer->setHeight( 240 );
#else
  auto grid = root()->setLayout( std::make_unique<Wt::WGridLayout>() );
  grid->setContentsMargins( 0, 0, 0, 0 );
  
  auto header = grid->addWidget( std::make_unique<Wt::WTemplate>(), 0, 0, 1, 3 );
  
  header->setTemplateText(WString::tr("app-header-local"));
  auto support = header->bindWidget("support-link", Wt::cpp14::make_unique<Wt::WText>( WString::tr("header-support") ) );
  support->addStyleClass( "SupportBtn" );
  support->setAttributeValue( "tabindex", "0" );
  //support->setAttributeValue( "aria-label", "" );
  support->clicked().connect( this, &FullSpectrumApp::showContactWindow );
  support->enterPressed().connect( this, &FullSpectrumApp::showContactWindow );
  
  auto info = header->bindWidget( "info-link", Wt::cpp14::make_unique<Wt::WText>( WString::tr("header-info")) );
  info->addStyleClass( "InfoBtn" );
  info->setAttributeValue( "tabindex", "0" );
  //info->setAttributeValue( "aria-label", "" );
  info->clicked().connect( this, &FullSpectrumApp::showInfoWindow );
  info->enterPressed().connect( this, &FullSpectrumApp::showInfoWindow );
  
  grid->addWidget( std::make_unique<WContainerWidget>(), 1, 0 );
  grid->addWidget( std::make_unique<WContainerWidget>(), 1, 2 );
  
#if( ENABLE_SESSION_DETAIL_LOGGING )
  const string datadir = data_directory();
  const bool save_spec = save_user_spectrum_files();
  m_gui = grid->addWidget( std::make_unique<AnalysisGui>(datadir,save_spec), 1, 1 );
#else
  m_gui = grid->addWidget( std::make_unique<AnalysisGui>(), 1, 1 );
#endif

  grid->setRowStretch( 1, 1 );
  grid->setColumnStretch( 0, 1 );
  grid->setColumnStretch( 1, 8 );
  grid->setColumnStretch( 2, 1 );
#endif // if( FOR_WEB_DEPLOYMENT ) / else
}//FullSpectrumApp


void FullSpectrumApp::showInfoWindow()
{
  auto dialog = root()->addChild(Wt::cpp14::make_unique<Wt::WDialog>("About Full Spectrum"));
  dialog->setAttributeValue( "role", "dialog" );
  dialog->setAttributeValue( "aria-labelledby", dialog->titleBar()->id() );
  dialog->addStyleClass( "InfoWindow" );
  dialog->rejectWhenEscapePressed();
  
  auto content = dialog->contents()->addWidget( make_unique<WTemplate>() );
  content->setTemplateText( WString::tr("info-window") );
  dialog->rejectWhenEscapePressed();
  dialog->setMovable( false );
  dialog->setClosable( true );
  Wt::WPushButton *close = dialog->footer()->addNew<Wt::WPushButton>("Close");
  close->clicked().connect( dialog, &Wt::WDialog::reject );
  close->setCanReceiveFocus(true);
  close->setFocus();
  
  dialog->finished().connect( [=](){
    root()->removeChild( dialog );
  });
  
  content->bindString( "build-date", WString::fromUTF8(__DATE__) );
  content->bindString( "gadras-version", WString::fromUTF8(Analysis::gadras_version_string()) );
  
  //BOOST_LIB_VERSION  //"boost/version.hpp"
  //wApp->libraryVersion()
  
  dialog->show();
}//void showInfoWindow()


void FullSpectrumApp::showContactWindow()
{
  auto dialog = root()->addChild(Wt::cpp14::make_unique<Wt::WDialog>("Full Spectrum Contact"));
  dialog->setAttributeValue( "role", "dialog" );
  dialog->setAttributeValue( "aria-labelledby", dialog->titleBar()->id() );
  dialog->addStyleClass( "InfoWindow" );
  dialog->rejectWhenEscapePressed();
  dialog->setMovable( false );
  dialog->setClosable( true );
  Wt::WPushButton *close = dialog->footer()->addNew<Wt::WPushButton>("Close");
  close->clicked().connect( dialog, &Wt::WDialog::reject );
  close->setCanReceiveFocus(true);
  close->setFocus();
  dialog->finished().connect( [=](){
    root()->removeChild( dialog );
  });
  
  auto content = dialog->contents()->addWidget( make_unique<WTemplate>() );
  content->setTemplateText( WString::tr("contact-window") );
  content->addStyleClass( "InfoWindowContent" );
  
  
  string shortuuid = m_uuid;
  const auto pos = shortuuid.find( '-' );
  if( pos != string::npos )
    shortuuid = shortuuid.substr(0,pos);
  content->bindString( "app-session", WString::fromUTF8(m_uuid) );
  content->bindString( "app-session-short", WString::fromUTF8(shortuuid) );
  content->bindString( "server-time", m_sessionStart.toString("ddd MMM d hh:mm:ss yyyy") );
  
  
  dialog->show();
}//void showContactWindow();

#if( ENABLE_SESSION_DETAIL_LOGGING )
void FullSpectrumApp::set_data_directory( const std::string &dir, const bool save_files )
{
  if( !dir.empty() && !SpecUtils::is_directory(dir) )
    throw runtime_error( "FullSpectrumApp::set_data_directory('" + dir + "'): invalid directory." );
  
  if( save_files && dir.empty() )
    throw runtime_error( "FullSpectrumApp::set_data_directory: you must specify the data directory when save_files is true" );
  
  std::unique_lock<std::mutex> queue_lock( sm_data_dir_mutex );
  sm_data_dir = dir;
  sm_save_user_spec_files = save_files;
}//void set_data_directory( const std::string &dir )


std::string FullSpectrumApp::data_directory()
{
  std::unique_lock<std::mutex> queue_lock( sm_data_dir_mutex );
  return sm_data_dir;
}//std::string data_directory()

bool FullSpectrumApp::save_user_spectrum_files()
{
  std::unique_lock<std::mutex> queue_lock( sm_data_dir_mutex );
  return sm_save_user_spec_files;
}
#endif

