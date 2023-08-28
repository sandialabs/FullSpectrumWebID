#ifndef D3SpectrumDisplayDiv_h
#define D3SpectrumDisplayDiv_h

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

#include "FullSpectrumId_config.h"

#include <map>
#include <memory>
#include <vector>
#include <utility>

#include <Wt/WColor.h>
#include <Wt/WEvent.h>
#include <Wt/WSignal.h>
#include <Wt/WContainerWidget.h>

#include "SpecUtils/SpecFile.h"

//Forward declarations
namespace Wt
{
  class WCssTextRule;
}//namespace Wt
namespace SpecUtils{ class Measurement; }
namespace SpecUtils{ enum class SpectrumType : int; }

/** Taken from InterSpec 20201223, and then converted to work with Wt 4.4.0 and a bunch of un-needed stuff removed. */

class D3SpectrumDisplayDiv : public Wt::WContainerWidget
{
public:
  D3SpectrumDisplayDiv();
  virtual ~D3SpectrumDisplayDiv();
  
  
  // A Hack for FullSpectrum where legend seems to always be on the left hand side of the chart
  void resetLegendPosition();
  
  //setTextInMiddleOfChart(...): draws some large text over the middle of the
  //  chart - used int the spectrum quizzer for text based questions.
  void setTextInMiddleOfChart( const Wt::WString &s );
  
  //setCompactAxis(): whether to slim down axis for small displays (e.g. on
  //  phone).  Note that effects wont be seen until next time chart is rendered.
  //  You should also adjust padding axis title text appropriately; x-axis
  //  padding of 23px seems to be a reasonable value.
  //Currently only effects x-axis.
  void setCompactAxis( const bool compact );
  bool isAxisCompacted() const;
  
  Wt::Signal<double/*keV*/,double/*counts*/,int/*pageX*/,int/*pageY*/> &chartClicked();
  Wt::Signal<double/*kev*/,double/*counts*/,int/*pageX*/,int/*pageY*/> &rightClicked();
  Wt::Signal<double/*keV*/,double/*counts*/> &doubleLeftClick();
  Wt::Signal<double/*keV start*/,double/*keV end*/> &controlKeyDragged();
  Wt::Signal<double/*keV start*/,double/*keV end*/> &shiftKeyDragged();
  
  Wt::Signal<double /*new roi lower energy*/,
             double /*new roi upper energy*/,
             double /*new roi lower px*/,
             double /*new roi upper px*/,
             double /*original roi lower energy*/,
             bool /*isFinalRange*/> &roiDragUpdate();
  
  Wt::Signal<double /*lower energy*/,
             double /*upper energy*/,
             int    /*num peaks to force*/,
             bool /*isFinalRange*/> &fitRoiDragUpdate();
  
  Wt::Signal<double,SpecUtils::SpectrumType> &yAxisScaled();
  
  void setData( std::shared_ptr<SpecUtils::Measurement> data_hist );
  void setSecondData( std::shared_ptr<SpecUtils::Measurement> hist );
  void setBackground( std::shared_ptr<SpecUtils::Measurement> background );
  
  void scheduleUpdateForeground();
  void scheduleUpdateBackground();
  void scheduleUpdateSecondData();

  
  void setForegroundSpectrumColor( const Wt::WColor &color );
  void setBackgroundSpectrumColor( const Wt::WColor &color );
  void setSecondarySpectrumColor( const Wt::WColor &color );
  void setTextColor( const Wt::WColor &color );
  void setAxisLineColor( const Wt::WColor &color );
  void setChartMarginColor( const Wt::WColor &color );
  void setChartBackgroundColor( const Wt::WColor &color );
  void setDefaultPeakColor( const Wt::WColor &color );
  
  
  // These 8 functions retrieve the corresponding info from the model.
  std::shared_ptr<SpecUtils::Measurement> data();
  std::shared_ptr<const SpecUtils::Measurement> data()       const;
  std::shared_ptr<SpecUtils::Measurement> secondData();
  std::shared_ptr<const SpecUtils::Measurement> secondData() const;
  std::shared_ptr<SpecUtils::Measurement> background();
  std::shared_ptr<const SpecUtils::Measurement> background() const;
  
  float foregroundLiveTime() const;
  float foregroundRealTime() const;
  
  float backgroundLiveTime() const;
  float backgroundRealTime() const;
  
  float secondForegroundLiveTime() const;
  float secondForegroundRealTime() const;
  
  //displayScaleFactor():  This is the multiple
  float displayScaleFactor( const SpecUtils::SpectrumType spectrum_type ) const;
  

  //setDisplayScaleFactor(): set the effective live time of 'spectrum_type'
  //  to be 'sf' timess the live time of 'spectrum_type'.
  void setDisplayScaleFactor( const float sf,
                             const SpecUtils::SpectrumType spectrum_type );
  
  
  void visibleRange( double &xmin, double &xmax,
                    double &ymin, double &ymax ) const;
  
