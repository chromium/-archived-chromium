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

#ifndef CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_
#define CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "base/basictypes.h"
#include "chrome/browser/tab_contents_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "chrome/views/focus_manager.h"
#include "chrome/views/root_view.h"
#include "chrome/views/view_container.h"

class AutomationProvider;
class TabContents;
class Profile;
class TabContentsContainerView;
// This class serves as the container window for an external tab.
// An external tab is a Chrome tab that is meant to displayed in an
// external process. This class provides the FocusManger needed by the
// TabContents as well as an implementation of TabContentsDelagate.
// It also implements ViewContainer
class ExternalTabContainer : public TabContentsDelegate,
                             public NotificationObserver,
                             public ChromeViews::ViewContainer,
                             public ChromeViews::KeystrokeListener,
                             public CWindowImpl<ExternalTabContainer,
                                                CWindow,
                                                CWinTraits<WS_POPUP |
                                                    WS_CLIPCHILDREN>> {
 public:
  BEGIN_MSG_MAP(ExternalTabContainer)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
    MSG_WM_DESTROY(OnDestroy)
  END_MSG_MAP()

  DECLARE_WND_CLASS(chrome::kExternalTabWindowClass)

  ExternalTabContainer(AutomationProvider* automation);
  ~ExternalTabContainer();

  TabContents* tab_contents() const {
    return tab_contents_;
  }

  bool Init(Profile* profile);
  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition,
                              const std::wstring& override_encoding);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void ReplaceContents(TabContents* source, TabContents* new_contents);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void StartDraggingDetachedContents(TabContents* source,
                                             TabContents* new_contents,
                                             const gfx::Rect& contents_bounds,
                                             const gfx::Point& mouse_pt);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);
  virtual void ContentsZoomChange(bool zoom_in);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void DidNavigate(NavigationType nav_type,
                           int relative_navigation_offet);
  virtual void SendExternalHostMessage(const std::string& receiver,
                                       const std::string& message);


  // Notification service callback.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  ////////////////////////////////////////////////////////////////////////////////
  // ChromeViews::ViewContainer
  ////////////////////////////////////////////////////////////////////////////////
  virtual void GetBounds(CRect *out, bool including_frame) const;
  virtual void MoveToFront(bool should_activate);
  virtual HWND GetHWND() const;
  virtual void PaintNow(const CRect& update_rect);
  virtual ChromeViews::RootView* GetRootView();
  virtual bool IsVisible();
  virtual bool IsActive();
  virtual bool GetAccelerator(int cmd_id,
                              ChromeViews::Accelerator* accelerator) {
    return false;
  }

  // ChromeViews::KeystrokeListener implementation
  // This method checks whether this keydown message is needed by the
  // external host. If so, it sends it over to the external host
  virtual bool ProcessKeyDown(HWND window, UINT message, WPARAM wparam,
                                LPARAM lparam);

  // Sets the keyboard accelerators needed by the external host
  void SetAccelerators(HACCEL accel_table, int accel_table_entry_count);

  // This is invoked when the external host reflects back to us a keyboard
  // message it did not process
  void ProcessUnhandledAccelerator(const MSG& msg);

  // A helper method that tests whether the given window is an
  // ExternalTabContainer window
  static bool IsExternalTabContainer(HWND window);

  // A helper method that retrieves the ExternalTabContainer object that
  // hosts the given tab window.
  static ExternalTabContainer* GetContainerForTab(HWND tab_window);

 protected:
  LRESULT OnSize(UINT, WPARAM, LPARAM, BOOL& handled);
  void OnDestroy();
  void OnFinalMessage(HWND window);

 protected:
  TabContents *tab_contents_;
  AutomationProvider* automation_;
  // Root view
  ChromeViews::RootView root_view_;
  // The accelerator table of the external host.
  HACCEL external_accel_table_;
  unsigned int external_accel_entry_count_;
  // A view to handle focus cycling
  TabContentsContainerView* tab_contents_container_;
 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalTabContainer);
};

#endif  // CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H__
