#ifndef FullSpectrumApp_h
#define FullSpectrumApp_h

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

#include <memory>
#include <cstdint>

#include <Wt/WDateTime.h>
#include <Wt/WApplication.h>

class AnalysisGui;

class FullSpectrumApp : public Wt::WApplication
{
public:
  FullSpectrumApp( const Wt::WEnvironment &env );
  
#if( ENABLE_SESSION_DETAIL_LOGGING )
  /** Set logging options for user sessions.
   
   @param dir  the base-directory to save session log files and potentially uploaded files to. If empty, do not save log or spectrum files.
   @param save_files whether to save user-uploaded spectrum files.  If true, 'dir' must not be empty.
   */
  static void set_data_directory( const std::string &dir, const bool save_files );
  
  static std::string data_directory();
  static bool save_user_spectrum_files();
#endif
  
protected:
  void showInfoWindow();
  void showContactWindow();
  
  const std::string m_uuid;
  const Wt::WDateTime m_sessionStart;
  AnalysisGui *m_gui;
};//class FullSpectrumApp


#endif //FullSpectrumApp_h
