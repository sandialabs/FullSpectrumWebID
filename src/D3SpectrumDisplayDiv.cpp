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

#include <memory>
#include <vector>
#include <utility>

#include <Wt/WJavaScript.h>
#include <Wt/WApplication.h>
#include <Wt/WStringStream.h>
#include <Wt/WContainerWidget.h>

#include "SpecUtils/SpecFile.h"
#include "SpecUtils/DateTime.h"
#include "SpecUtils/StringAlgo.h"
#include "SpecUtils/D3SpectrumExport.h"
#include "FullSpectrumId/D3SpectrumDisplayDiv.h"

using namespace Wt;
using namespace std;

using SpecUtils::Measurement;

namespace
{
  const std::string &jsbool( bool val )
  {
    static const std::string t = "true";
    static const std::string f = "false";
    
    return val ? t : f;
  };
}//namespace


D3SpectrumDisplayDiv::D3SpectrumDisplayDiv()
: WContainerWidget(),
  m_renderFlags(),
  m_layoutWidth( 0 ),
  m_layoutHeight( 0 ),
  m_foreground( nullptr ),
  m_secondary( nullptr ),
  m_background( nullptr ),
  m_secondaryScale( 1.0f ),
  m_backgroundScale( 1.0f ),
  m_compactAxis( false ),
  m_legendEnabled( true ),
  m_yAxisIsLog( true ),
  m_backgroundSubtract( false ),
  m_showVerticalLines( false ),
  m_showHorizontalLines( false ),
  m_showHistogramIntegralsInLegend( true ),
  m_showXAxisSliderChart( false ),
  m_showYAxisScalers( false ),
  m_jsgraph( jsRef() + ".chart" ),
  m_xAxisMinimum(0.0),
  m_xAxisMaximum(0.0),
  m_yAxisMinimum(0.0),
  m_yAxisMaximum(0.0),
  m_chartWidthPx(0.0),
  m_chartHeightPx(0.0),
  m_foregroundLineColor( 0x00, 0x00, 0x00 ),  //black
  m_backgroundLineColor( 0x00, 0xff, 0xff ),  //cyan
  m_secondaryLineColor( 0x00, 0x80, 0x80 ),   //dark green
  m_textColor( 0x00, 0x00, 0x00 ),
  m_axisColor( 0x00, 0x00, 0x00 ),
  m_chartMarginColor(),
  m_chartBackgroundColor(),
  m_defaultPeakColor( 0, 51, 255, 155 )
{
  setLayoutSizeAware( true );
  addStyleClass( "SpectrumDisplayDiv" );
  
  // Cancel right-click events for the div, we handle it all in JS
  setAttributeValue( "oncontextmenu",
                     "event.cancelBubble = true; event.returnValue = false; return false;"
                    );
#if( USE_MINIFIED_JS_CSS )
  wApp->useStyleSheet( "SpectrumChartD3.min.css" );
#else
  wApp->useStyleSheet( "SpectrumChartD3.css" );
#endif
  
  initChangeableCssRules();
  
  wApp->require( "d3.v3.min.js", "d3.v3.js" );
  
#if( USE_MINIFIED_JS_CSS )
  wApp->require( "SpectrumChartD3.min.js" );
#else
  wApp->require( "SpectrumChartD3.js" );
#endif
}//D3SpectrumDisplayDiv constructor


