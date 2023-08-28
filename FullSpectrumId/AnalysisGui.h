#ifndef AnalysisGui_h
#define AnalysisGui_h

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

#if( ENABLE_SESSION_DETAIL_LOGGING )
#include <sstream>
#endif

#include "FullSpectrumId_config.h"

#include <Wt/WLocalDateTime.h>
#include <Wt/WContainerWidget.h>

namespace Wt
{
  class WText;
  class WLabel;
  class WComboBox;
  class WFileUpload;
  class WApplication;
  class WStackedWidget;
}

namespace SpecUtils
{
  class SpecFile;
}

namespace Analysis
{
  struct AnalysisInput;
  struct AnalysisOutput;
}

class D3TimeChart;
class SimpleDialog;
class SampleSelect;
class D3SpectrumDisplayDiv;

class AnalysisGui : public Wt::WContainerWidget
{
public:
#if( ENABLE_SESSION_DETAIL_LOGGING )
  /** AnalysisGui constructor.
   @arg data_base_dir The base-directory to save user uploaded files to.; a unique directory will be created under this directory for this
        session.  If blank or invalid directory, dont save files.
   */
  AnalysisGui( const std::string &data_base_dir, const bool save_spec_files );
#else
  AnalysisGui();
#endif
  
private:
  
  void initSpectrumChart();
  void initTimeChart();
  
  bool synthesizingBackground() const;
  
  enum class SpecUploadType
  {
    Foreground,
    Background
  };
  
  /** Function called when a new foreground or background file is uploaded. */
  void fileUploaded( const SpecUploadType type );
  
  void fileUploadWorker( const SpecUploadType type, SimpleDialog *dialog, Wt::WApplication *app );
  
  /** Callback for when the user tries to upload a file larger than allowed. */
  void uploadToLarge( const int64_t fileSize, const SpecUploadType type );
  
  void showBackgroundUpload();
  void showBackgroundBeingSynthesized();
  
  void checkInputState();
  void drfSelectionChanged();
  void sampleNumberToUseChanged();
  
  void anaResultCallback( const Analysis::AnalysisInput &input,
                          const Analysis::AnalysisOutput &output );
  
  Wt::WLabel *m_foreUploadLabel;
  Wt::WFileUpload *m_foregroundUpload;
  SampleSelect *m_foreSelectForeSample;
  SampleSelect *m_foreSelectBackSample;
  
  Wt::WLabel *m_backUploadLabel;
  Wt::WStackedWidget *m_backgroundUploadStack;
  Wt::WContainerWidget *m_backgroundUploadHolder;
  Wt::WContainerWidget *m_synthBackgroundHolder;
  SampleSelect *m_backSelectBackSample;
  Wt::WFileUpload *m_backgroundUpload;
  Wt::WLabel *m_drfSelectorLabel;
  Wt::WComboBox *m_drfSelector;
  Wt::WText *m_drfWarning;
  
  /** Displays instructions to the users that change with each step. */
  Wt::WText *m_instructions;
  
  /** Display error message whenever file is uploaded.  When a different file is uploaded the message will be cleared out, and if any
   error, then updated.
   */
  Wt::WText *m_parseError;
  /** Displays the analysis
   
   */
  Wt::WText *m_result;
  Wt::WText *m_analysisError;
  Wt::WText *m_analysisWarning;
  Wt::WContainerWidget *m_chartHolder;
  
  std::shared_ptr<SpecUtils::SpecFile> m_foreground;
  std::shared_ptr<SpecUtils::SpecFile> m_background;
  
  size_t m_ana_number;
  
  D3SpectrumDisplayDiv *m_chart;
  D3TimeChart *m_timeline;
  
  // Track number of user uploads, so if too many invalid spectrum files are uploaded, we can
  //  just quit the session.
  size_t m_numUploadsTotal;
  size_t m_numUploadsParsed;
  size_t m_numBytesUploaded;
  
  
  /** We will define a class to log user actions with hopefully enough detail to to answer any support questions users may ask.
   
   The entries are XML formatted.
     
   If compile-time option ENABLE_SESSION_DETAIL_LOGGING is enabled, creating a log entry will also cause the session directory
   to be created, so please wait until user makes some real action before creating this entry.
   
   Contents of entry are placed both in the Wt log file, as well a 'user_action_log.xml' in the session specific directory (if enabled).
   */
  class UserActionLogEntry : public std::stringstream
  {
  public:
    UserActionLogEntry( const std::string &tag, AnalysisGui *gui );
    ~UserActionLogEntry();
    
    const std::string m_tag;
#if( ENABLE_SESSION_DETAIL_LOGGING )
    const std::string m_dir;
#endif
  };//class UserActionLogEntry
  
  friend class UserActionLogEntry;

#if( ENABLE_SESSION_DETAIL_LOGGING )
  /** Function that will check if the directory to store user-uploaded data has been made, and if not make it.
   
   @returns If you should save user uploaded data (base directory is valid, and could create session specific directory) into
            m_data_dir.  Will return false if base directory was not set, or couldnt make specific 
   */
  bool check_session_data_dir();
  
  /** The server-local time time this class was initiated.  Used to create the session-specific data-directory. */
  const Wt::WLocalDateTime m_startTime;
  
  /** Data base-directory passed into the constructor of this class.
   If this turns out to be an invalid directory, or the session specific sub-directory couldn't be made, this will be set to an empty string
   by the #check_session_data_dir function.
   */
  std::string m_data_base_dir;
  
  /** The session-specific directory to log user input and save files to.  Will be empty until the directory is actually created. */
  std::string m_data_dir;
  
  /** Whether to save user uploaded spectrum files. */
  bool m_save_spectrum_files;
  
  /** Variable to number the user-uploaded files. */
  size_t m_uploadedFileNumber;
#endif
};//class AnalysisGui

#endif
