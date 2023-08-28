#ifndef SimpleDialog_h
#define SimpleDialog_h

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

#include <Wt/WDialog.h>
#include <Wt/WString.h>

namespace Wt
{
  class WPushButton;
  class WContainerWidget;
}//namespace Wt

/** A simple minimal dialog meant to ask the user a question modal question where user should respond before continuing.
   Kinda similar to a iOS dialog asking a question.
 
 Shown centered in the middle of the screen.
 
 Taken from InterSpec and converted to Wt 4 for FUllSpectrum 
 */
class SimpleDialog : public Wt::WDialog
{
public:
  enum SimpleDialogProperties
  {
    // Show modal background
    // Allow clicking on background to dismiss
    // Allow escape key to dismiss
  };//enum SimpleDialogProperties


  SimpleDialog();
  SimpleDialog( const Wt::WString &title );
  SimpleDialog( const Wt::WString &title, const Wt::WString &content );
  ~SimpleDialog();
  
  /** Add a button to the footer.
   
   Buttons are added left-to-right, and clicking on them will cause the dialog to hide and become deleted, so you dont need to worry
   about cleaning up the dialog.
   
   Hookup to the returned button to trigger actions after clicking.
   */
  Wt::WPushButton *addButton( const Wt::WString &txt );
  
protected:
  virtual void render( Wt::WFlags<Wt::RenderFlag> flags );
  
  void init( const Wt::WString &title, const Wt::WString &content );
  
private:
  void startDeleteSelf();
  
  // We dont need to keep a pointer around to the title or message contents right now, but as
  //  the use of this class is still shaking out (as of 20210201), we will for the moment.
  
  /** Holds the title text.
   Will have CSS style class "title".
   
   Will be null if no title text is passed in.
   */
  Wt::WText *m_title;
  
  /** Holds the message contents.
   Will have CSS style class "content".
   
   Will be null if no contents text is passed in.
   */
  Wt::WText *m_msgContents;
};//class SimpleDialog


#endif //SimpleDialog_h