void D3SpectrumDisplayDiv::defineJavaScript()
{
  string options = "{title: '', showAnimation: true, animationDuration: 200";
  options += ", xlabel: '" + m_xAxisTitle + "'";
  options += ", ylabel: '" + m_yAxisTitle + "'";
  options += ", compactXAxis: " + jsbool(m_compactAxis);
  options += ", allowDragRoiExtent: true";
  options += ", showRefLineInfoForMouseOver: " + jsbool(false);
  options += ", yscale: " + string(m_yAxisIsLog ? "'log'" : "'lin'");
  options += ", backgroundSubtract: " + jsbool( m_backgroundSubtract );
  options += ", showLegend: " + jsbool(m_legendEnabled);
  options += ", gridx: " + jsbool(m_showHorizontalLines);
  options += ", gridy: " + jsbool(m_showVerticalLines);
  options += ", showXAxisSliderChart: " + jsbool(m_showXAxisSliderChart);
  options += ", scaleBackgroundSecondary: " + jsbool(m_showYAxisScalers);
  options += ", wheelScrollYAxis: true";
  options += ", sliderChartHeightFraction: 0.1";  //ToDo: track this in C++
  options += ", spectrumLineWidth: 1.0";  //ToDo: Let this be specified in C++
  options += ", showUserLabels: " + jsbool(false);
  options += ", showPeakLabels: " + jsbool(false);
  options += ", showNuclideNames: " + jsbool(false);
  options += ", showNuclideEnergies: " + jsbool(false);
  options += ", showEscapePeaks:" + jsbool(false);
  options += ", showComptonPeaks:" + jsbool(false);
  options += ", showComptonPeaks:" + jsbool(false);
  options += ", showSumPeaks:" + jsbool(false);
  // For FullSpectrum we dont care about receiving client side events, so we'll skip it
  options += ", noEventsToServer:" + jsbool(true);
  options += "}";
  
  setJavaScriptMember( "chart", "new SpectrumChartD3(" + jsRef() + "," + options + ");");
  setJavaScriptMember( "wtResize", "function(self, w, h, layout){ if(" + m_jsgraph + ") " + m_jsgraph + ".handleResize();}" );
  
  if( !m_xRangeChangedJS )
  {
    using namespace std::placeholders;
    
    m_xRangeChangedJS.reset( new JSignal<double,double,double,double>( this, "xrangechanged", true ) );
    m_xRangeChangedJS->connect( std::bind( &D3SpectrumDisplayDiv::chartXRangeChangedCallback, this, _1, _2, _3, _4 ) );
    
    m_shiftKeyDraggJS.reset( new JSignal<double,double>( this, "shiftkeydragged", true ) );
    m_shiftKeyDraggJS->connect( std::bind( &D3SpectrumDisplayDiv::chartShiftKeyDragCallback, this, _1, _2 ) );
    
    m_shiftAltKeyDraggJS.reset( new JSignal<double,double>( this, "shiftaltkeydragged", true ) );
    m_shiftAltKeyDraggJS->connect( std::bind( &D3SpectrumDisplayDiv::chartShiftAltKeyDragCallback, this, _1, _2 ) );
    
    m_rightMouseDraggJS.reset( new JSignal<double,double>( this, "rightmousedragged", true ) );
    m_rightMouseDraggJS->connect( std::bind( &D3SpectrumDisplayDiv::chartRightMouseDragCallback, this, _1, _2 ) );
    
    m_leftClickJS.reset( new JSignal<double,double,int,int>( this, "leftclicked", true ) );
    m_leftClickJS->connect( std::bind( &D3SpectrumDisplayDiv::chartLeftClickCallback, this, _1, _2, _3, _4 ) );
    
    m_doubleLeftClickJS.reset( new JSignal<double,double>( this, "doubleclicked", true ) );
    m_doubleLeftClickJS->connect( std::bind( &D3SpectrumDisplayDiv::chartDoubleLeftClickCallback, this, _1, _2 ) );
    
    m_rightClickJS.reset( new JSignal<double,double,int,int>( this, "rightclicked", true ) );
    m_rightClickJS->connect( std::bind( &D3SpectrumDisplayDiv::chartRightClickCallback, this, _1, _2, _3, _4 ) );
    
    m_yAxisDraggedJS.reset( new Wt::JSignal<double,std::string>( this, "yscaled", true ) );
    m_yAxisDraggedJS->connect( [this]( const double scale, const std::string &spectrum ){
      yAxisScaled(scale,spectrum);
    } );
    
    //need legend closed signal.
    m_legendClosedJS.reset( new JSignal<>( this, "legendClosed", true ) );
    m_legendClosedJS->connect( std::bind( [this](){
      m_legendEnabled = false;
      m_legendDisabledSignal.emit();
    }) );
  }//if( !m_xRangeChangedJS )
  
  
  for( const string &js : m_pendingJs )
    doJavaScript( js );
  m_pendingJs.clear();
  m_pendingJs.shrink_to_fit();
  
  //I think x and y ranges should be taken care of via m_pendingJs... untested
  //m_xAxisMinimum, m_xAxisMaximum, m_yAxisMinimum, m_yAxisMaximum;
}//void defineJavaScript()


void D3SpectrumDisplayDiv::initChangeableCssRules()
{
  WCssStyleSheet &style = wApp->styleSheet();

  //m_cssRules["TextColor"] = style.addRule( ".xaxistitle, .yaxistitle, .yaxis, .yaxislabel, .xaxis", "stroke: blue" );
  //m_cssRules["AxisColor"] = style.addRule( ".xaxis > .domain, .yaxis > .domain, .xaxis > .tick > line, .yaxis > .tick, .yaxistick", "stroke: red;" );
  m_cssRules["GridColor"] = style.addRule( ".xgrid > .tick, .ygrid > .tick", "stroke: #b3b3b3" );
  m_cssRules["MinorGridColor"] = style.addRule( ".minorgrid", "stroke: #e6e6e6" );
  //m_cssRules["FeatureLinesColor"] = style.addRule( ".peakLine, .escapeLineForward, .mouseLine, .secondaryMouseLine", "stroke: black" );
  
}//void initChangeableCssRules()


void D3SpectrumDisplayDiv::setTextInMiddleOfChart( const Wt::WString &s )
{
  // TODO: D3 chart currently does not have functionality to set text in the middle of chart...
}

void D3SpectrumDisplayDiv::setCompactAxis( const bool compact )
{
  m_compactAxis = compact;
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setCompactXAxis(" + jsbool(compact) + ");" );
}

bool D3SpectrumDisplayDiv::isAxisCompacted() const
{
  return m_compactAxis;
}


bool D3SpectrumDisplayDiv::legendIsEnabled() const
{
  return m_legendEnabled;
}


void D3SpectrumDisplayDiv::enableLegend()
{
  m_legendEnabled = true;
  m_legendEnabledSignal.emit();
  
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setShowLegend(true);" );
}//void D3SpectrumDisplayDiv::enableLegend()


void D3SpectrumDisplayDiv::disableLegend()
{
  m_legendEnabled = false;
  m_legendDisabledSignal.emit();
  
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setShowLegend(false);" );
}//void disableLegend()


void D3SpectrumDisplayDiv::showHistogramIntegralsInLegend( const bool show )
{
  m_showHistogramIntegralsInLegend = show;
  // TODO: No option in D3 to show/hide histogram integrals in legend
}