  virtual void setXAxisTitle( const std::string &title );
  virtual void setYAxisTitle( const std::string &title );
  
  const std::string xAxisTitle() const;
  const std::string yAxisTitle() const;

  
  void enableLegend();
  void disableLegend();
  bool legendIsEnabled() const;
  
  Wt::Signal<> &legendEnabled();
  Wt::Signal<> &legendDisabled();
  
  void showHistogramIntegralsInLegend( const bool show );

  
  Wt::Signal<double,double> &xRangeChanged();
  Wt::Signal<double,double> &rightMouseDragg();
  
  Wt::Signal<double,double> &shiftAltKeyDragged();


  //By default SpectrumDisplayDiv has setLayoutSizeAware(true) set, so if the
  //  widget is being sized by a Wt layout manager, layoutWidth() and
  //  layoutHeight() will return this widget width and height respectively
  int layoutWidth() const;
  int layoutHeight() const;
  
  //For the case of auto-ranging x-axis, the below _may_ return 0 when auto
  //  range is set, but chart hasnt been rendered  (although maybe +-DBL_MAX)
  double xAxisMinimum() const;
  double xAxisMaximum() const;
  
  double chartWidthInPixels() const;
  double chartHeightInPixels() const;
  
  double yAxisMinimum() const;
  double yAxisMaximum() const;
  
  bool yAxisIsLog() const;
  void setYAxisLog( bool log );
  
  void showGridLines( bool show );  //shows horizantal and vertical
  void showVerticalLines( const bool draw );
  void showHorizontalLines( const bool draw );
  bool verticalLinesShowing() const;  // Added by christian (20170425)
  bool horizontalLinesShowing() const;
  
  bool backgroundSubtract() const;
  void setBackgroundSubtract( bool subtract );
  
  void showXAxisSliderChart( const bool show );
  bool xAxisSliderChartIsVisible() const;
  
  void showYAxisScalers( const bool show );
  bool yAxisScalersIsVisible() const;
  
  void setXAxisMinimum( const double minimum );
  void setXAxisMaximum( const double maximum );
  void setXAxisRange( const double minimum, const double maximum );
  
  void setYAxisMinimum( const double minimum );
  void setYAxisMaximum( const double maximum );
  void setYAxisRange( const double minimum, const double maximum );
  
protected:

  //updates the data JSON for the D3 spectrum on the JS side
  void renderForegroundToClient();
  void renderBackgroundToClient();
  void renderSecondDataToClient();
  
  
  void defineJavaScript();
  
  void initUserTools();
  
  /** In order to change some chart colors after intial load, we have to use
      WCssTextRule's, which dont seem to overide the text css files loaded, and
      using our own WCssStyleSheet (instead of WApplications) didnt seem to work
      out super easily on first try...
   */
  void initChangeableCssRules();
  
  void doBackgroundLiveTimeNormalization();
  void doSecondaryLiveTimeNormalization();
  
  
  //layoutSizeChanged(...): adjusts display binning if necessary
  virtual void layoutSizeChanged ( int width, int height );
  
  virtual void render( Wt::WFlags<Wt::RenderFlag> flags );
  
  /** Flags */
  enum D3RenderActions
  {
    UpdateForegroundSpectrum = 0x02,
    UpdateBackgroundSpectrum = 0x04,
    UpdateSecondarySpectrum = 0x08,
    
    ResetXDomain = 0x10
    
    //ToDo: maybe add a few other things to this mechanism.
  };//enum D3RenderActions
  
  Wt::WFlags<D3RenderActions> m_renderFlags;
  
  int m_layoutWidth;
  int m_layoutHeight;
  
  std::shared_ptr<SpecUtils::Measurement> m_foreground;
  std::shared_ptr<SpecUtils::Measurement> m_secondary;
  std::shared_ptr<SpecUtils::Measurement> m_background;
  float m_secondaryScale;
  float m_backgroundScale;
  
  bool m_compactAxis;
  bool m_legendEnabled;
  bool m_yAxisIsLog;
  bool m_backgroundSubtract;
  
  bool m_showVerticalLines;
  bool m_showHorizontalLines;
  bool m_showHistogramIntegralsInLegend;  //Not currently used/implemented
  
  bool m_showXAxisSliderChart;
  bool m_showYAxisScalers;
  
  std::string m_xAxisTitle;
  std::string m_yAxisTitle;
  
