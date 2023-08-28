#ifndef D3TimeChart_h
#define D3TimeChart_h

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

/**
 Taken from the 'feature/D3TimeHistory_davlee' branch of InterSpec 20210227, git hash 116f67d16fb99fbfdd07067cd0e1614d593bce49
 */


//Forward declarations
namespace Wt
{
  class WCssTextRule;
}//namespace Wt

namespace SpecUtils
{
  class SpecFile;
  class Measurement;
  enum class SpectrumType : int;
}//namespace SpecUtils


/**
 Things to handle:
 - Display time series of gamma + neutron gross-count data.
 - Neutron and gamma use diff y-axis (neutron axis should disapear if no neutron info avalable).
 - Handle case where background is like ~5 minutes long, but item of interest is only a few seconds.
 - Option to have x-axis either be real time (fixing up issue with previous point), or have each
   sample take up the same number of pixels.
 - Handle case where there are more time-segments than pixels.
 - Indicate time segments being used for foreground, background, secondary spectrum (e.g., yellow,
   blue, etc fill).  Segments may not be continuos.
 - Indicate where vehicle occupancy begins and ends, and area designated as background or intrinsic.
 - Handle allowing user to select time regions to show spectrum for foreground, background,
   secondary (currently in InterSpec if user holds option when sleecting region, it will be used for
   background).  Regions may be discontinuous, and user may want to add/remove from currently used
   time regions (currently shift key while selecting will add to region, or remove if already
   selected).
 - Use d3.v3.min.js as used by SpecUtils.
 - Touch device compatible (details to be tested/worked out later).
 - Chart may be dynamically resized (e.g., user changes screen size).
 - X and Y axis units should be human-friendly numbers (e.g., time is at {1, 1.5, 2.0,...} and not
   {1.05, 1.55, 2.05, ..}, and similarly for y-axis counts.
 - X-axis should indicate start/end of time interval, not middle of time interval.
 - Should be histogram, not smooth graph.
 - Support displaying counts/time based on mouse position.
 - Make x-axis label compact and/or disapear.
 - Display multiple gamma lines corresponding to detectors in the system (stacked or overlaid); this
   can be ignored at first maybe.
 - Line, axis, background, time highlight region colors settable.
 - Adjust x-axis and y-axis ranges automatatically.  Default to have y-axis always go down to zero,
   and maybe have an option to adjust to minimum data height.
 - Show horizantal and vertical grid lines.
 - Adjustable padding around chart
 - Optional: be able to save as a PNG/JPEG/static-SVG.
 */


class D3TimeChart : public Wt::WContainerWidget
{
public:
  D3TimeChart();
  virtual ~D3TimeChart();
  
  /** Set the spectrum file to display the time history for.
   
   Will remove any existing highlighted intervals.
   */
  void setData( std::shared_ptr<const SpecUtils::SpecFile> data );
  
  
  void setHighlightedIntervals( const std::set<int> &sample_numbers,
                                const SpecUtils::SpectrumType type );
  
  void saveChartToPng( const std::string &filename );

  /** Signal when the user clicks on the chart.
   Gives the sample numebr user clicked on and a bitwise or of Wt::KeyboardModifiers.
   */
  Wt::Signal<int/*sample number*/,Wt::WFlags<Wt::KeyboardModifier>> &chartClicked();
  
  /** When the user drags on the chart to change the time range the spectrum is displayed for. */
  Wt::Signal<int/*start sample number*/,int/*end sample number*/,Wt::WFlags<Wt::KeyboardModifier>> &chartDragged();
  
  /** When the chart is resized; gives new width and height of the chart area (e.g., the area inside
   the axis lines), in pixels.
   */
  Wt::Signal<double/*chart width px*/,double/*chart height px*/> &chartResized();
  
  /**  Signal emitted when the displayed x-axis range changes via a user action; e.g., when zooming
   into or out of a region of interest.
   */
  Wt::Signal<int/*start sample number*/,int/*end sample number*/,int/*samples per channel*/> &displayedXRangeChange();
  
  
  static std::vector<std::pair<int,int>> sampleNumberRangesWithOccupancyStatus(
                                                const SpecUtils::OccupancyStatus status,
                                                std::shared_ptr<const SpecUtils::SpecFile> spec );
  
  /** Schedules (re)-rendering data + highlight regions. */
  void scheduleRenderAll();
  
  /** Schedules rendering the highlight regions. */
  void scheduleHighlightRegionRender();

  
  void setGammaLineColor( const Wt::WColor &color );
  void setNeutronLineColor( const Wt::WColor &color );
  
  void setTextColor( const Wt::WColor &color );
  void setAxisLineColor( const Wt::WColor &color );
  void setChartMarginColor( const Wt::WColor &color );
  void setChartBackgroundColor( const Wt::WColor &color );
  
  void setXAxisTitle( const std::string &title );
  void setY1AxisTitle( const std::string &title );
  void setY2AxisTitle( const std::string &title );
  
  
  //By default SpectrumDisplayDiv has setLayoutSizeAware(true) set, so if the
  //  widget is being sized by a Wt layout manager, layoutWidth() and
  //  layoutHeight() will return this widget width and height respectively
  int layoutWidth() const;
  int layoutHeight() const;
  