Signal<> &D3SpectrumDisplayDiv::legendEnabled()
{
  return m_legendEnabledSignal;
}


Signal<> &D3SpectrumDisplayDiv::legendDisabled()
{
  return m_legendDisabledSignal;
}


Signal<double,double,int,int> &D3SpectrumDisplayDiv::chartClicked()
{
  return m_leftClick;
}//Signal<double,double,int,int> &chartClicked()


Wt::Signal<double,double,int,int> &D3SpectrumDisplayDiv::rightClicked()
{
  return m_rightClick;
}

Wt::Signal<double,double,double,double,double,bool> &D3SpectrumDisplayDiv::roiDragUpdate()
{
  return m_roiDrag;
}

Wt::Signal<double, double, int, bool> &D3SpectrumDisplayDiv::fitRoiDragUpdate()
{
  return m_fitRoiDrag;
}


Wt::Signal<double,SpecUtils::SpectrumType> &D3SpectrumDisplayDiv::yAxisScaled()
{
  return m_yAxisScaled;
}

Wt::Signal<double,double> &D3SpectrumDisplayDiv::doubleLeftClick()
{
  return m_doubleLeftClick;
}

Wt::Signal<double,double> &D3SpectrumDisplayDiv::controlKeyDragged()
{
  return m_controlKeyDragg;
}

Wt::Signal<double,double> &D3SpectrumDisplayDiv::shiftKeyDragged()
{
  return m_shiftKeyDragg;
}


Wt::Signal<double,double> &D3SpectrumDisplayDiv::rightMouseDragg()
{
  return m_rightMouseDragg;
}//Signal<double,double> &rightMouseDragg()


void D3SpectrumDisplayDiv::layoutSizeChanged ( int width, int height )
{
  m_layoutWidth = width;
  m_layoutHeight = height;
}//void layoutSizeChanged ( int width, int height )


void D3SpectrumDisplayDiv::render( Wt::WFlags<Wt::RenderFlag> flags )
{
  const bool renderFull = flags.test(Wt::RenderFlag::Full);
  //const bool renderUpdate = flags.test(Wt::RenderFlag::Update);
  
  WContainerWidget::render( flags );
  
  if( renderFull )
    defineJavaScript();
  
  if( m_renderFlags.test(UpdateForegroundSpectrum) )
    renderForegroundToClient();
  
  if( m_renderFlags.test(UpdateBackgroundSpectrum) )
    renderBackgroundToClient();
  
  if( m_renderFlags.test(UpdateSecondarySpectrum) )
    renderSecondDataToClient();
  
  m_renderFlags = Wt::WFlags<D3RenderActions>();
}


int D3SpectrumDisplayDiv::layoutWidth() const
{
  return m_layoutWidth;
}//int layoutWidth() const


int D3SpectrumDisplayDiv::layoutHeight() const
{
  return m_layoutHeight;
}//int layoutHeight() const


double D3SpectrumDisplayDiv::xAxisMinimum() const
{
  return m_xAxisMinimum;
}//double xAxisMinimum() const


double D3SpectrumDisplayDiv::xAxisMaximum() const
{
  return m_xAxisMaximum;
}//double xAxisMaximum() const


double D3SpectrumDisplayDiv::chartWidthInPixels() const
{
  return m_chartWidthPx;
}//double chartWidthInPixels() const

double D3SpectrumDisplayDiv::chartHeightInPixels() const
{
  return m_chartHeightPx;
}//double chartHeightInPixels() const

double D3SpectrumDisplayDiv::yAxisMinimum() const
{
  return m_yAxisMinimum;
}//double yAxisMinimum() const


double D3SpectrumDisplayDiv::yAxisMaximum() const
{
  return m_yAxisMaximum;
}//double yAxisMaximum() const


bool D3SpectrumDisplayDiv::yAxisIsLog() const
{
  return m_yAxisIsLog;
}//bool yAxisIsLog() const;


void D3SpectrumDisplayDiv::setYAxisLog( bool log )
{
  m_yAxisIsLog = log;
  if( isRendered() )
    doJavaScript( m_jsgraph + (log ? ".setLogY();" : ".setLinearY();") );
}//void setYAxisLog( bool log )

void D3SpectrumDisplayDiv::showGridLines( bool show )
{
  m_showVerticalLines = show;
  m_showHorizontalLines = show;
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setGridX(" + jsbool(show) + ");"
                  + m_jsgraph + ".setGridY(" + jsbool(show) + ");" );
}

void D3SpectrumDisplayDiv::showVerticalLines( const bool draw )
{
  m_showVerticalLines = draw;
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setGridX(" + jsbool(draw) + ");" );
}

void D3SpectrumDisplayDiv::showHorizontalLines( const bool draw )
{
  m_showHorizontalLines = draw;
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setGridY(" + jsbool(draw) + ");" );
}

bool D3SpectrumDisplayDiv::verticalLinesShowing() const
{
  return m_showVerticalLines;
}

bool D3SpectrumDisplayDiv::horizontalLinesShowing() const
{
  return m_showHorizontalLines;
}


bool D3SpectrumDisplayDiv::backgroundSubtract() const
{
  return m_backgroundSubtract;
}//bool backgroundSubtract() const