  // JSignals
  //for all the bellow, the doubles are all the <x,y> coordinated of the action
  //  where x is in energy, and y is in counts.
  std::unique_ptr<Wt::JSignal<double, double> > m_shiftKeyDraggJS;
  std::unique_ptr<Wt::JSignal<double, double> > m_shiftAltKeyDraggJS;
  std::unique_ptr<Wt::JSignal<double, double> > m_rightMouseDraggJS;
  std::unique_ptr<Wt::JSignal<double, double> > m_doubleLeftClickJS;
  std::unique_ptr<Wt::JSignal<double,double,int/*pageX*/,int/*pageY*/> > m_leftClickJS;
  std::unique_ptr<Wt::JSignal<double,double,int/*pageX*/,int/*pageY*/> > m_rightClickJS;
  /** Currently including chart area in pixels in xRange changed from JS; this
      size in pixels is only approximate, since chart may not have been totally layed out
      and rendered when this signal was emmitted.
   ToDo: Should create dedicated signals for chart size in pixel, and also Y-range.
   */
  std::unique_ptr<Wt::JSignal<double,double,double,double> > m_xRangeChangedJS;
  std::unique_ptr<Wt::JSignal<double,double,double,double,double,bool> > m_roiDraggedJS;
  std::unique_ptr<Wt::JSignal<double,double,int,bool,double,double> > m_fitRoiDragJS;
  std::unique_ptr<Wt::JSignal<double,std::string> > m_yAxisDraggedJS;
  
  std::unique_ptr<Wt::JSignal<> > m_legendClosedJS;
  
  // Wt Signals
  //for all the bellow, the doubles are all the <x,y> coordinated of the action
  //  where x is in energy, and y is in counts.
  Wt::Signal<> m_legendEnabledSignal;
  Wt::Signal<> m_legendDisabledSignal;
  Wt::Signal<double/*xlow*/,double/*xhigh*/> m_xRangeChanged;
  Wt::Signal<double,double> m_controlKeyDragg;
  Wt::Signal<double,double> m_shiftKeyDragg;
  Wt::Signal<double,double> m_shiftAltKeyDragg;
  Wt::Signal<double,double> m_rightMouseDragg;
  Wt::Signal<double,double,int/*pageX*/,int/*pageY*/> m_leftClick;
  Wt::Signal<double,double> m_doubleLeftClick;
  Wt::Signal<double,double,int/*pageX*/,int/*pageY*/> m_rightClick;
  
  Wt::Signal<double /*new roi lower energy*/,
             double /*new roi upper energy*/,
             double /*new roi lower px*/,
             double /*new roi upper px*/,
             double /*original roi lower energy*/,
             bool /*isFinalRange*/> m_roiDrag;
  
  Wt::Signal<double /*lower energy*/,
             double /*upper energy*/,
             int /*force n peaks*/,
             bool /*isFinalRange*/> m_fitRoiDrag;
  
  Wt::Signal<double,SpecUtils::SpectrumType> m_yAxisScaled;
  
  // Signal Callbacks
  void chartShiftKeyDragCallback( double x0, double x1 );
  void chartShiftAltKeyDragCallback( double x0, double x1 );
  void chartRightMouseDragCallback( double x0, double x1 );
  void chartLeftClickCallback( double x, double y, int pageX, int pageY );
  void chartDoubleLeftClickCallback( double x, double y );
  void chartRightClickCallback( double x, double y, int pageX, int pageY );
  
  void yAxisScaled( const double scale, const std::string &spectrum );
  
  //chartXRangeChangedCallback(...): rebins the displayed data, and sets the
  //  y-axis to be auto-range
  void chartXRangeChangedCallback( double x, double y, double chart_width_px, double chart_height_px );
  
  /** The javascript variable name used to refer to the SpecrtumChartD3 object.
      Currently is `jsRef() + ".chart"`.
   */
  const std::string m_jsgraph;
  
  // X-axis and Y-axis values
  double m_xAxisMinimum;
  double m_xAxisMaximum;
  double m_yAxisMinimum;
  double m_yAxisMaximum;
  
  /** The width of the plotting area. */
  double m_chartWidthPx;
  double m_chartHeightPx;

  
  Wt::WColor m_foregroundLineColor;
  Wt::WColor m_backgroundLineColor;
  Wt::WColor m_secondaryLineColor;
  Wt::WColor m_textColor;
  Wt::WColor m_axisColor;
  Wt::WColor m_chartMarginColor;
  Wt::WColor m_chartBackgroundColor;
  Wt::WColor m_defaultPeakColor;
  
  std::map<std::string,Wt::WCssTextRule *> m_cssRules;
  
  /** JS calls requested before the widget has been rendered, so wouldnt have
     ended up doing anything are saved here, and then executed once the widget
     is rendered.
     Note that not all calls to the D3 chart before Wt's rendering need to go
     here as they will be options set to the D3 chart during first rendering.
   */
  std::vector<std::string> m_pendingJs;
};//class SpectrumDisplayDiv


#endif
