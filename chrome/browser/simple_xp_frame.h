// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_SIMPLE_XP_FRAME_H__
#define CHROME_BROWSER_SIMPLE_XP_FRAME_H__

#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/tab_icon_view.h"
#include "chrome/browser/xp_frame.h"
#include "chrome/views/menu_button.h"
#include "chrome/views/view_menu_delegate.h"

class SimpleXPFrameTitleBar;
class WebAppIconManager;

namespace ChromeViews {
class Label;
}

////////////////////////////////////////////////////////////////////////////////
//
// A simple frame that contains a browser object. This frame doesn't show any
// tab. It is used for web applications. It will likely be used in the future
// for detached popups.
//
////////////////////////////////////////////////////////////////////////////////
class SimpleXPFrame : public XPFrame,
                      public LocationBarView::Delegate {
 public:
  // Invoked by ChromeFrame::CreateChromeFrame to create a new SimpleXPFrame.
  // An empty |bounds| means that Windows should decide where to place the
  // window.
  static SimpleXPFrame* CreateFrame(const gfx::Rect& bounds, Browser* browser);

  virtual ~SimpleXPFrame();

  // Overridden from XPFrame.
  virtual void Init();
  virtual void Layout();
  virtual bool IsTabStripVisible() const { return false; }
  virtual bool IsToolBarVisible() const { return false; }
  virtual bool SupportsBookmarkBar() const { return false; }
#ifdef CHROME_PERSONALIZATION
  virtual bool PersonalizationEnabled() const { return false; }
#endif
  virtual LRESULT OnNCHitTest(const CPoint& pt);
  virtual void SetWindowTitle(const std::wstring& title);
  virtual void ValidateThrobber();
  virtual void ShowTabContents(TabContents* selected_contents);
  virtual void UpdateTitleBar();

  // Return the currently visible contents.
  TabContents* GetCurrentContents();

  void RunMenu(const CPoint& pt, HWND hwnd);

  // Returns true if this popup contains a popup which has been created
  // by a browser with a minimal chrome.
  bool IsApplication() const;

  // LocationBarView delegate.
  virtual TabContents* GetTabContents();
  virtual void OnInputInProgress(bool in_progress);

 protected:
  explicit SimpleXPFrame(Browser* browser);

  // The default implementation has a title bar. Override if not needed.
  virtual bool IsTitleBarVisible() { return true; }

  // Overriden to create the WebAppIconManager, then invoke super.
  virtual void InitAfterHWNDCreated();

 private:
  // Set the current window icon. Use NULL for a default icon.
  void SetCurrentIcon(HICON icon);

  // Update the location bar if it is visible.
  void UpdateLocationBar();

  // The simple frame title bar including favicon, menu and title.
  SimpleXPFrameTitleBar* title_bar_;

  // The optional URL field.
  //SimplifiedURLField* url_field_;
  LocationBarView* location_bar_;

  // Handles the icon for web apps.
  scoped_ptr<WebAppIconManager> icon_manager_;

  DISALLOW_EVIL_CONSTRUCTORS(SimpleXPFrame);
};

class SimpleXPFrameTitleBar;

////////////////////////////////////////////////////////////////////////////////
//
// A custom menu button for the custom title bar.
//
////////////////////////////////////////////////////////////////////////////////
class TitleBarMenuButton : public ChromeViews::MenuButton {
 public:
  explicit TitleBarMenuButton(SimpleXPFrameTitleBar* title_bar);
  virtual ~TitleBarMenuButton();

  // Set the contents view which is the view presenting the menu icon.
  void SetContents(ChromeViews::View* contents);

  // overridden from View
  virtual void GetPreferredSize(CSize *out);
  virtual void Paint(ChromeCanvas* canvas);
  virtual bool OnMousePressed(const ChromeViews::MouseEvent& e);

 private:
  // The drop arrow icon.
  SkBitmap* drop_arrow_;

  // The contents is an additional view positioned before the drop down.
  ChromeViews::View* contents_;

  // The title bar that created this instance.
  SimpleXPFrameTitleBar* title_bar_;

  DISALLOW_EVIL_CONSTRUCTORS(TitleBarMenuButton);
};

////////////////////////////////////////////////////////////////////////////////
//
// Custom title bar.
//
////////////////////////////////////////////////////////////////////////////////
class SimpleXPFrameTitleBar : public ChromeViews::View,
                              public TabIconView::TabContentsProvider,
                              public ChromeViews::ViewMenuDelegate {
 public:
  explicit SimpleXPFrameTitleBar(SimpleXPFrame* parent);
  virtual ~SimpleXPFrameTitleBar();

  // Overriden from TabIconView::TabContentsProvider:
  virtual TabContents* GetCurrentTabContents();
  virtual SkBitmap GetFavIcon();

  virtual void RunMenu(ChromeViews::View* source, const CPoint& pt, HWND hwnd);
  virtual void Layout();
  bool WillHandleMouseEvent(int x, int y);
  void SetWindowTitle(std::wstring s);
  void ValidateThrobber();
  void CloseWindow();

  // Updates the state of the tab icon.
  void Update();

  TabIconView* tab_icon_view() const { return tab_icon_.get(); }

 private:
  // The menu button.
  TitleBarMenuButton* menu_button_;

  // The tab icon.
  scoped_ptr<TabIconView> tab_icon_;

  // The corresponding SimpleXPFrame.
  SimpleXPFrame* parent_;

  // The window title.
  ChromeViews::Label* label_;

  // Lazily created chrome icon. Created and used as the icon in the
  // TabIconView for all non-Application windows.
  SkBitmap* chrome_icon_;

  DISALLOW_EVIL_CONSTRUCTORS(SimpleXPFrameTitleBar);
};

#endif  // CHROME_BROWSER_SIMPLE_XP_FRAME_H__