  double chartWidthInPixels() const;
  double chartHeightInPixels() const;
  
  void setCompactAxis( const bool compact );
  bool isAxisCompacted() const;
  
  void showGridLines( const bool draw );
  void showVerticalLines( const bool draw );
  void showHorizontalLines( const bool draw );
  bool verticalLinesShowing() const;
  bool horizontalLinesShowing() const;
  
  void setXAxisRangeSamples( const int min_sample_num, const int max_sample_num );
  
  /** Override WWebWidget::doJavaScript() to wait until this widget has been rendered before
   executing javascript so we can be sure all the JS objects we need are created.
   */
  virtual void doJavaScript( const std::string &js );
  
protected:
  
  void defineJavaScript();
  
  /** In order to change some chart colors after intial load, we have to use
      WCssTextRule's, which dont seem to overide the text css files loaded, and
      using our own WCssStyleSheet (instead of WApplications) didnt seem to work
      out super easily on first try...
   */
  void initChangeableCssRules();
  
  void setDataToClient();
  void setHighlightRegionsToClient();
  
  //layoutSizeChanged(...): adjusts display binning if necessary
  virtual void layoutSizeChanged ( int width, int height );
  
  virtual void render( Wt::WFlags<Wt::RenderFlag> flags );
  
  /** Flags */
  enum TimeRenderActions
  {
    UpdateData = 0x01,
    
    UpdateHighlightRegions = 0x02,
    
    //ResetXDomain = 0x10
    
    //ToDo: maybe add a few other things to this mechanism.
  };//enum D3RenderActions
  
  Wt::WFlags<TimeRenderActions> m_renderFlags;
  
  /** The width, in pixels, of this entire widget. */
  int m_layoutWidth;
  /** The height, in pixels, of this entire widget. */
  int m_layoutHeight;
  
  /** The width of the plotting area in pixels. */
  double m_chartWidthPx;
  
  /** The height of the plotting area in pixels. */
  double m_chartHeightPx;
  
  bool m_compactXAxis;
  bool m_showVerticalLines;
  bool m_showHorizontalLines;
  
  
  std::shared_ptr<const SpecUtils::SpecFile> m_spec;
  
  struct HighlightRegion
  {
    int start_sample_number;
    int end_sample_number;
    SpecUtils::SpectrumType type;
    Wt::WColor color;
  };//HighlightRegion
  
  
  std::vector<D3TimeChart::HighlightRegion> m_highlights;
  
  std::string m_xAxisTitle;
  std::string m_y1AxisTitle;
  std::string m_y2AxisTitle;
  
  // Signals to hook C++ code to, to be notified when a user action happens
  Wt::Signal<int/*sample number*/,Wt::WFlags<Wt::KeyboardModifier>> m_chartClicked;
  Wt::Signal<int/*start sample number*/,int/*end sample number*/,Wt::WFlags<Wt::KeyboardModifier>> m_chartDragged;
  Wt::Signal<double/*chart width px*/,double/*chart height px*/> m_chartResized;
  Wt::Signal<int/*start sample number*/,int/*end sample number*/,int/*samples per channel*/> m_displayedXRangeChange;
  
  // Signals called from JS to propogate infromation to the C++
  std::unique_ptr<Wt::JSignal<int,int>>       m_chartClickedJS;
  std::unique_ptr<Wt::JSignal<int,int,int>>   m_chartDraggedJS;
  std::unique_ptr<Wt::JSignal<double,double>> m_chartResizedJS;
  std::unique_ptr<Wt::JSignal<int,int,int>>   m_displayedXRangeChangeJS;
  
  // Functions connected to the JSignal's
  void chartClickedCallback( int sample_number, int modifier_keys );
  void chartDraggedCallback( int first_sample_number, int last_sample_number, int modifier_keys );
  void chartResizedCallback( double chart_width_px, double chart_height_px );
  void displayedXRangeChangeCallback( int first_sample_number, int last_sample_number, int samples_per_channel );
  
  /** The javascript variable name used to refer to the SpecrtumChartD3 object.
      Currently is `jsRef() + ".chart"`.
   */
  const std::string m_jsgraph;

  Wt::WColor m_gammaLineColor;
  Wt::WColor m_neutronLineColor;
  Wt::WColor m_foregroundHighlightColor;
  Wt::WColor m_backgroundHighlightColor;
  Wt::WColor m_secondaryHighlightColor;
  Wt::WColor m_occLineColor;
  Wt::WColor m_textColor;
  Wt::WColor m_axisColor;
  Wt::WColor m_chartMarginColor;
  Wt::WColor m_chartBackgroundColor;
  
  std::map<std::string,Wt::WCssTextRule *> m_cssRules;
  
  /** JS calls requested before the widget has been rendered, so wouldnt have
     ended up doing anything are saved here, and then executed once the widget
     is rendered.
     Note that not all calls to the D3 chart before Wt's rendering need to go
     here as they will be options set to the D3 chart during first rendering.
   */
  std::vector<std::string> m_pendingJs;
};//class D3TimeChart_h


#endif //D3TimeChart_h