void D3SpectrumDisplayDiv::setBackgroundSubtract( bool subtract )
{
  if( subtract == m_backgroundSubtract )
    return;
  
  m_backgroundSubtract = subtract;
  
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setBackgroundSubtract(" + jsbool(subtract) + ");" );
}//void setBackgroundSubtract( bool subtract )

void D3SpectrumDisplayDiv::setXAxisMinimum( const double minimum )
{
  const string minimumStr = to_string( minimum );
  m_xAxisMinimum = minimum;
  
  string js = m_jsgraph + ".setXAxisMinimum(" + minimumStr + ");";
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void setXAxisMinimum( const double minimum )


void D3SpectrumDisplayDiv::setXAxisMaximum( const double maximum )
{
  const string maximumStr = to_string( maximum );
  m_xAxisMaximum = maximum;
  
  string js = m_jsgraph + ".setXAxisMaximum(" + maximumStr + ");";
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void setXAxisMaximum( const double maximum )


void D3SpectrumDisplayDiv::setYAxisMinimum( const double minimum )
{
  const string minimumStr = to_string( minimum );
  m_yAxisMinimum = minimum;
  
  string js = m_jsgraph + ".setYAxisMinimum(" + minimumStr + ");";
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void setYAxisMinimum( const double minimum )


void D3SpectrumDisplayDiv::setYAxisMaximum( const double maximum )
{
  const string maximumStr = to_string( maximum );
  m_yAxisMaximum = maximum;
  
  string js = m_jsgraph + ".setYAxisMaximum(" + maximumStr + ");";
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void setYAxisMaximum( const double maximum )


void D3SpectrumDisplayDiv::setXAxisRange( const double minimum, const double maximum )
{
  const string minimumStr = to_string( minimum );
  const string maximumStr = to_string( maximum );
  m_xAxisMinimum = minimum;
  m_xAxisMaximum = maximum;
  
  string js = m_jsgraph + ".setXAxisRange(" + minimumStr + "," + maximumStr + ",false);"
              + m_jsgraph + ".redraw()();";
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void setXAxisRange( const double minimum, const double maximum );


void D3SpectrumDisplayDiv::setYAxisRange( const double minimum,
                                       const double maximum )
{
  //cout << "setYAxisRange" << endl;
  const string minimumStr = to_string( minimum );
  const string maximumStr = to_string( maximum );
  m_yAxisMinimum = minimum;
  m_yAxisMaximum = maximum;
  
  string js = m_jsgraph + ".setYAxisRange(" + minimumStr + "," + maximumStr + ");";
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void setYAxisRange( const double minimum, const double maximum );


void D3SpectrumDisplayDiv::doBackgroundLiveTimeNormalization()
{
  if( !m_background || !m_foreground
     || (m_background->live_time() <= 0.0f)
     || (m_foreground->live_time() <= 0.0f) )
  {
    m_backgroundScale = 1.0f;
  }else
  {
    m_backgroundScale = m_foreground->live_time() / m_background->live_time();
  }
}//void doBackgroundLiveTimeNormalization()


void D3SpectrumDisplayDiv::doSecondaryLiveTimeNormalization()
{
  if( !m_secondary || !m_foreground
     || (m_secondary->live_time() <= 0.0f)
     || (m_foreground->live_time() <= 0.0f) )
  {
    m_secondaryScale = 1.0f;
  }else
  {
    m_secondaryScale = m_foreground->live_time() / m_secondary->live_time();
  }
}//void doSecondaryLiveTimeNormalization()


void D3SpectrumDisplayDiv::setData( std::shared_ptr<Measurement> data_hist )
{
  const float oldBackSF = m_backgroundScale;
  const float oldSecondSF = m_secondaryScale;
  
  m_foreground = data_hist;
  
  m_renderFlags |= ResetXDomain;
  
  scheduleUpdateForeground();
  
  // Update background and second SF
  doBackgroundLiveTimeNormalization();
  doSecondaryLiveTimeNormalization();
  
  
  //If the background or secondary data spectrum scale factors changed, we need
  //  to update those to the client as well.
  if( m_background && (fabs(m_backgroundScale - oldBackSF) > 0.000001f*std::max(m_backgroundScale,oldBackSF)) )
    scheduleUpdateBackground();
  
  if( m_secondary && (fabs(m_secondaryScale - oldSecondSF) > 0.000001f*std::max(m_secondaryScale,oldSecondSF)) )
    scheduleUpdateSecondData();
}//void setData( std::shared_ptr<Measurement> data_hist )


std::shared_ptr<Measurement> D3SpectrumDisplayDiv::data()
{
  return m_foreground;
}//std::shared_ptr<Measurement> data()


std::shared_ptr<const Measurement> D3SpectrumDisplayDiv::data() const
{
  return m_foreground;
}//std::shared_ptr<const Measurement> data() const


std::shared_ptr<Measurement> D3SpectrumDisplayDiv::secondData()
{
  return m_secondary;
}//std::shared_ptr<Measurement> secondData()


std::shared_ptr<const Measurement> D3SpectrumDisplayDiv::secondData() const
{
  return m_secondary;
}//std::shared_ptr<const Measurement> secondData() const


std::shared_ptr<Measurement> D3SpectrumDisplayDiv::background()
{
  return m_background;
}//std::shared_ptr<Measurement> background()


std::shared_ptr<const Measurement> D3SpectrumDisplayDiv::background() const
{
  return m_background;
}//std::shared_ptr<const Measurement> background() const


float D3SpectrumDisplayDiv::foregroundLiveTime() const
{
  return m_foreground ? m_foreground->live_time() : 0.0f;
}

float D3SpectrumDisplayDiv::foregroundRealTime() const
{
  return m_foreground ? m_foreground->real_time() : 0.0f;
}

float D3SpectrumDisplayDiv::backgroundLiveTime() const
{
  return m_background ? m_background->live_time() : 0.0f;
}

float D3SpectrumDisplayDiv::backgroundRealTime() const
{
  return m_background ? m_background->real_time() : 0.0f;
}

float D3SpectrumDisplayDiv::secondForegroundLiveTime() const
{
  return m_secondary ? m_secondary->live_time() : 0.0f;
}

float D3SpectrumDisplayDiv::secondForegroundRealTime() const
{
  return m_secondary ? m_secondary->real_time() : 0.0f;
}


void D3SpectrumDisplayDiv::setDisplayScaleFactor( const float sf,
                                               const SpecUtils::SpectrumType spectrum_type )
{
  switch( spectrum_type )
  {
    case SpecUtils::SpectrumType::Foreground:
      throw runtime_error( "setDisplayScaleFactor can not be called for foreground" );
      
    case SpecUtils::SpectrumType::SecondForeground:
      m_secondaryScale = sf;
      scheduleUpdateSecondData();
      break;
      
    case SpecUtils::SpectrumType::Background:
      m_backgroundScale = sf;
      scheduleUpdateBackground();
      break;
  }//switch( spectrum_type )
}//void setDisplayScaleFactor(...)




float D3SpectrumDisplayDiv::displayScaleFactor( const SpecUtils::SpectrumType spectrum_type ) const
{
  switch( spectrum_type )
  {
    case SpecUtils::SpectrumType::Foreground:
      return 1.0f;
    case SpecUtils::SpectrumType::SecondForeground:
      return m_secondaryScale;
    case SpecUtils::SpectrumType::Background:
      return m_backgroundScale;
  }//switch( spectrum_type )
  
  throw runtime_error( "D3SpectrumDisplayDiv::displayScaleFactor(...): invalid input arg" );
  
  return 1.0f;
}//double displayScaleFactor( SpecUtils::SpectrumType spectrum_type ) const;


void D3SpectrumDisplayDiv::setBackground( std::shared_ptr<Measurement> background )
{
  m_background = background;
  doBackgroundLiveTimeNormalization();
  if( !background && m_backgroundSubtract )
    setBackgroundSubtract( false );
  scheduleUpdateBackground();
}//void D3SpectrumDisplayDiv::setBackground(...);


void D3SpectrumDisplayDiv::setSecondData( std::shared_ptr<Measurement> hist )
{
  m_secondary = hist;
  doSecondaryLiveTimeNormalization();
  scheduleUpdateSecondData();
}//void D3SpectrumDisplayDiv::setSecondData( std::shared_ptr<Measurement> background );


void D3SpectrumDisplayDiv::visibleRange( double &xmin, double &xmax,
                                      double &ymin, double &ymax ) const
{
  xmin = m_xAxisMinimum;
  xmax = m_xAxisMaximum;
  ymin = m_yAxisMinimum;
  ymax = m_yAxisMaximum;
}


const string D3SpectrumDisplayDiv::xAxisTitle() const
{
  return m_xAxisTitle;
}//const Wt::WString &xAxisTitle() const;


const string D3SpectrumDisplayDiv::yAxisTitle() const
{
  return m_yAxisTitle;
}//const Wt::WString &yAxisTitle() const;


void D3SpectrumDisplayDiv::setXAxisTitle( const std::string &title )
{
  m_xAxisTitle = title;
  SpecUtils::ireplace_all( m_xAxisTitle, "'", "&#39;" );
  
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setXAxisTitle('" + title + "');" );
}//void setXAxisTitle( const std::string &title )


void D3SpectrumDisplayDiv::setYAxisTitle( const std::string &title )
{
  m_yAxisTitle = title;
  SpecUtils::ireplace_all( m_yAxisTitle, "'", "&#39;" );
  
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setYAxisTitle('" + title + "');" );
}//void setYAxisTitle( const std::string &title )



Wt::Signal<double,double> &D3SpectrumDisplayDiv::xRangeChanged()
{
  return m_xRangeChanged;
}//xRangeChanged()


Wt::Signal<double,double> &D3SpectrumDisplayDiv::shiftAltKeyDragged()
{
  return m_shiftAltKeyDragg;
}




void D3SpectrumDisplayDiv::scheduleUpdateForeground()
{
  m_renderFlags |= UpdateForegroundSpectrum;
  scheduleRender();
}


void D3SpectrumDisplayDiv::scheduleUpdateBackground()
{
  m_renderFlags |= UpdateBackgroundSpectrum;
  scheduleRender();
}


void D3SpectrumDisplayDiv::scheduleUpdateSecondData()
{
  m_renderFlags |= UpdateSecondarySpectrum;
  scheduleRender();
}


void D3SpectrumDisplayDiv::renderForegroundToClient()
{
  const std::shared_ptr<Measurement> data_hist = m_foreground;
  
  string js;
  
  const string resetDomain = m_renderFlags.test(ResetXDomain) ? "true" : "false";
  
  // Set the data for the chart
  if ( data_hist ) {
    // Create the measurement array (should only have one measurement)
    std::ostringstream ostr;
    std::vector< std::pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions> > measurements;
    std::pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions> foregroundData;
    D3SpectrumExport::D3SpectrumOptions foregroundOptions;
    
    // Set options for the spectrum
    foregroundOptions.line_color = m_foregroundLineColor.isDefault() ? string("black") : m_foregroundLineColor.cssText();
    foregroundOptions.peak_color = m_defaultPeakColor.isDefault() ? string("blue") : m_defaultPeakColor.cssText();
    foregroundOptions.spectrum_type = SpecUtils::SpectrumType::Foreground;
    foregroundOptions.display_scale_factor = displayScaleFactor( SpecUtils::SpectrumType::Foreground );  //will always be 1.0f
    
    // Set the peak data for the spectrum
    
    measurements.push_back( pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions>(data_hist.get(),foregroundOptions) );
    
    // Set the data on the JS side
    if ( D3SpectrumExport::write_and_set_data_for_chart(ostr, id(), measurements) ) {
      string data = ostr.str();
      size_t index = data.find( "spec_chart_" );
      data = data.substr( 0, index );
      js = data + m_jsgraph + ".setSpectrumData(data_" + id() + ", " + resetDomain + ", 'FOREGROUND', 0, 1 );";
    }
  } else {
    //js = m_jsgraph + ".removeSpectrumDataByType(" + resetDomain + ", 'FOREGROUND' );";
    js = m_jsgraph + ".setData(null,true);";
  }//if ( data_hist ) / else
  
  
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void D3SpectrumDisplayDiv::updateData()


void D3SpectrumDisplayDiv::renderBackgroundToClient()
{
  string js;
  const std::shared_ptr<Measurement> background = m_background;
  
  // Set the data for the chart
  if ( background ) {
    // Create the measurement array (should only have one measurement)
    std::ostringstream ostr;
    std::vector< std::pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions> > measurements;
    std::pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions> backgroundData;
    D3SpectrumExport::D3SpectrumOptions backgroundOptions;
    
    // Set options for the spectrum
    backgroundOptions.line_color = m_backgroundLineColor.isDefault() ? string("green") : m_backgroundLineColor.cssText();
    backgroundOptions.spectrum_type = SpecUtils::SpectrumType::Background;
    backgroundOptions.display_scale_factor = displayScaleFactor( SpecUtils::SpectrumType::Background );
    
    // We cant currently access the parent InterSpec class, but if we could, then
    //  we could draw the background peaks by doing something like:
    //const std::set<int> &backSample = m_interspec->displayedSamples(SpecUtils::SpectrumType::Background);
    //std::shared_ptr<SpecMeas> backgroundMeas = m_interspec->measurment(SpecUtils::SpectrumType::Background);
    //std::shared_ptr< std::deque< std::shared_ptr<const PeakDef> > > backpeaks = backgroundMeas->peaks( backSample );
    //vector< std::shared_ptr<const PeakDef> > inpeaks( backpeaks->begin(), backpeaks->end() );
    //backgroundOptions.peaks_json = PeakDef::peak_json( inpeaks );
    
    measurements.push_back( pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions>(background.get(), backgroundOptions) );
    
    // Set the data on the JS side
    if ( D3SpectrumExport::write_and_set_data_for_chart(ostr, id(), measurements) ) {
      string data = ostr.str();
      size_t index = data.find( "spec_chart_" );
      data = data.substr( 0, index );
      js = data + m_jsgraph + ".setSpectrumData(data_" + id() + ", false, 'BACKGROUND', 1, -1);";
    }
  } else {
    js = m_jsgraph + ".removeSpectrumDataByType(false, 'BACKGROUND' );";
  }//if ( background )
  
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void D3SpectrumDisplayDiv::updateBackground()


void D3SpectrumDisplayDiv::renderSecondDataToClient()
{
  string js;
  const std::shared_ptr<Measurement> hist = m_secondary;
  
  // Set the data for the chart
  if ( hist ) {
    // Create the measurement array (should only have one measurement)
    std::ostringstream ostr;
    std::vector< std::pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions> > measurements;
    std::pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions> secondaryData;
    D3SpectrumExport::D3SpectrumOptions secondaryOptions;
    
    // Set options for the spectrum
    secondaryOptions.line_color = m_secondaryLineColor.isDefault() ? string("steelblue") : m_secondaryLineColor.cssText();
    secondaryOptions.spectrum_type = SpecUtils::SpectrumType::SecondForeground;
    secondaryOptions.display_scale_factor = displayScaleFactor( SpecUtils::SpectrumType::SecondForeground );
    
    measurements.push_back( pair<const Measurement *,D3SpectrumExport::D3SpectrumOptions>(hist.get(), secondaryOptions) );
    
    // Set the data on the JS side
    if ( D3SpectrumExport::write_and_set_data_for_chart(ostr, id(), measurements) ) {
      string data = ostr.str();
      size_t index = data.find( "spec_chart_" );
      data = data.substr( 0, index );
      js = data + m_jsgraph + ".setSpectrumData(data_" + id() + ", false, 'SECONDARY', 2, 1);";
    }
  } else {
    js = m_jsgraph + ".removeSpectrumDataByType(false, 'SECONDARY' );";
  }//if ( hist )
  
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}//void D3SpectrumDisplayDiv::updateSecondData()


void D3SpectrumDisplayDiv::setForegroundSpectrumColor( const Wt::WColor &color )
{
  m_foregroundLineColor = color.isDefault() ? WColor( 0x00, 0x00, 0x00 ) : color;
  scheduleUpdateForeground();
}

void D3SpectrumDisplayDiv::setBackgroundSpectrumColor( const Wt::WColor &color )
{
  m_backgroundLineColor = color.isDefault() ? WColor(0x00,0xff,0xff) : color;
  scheduleUpdateBackground();
}

void D3SpectrumDisplayDiv::setSecondarySpectrumColor( const Wt::WColor &color )
{
  m_secondaryLineColor = color.isDefault() ? WColor(0x00,0x80,0x80) : color;
  scheduleUpdateSecondData();
}

void D3SpectrumDisplayDiv::setTextColor( const Wt::WColor &color )
{
  m_textColor = color.isDefault() ? WColor(0,0,0) : color;
  const string c = m_textColor.cssText();
  
  const string rulename = "TextColor";
  
  WCssStyleSheet &style = wApp->styleSheet();
  if( m_cssRules.count(rulename) )
    style.removeRule( m_cssRules[rulename] );
  m_cssRules[rulename] = style.addRule( ".xaxistitle, .yaxistitle, .yaxis, .yaxislabel, .xaxis", "stroke: " + c );
}


void D3SpectrumDisplayDiv::setAxisLineColor( const Wt::WColor &color )
{
  m_axisColor = color.isDefault() ? WColor(0,0,0) : color;
  
  string rulename = "AxisColor";
  
  WCssStyleSheet &style = wApp->styleSheet();
  if( m_cssRules.count(rulename) )
    style.removeRule( m_cssRules[rulename] );
  m_cssRules[rulename] = style.addRule( ".xaxis > .domain, .yaxis > .domain, .xaxis > .tick > line, .yaxis > .tick, .yaxistick", "stroke: " + m_axisColor.cssText() );
  
  //ToDo: is setting feature line colors okay like this
  rulename = "FeatureLinesColor";
  if( m_cssRules.count(rulename) )
    style.removeRule( m_cssRules[rulename] );
  m_cssRules[rulename] = style.addRule( ".peakLine, .escapeLineForward, .mouseLine, .secondaryMouseLine", "stroke: " + m_axisColor.cssText() );
  
  //ToDo: figure out how to make grid lines look okay.
  //rulename = "GridColor";
  //if( m_cssRules.count(rulename) )
  //  style.removeRule( m_cssRules[rulename] );
  //m_cssRules[rulename] = style.addRule( ".xgrid > .tick, .ygrid > .tick", "stroke: #b3b3b3" );
  //
  //rulename = "MinorGridColor";
  //if( m_cssRules.count(rulename) )
  //  style.removeRule( m_cssRules[rulename] );
  //m_cssRules[rulename] = style.addRule( ".minorgrid", "stroke: #e6e6e6" );
}

void D3SpectrumDisplayDiv::setChartMarginColor( const Wt::WColor &color )
{
  m_chartMarginColor = color;
  
  const string rulename = "MarginColor";
  
  WCssStyleSheet &style = wApp->styleSheet();
  
  if( color.isDefault() )
  {
    if( m_cssRules.count(rulename) )
    {
      style.removeRule( m_cssRules[rulename] );
      m_cssRules.erase( rulename );
    }
    
    return;
  }//if( color.isDefault() )
  
  //Actualkly this will set the background for the entire chart...
  m_cssRules[rulename] = style.addRule( "#" + id() + " > svg", "background: " + color.cssText() );
}//setChartMarginColor(...)


void D3SpectrumDisplayDiv::setChartBackgroundColor( const Wt::WColor &color )
{
  m_chartBackgroundColor = color;
  const string c = color.isDefault() ? "rgba(0,0,0,0)" : color.cssText();
  
  const string rulename = "BackgroundColor";
  
  WCssStyleSheet &style = wApp->styleSheet();
  
  if( color.isDefault() )
  {
    if( m_cssRules.count(rulename) )
      style.removeRule( m_cssRules[rulename] );
  }//if( color.isDefault() )
  
  m_cssRules[rulename] = style.addRule( "#chartarea" + id(), "fill: " + c );
}

void D3SpectrumDisplayDiv::setDefaultPeakColor( const Wt::WColor &color )
{
  m_defaultPeakColor = color.isDefault() ? WColor(0,51,255,155) : color;
  
  //The default peak color is specified as part of the foreground JSON, so we
  //  need to reload the foreground to client to update the color.
  scheduleUpdateForeground();
}


void D3SpectrumDisplayDiv::showXAxisSliderChart( const bool show )
{
  if( m_showXAxisSliderChart == show )
    return;
  
  m_showXAxisSliderChart = show;
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setShowXAxisSliderChart(" + jsbool(show) + ");" );
}//void showXAxisSliderChart( const bool show )


bool D3SpectrumDisplayDiv::xAxisSliderChartIsVisible() const
{
  return m_showXAxisSliderChart;
}//void xAxisSliderChartIsVisible() const;



void D3SpectrumDisplayDiv::showYAxisScalers( const bool show )
{
  if( m_showYAxisScalers == show )
    return;
  
  m_showYAxisScalers = show;
  if( isRendered() )
    doJavaScript( m_jsgraph + ".setShowSpectrumScaleFactorWidget(" + jsbool(show) + ");" );
}//void showXAxisSliderChart( const bool show )


bool D3SpectrumDisplayDiv::yAxisScalersIsVisible() const
{
  return m_showYAxisScalers;
}//void xAxisSliderChartIsVisible() const;


void D3SpectrumDisplayDiv::chartShiftKeyDragCallback( double x0, double x1 )
{
  //cout << "chartShiftKeyDragCallback" << endl;
  m_shiftKeyDragg.emit( x0, x1 );
}//void D3SpectrumDisplayDiv::chartShiftKeyDragCallback(...)

void D3SpectrumDisplayDiv::chartShiftAltKeyDragCallback( double x0, double x1 )
{
  //cout << "chartShiftAltKeyDragCallback" << endl;
  m_shiftAltKeyDragg.emit( x0, x1 );
}//void D3SpectrumDisplayDiv::chartShiftAltKeyDragCallback(...)

void D3SpectrumDisplayDiv::chartRightMouseDragCallback( double x0, double x1 )
{
  //cout << "chartRightMouseDragCallback" << endl;
  m_rightMouseDragg.emit( x0, x1 );
}//void D3SpectrumDisplayDiv::chartRightMouseDragCallback(...)

void D3SpectrumDisplayDiv::chartLeftClickCallback( double x, double y, int pageX, int pageY )
{
  //cout << "chartLeftClickCallback" << endl;
  m_leftClick.emit( x, y, pageX, pageY );
}//void D3SpectrumDisplayDiv::chartDoubleLeftClickCallback(...)

void D3SpectrumDisplayDiv::chartDoubleLeftClickCallback( double x, double y )
{
  //cout << "chartDoubleLeftClickCallback" << endl;
  m_doubleLeftClick.emit( x, y );
}//void D3SpectrumDisplayDiv::chartDoubleLeftClickCallback(...)

void D3SpectrumDisplayDiv::chartRightClickCallback( double x, double y, int pageX, int pageY )
{
  //cout << "chartRightClickCallback" << endl;
  m_rightClick.emit( x, y, pageX, pageY );
}//void D3SpectrumDisplayDiv::chartRightClickCallback(...)



void D3SpectrumDisplayDiv::yAxisScaled( const double scale, const std::string &spectrum )
{
  SpecUtils::SpectrumType type;

  //Dont call D3SpectrumDisplayDiv::setDisplayScaleFactor(...) since we dont
  //  have to re-load data to client, but we should keep all the c++ up to date.
  
  if( spectrum == "FOREGROUND" )
  {
    type = SpecUtils::SpectrumType::Foreground;
  }else if( spectrum == "BACKGROUND" )
  {
    type = SpecUtils::SpectrumType::Background;
    m_backgroundScale = scale;
  }else if( spectrum == "SECONDARY" )
  {
    type = SpecUtils::SpectrumType::SecondForeground;
    m_secondaryScale = scale;
  }else
  {
    cerr << "Received yscaled signal with scale " << scale << " and spectrum = '"
    << spectrum << "', which is invalid" << endl;
    return;
  }
  
  m_yAxisScaled.emit(scale,type);
}//void yAxisScaled( const double scale, const std::string &spectrum )



void D3SpectrumDisplayDiv::chartXRangeChangedCallback( double x0, double x1, double chart_width_px, double chart_height_px )
{
  if( fabs(m_xAxisMinimum-x0)<0.0001 && fabs(m_xAxisMaximum-x1)<0.0001
      && fabs(m_chartWidthPx-chart_width_px)<0.0001 && fabs(m_chartHeightPx-chart_height_px)<0.0001 )
  {
    //cout << "No appreciable change in x-range or chart pixel, not emitting" << endl;
    return;
  }
  
  //cout << "chartXRangeChangedCallback{" << x0 << "," << x1 << "," << chart_width_px << "," << chart_height_px << "}" << endl;
  m_xAxisMinimum = x0;
  m_xAxisMaximum = x1;
  m_chartWidthPx = chart_width_px;
  m_chartHeightPx = chart_height_px;
  
  m_xRangeChanged.emit( x0, x1 );
}//void D3SpectrumDisplayDiv::chartXRangeChangedCallback(...)

D3SpectrumDisplayDiv::~D3SpectrumDisplayDiv()
{
  //doJavaScript( "try{" + m_jsgraph + "=null;}catch(){}" );
}//~D3SpectrumDisplayDiv()


void D3SpectrumDisplayDiv::resetLegendPosition()
{
  const string js = "setTimeout( function(){ try{"
    "let w = d3.select('#" + id() + " > svg > g').attr('width');"
    "let lw = d3.select('#" + id() + " .legend')[0][0].getBoundingClientRect().width;"
    "let x = Math.max(0, w - lw - 15);"
    "d3.select('#" + id() + " .legend').attr('transform','translate(' + x + ',15)');"
  "}catch(e){"
    "console.log( 'Error setting legend pos: ' + e );"
  "} }, 0 );";
  
  if( isRendered() )
    doJavaScript( js );
  else
    m_pendingJs.push_back( js );
}
