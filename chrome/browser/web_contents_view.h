// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_CONTENTS_VIEW_H_
#define CHROME_BROWSER_WEB_CONTENTS_VIEW_H_

#include <windows.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "chrome/browser/render_view_host_delegate.h"

class InfoBarView;
class RenderViewHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class RenderWidgetHostViewWin;  // TODO(brettw) this should not be necessary.
class WebContents;
struct WebDropData;
class WebKeyboardEvent;

// The WebContentsView is an interface that is implemented by the platform-
// dependent web contents views. The WebContents uses this interface to talk to
// them. View-related messages will also get forwarded directly to this class
// from RenderViewHost via RenderViewHostDelegate::View.
//
// It contains a small amount of logic with respect to creating new sub-view
// that should be the same for all platforms.
class WebContentsView : public RenderViewHostDelegate::View {
 public:
  virtual ~WebContentsView() {}

  virtual void CreateView(HWND parent_hwnd,
                          const gfx::Rect& initial_bounds) = 0;

  // Sets up the View that holds the rendered web page, receives messages for
  // it and contains page plugins.
  // TODO(brettw) make this so we don't need to return the Win version (see the
  // caller in WebContents).
  virtual RenderWidgetHostViewWin* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) = 0;

  // Returns the HWND that contains the contents of the tab.
  // TODO(brettw) this should not be necessary in this cross-platform interface.
  virtual HWND GetContainerHWND() const = 0;

  // Returns the HWND with the main content of the tab (i.e. the main render
  // view host, though there may be many popups in the tab as children of the
  // container HWND).
  // TODO(brettw) this should not be necessary in this cross-platform interface.
  virtual HWND GetContentHWND() const = 0;

  // Computes the rectangle for the native widget that contains the contents of
  // the tab relative to its parent.
  virtual void GetContainerBounds(gfx::Rect *out) const = 0;

  // Helper function for GetContainerBounds. Most callers just want to know the
  // size, and this makes it more clear.
  gfx::Size GetContainerSize() const {
    gfx::Rect rc;
    GetContainerBounds(&rc);
    return gfx::Size(rc.width(), rc.height());
  }

  // Enumerate and 'un-parent' any plugin windows that are children of us.
  virtual void DetachPluginWindows() = 0;

  // Displays the given error in the info bar. A new info bar will be shown if
  // one is not shown already. The new error text will replace any existing
  // text shown by this same function.
  //
  // Note: this replacement behavior is historical; crashed plugin and out of
  // JS memory used the same message. This seems reasonable, but it may not be
  // the best thing for all error messages.
  virtual void DisplayErrorInInfoBar(const std::wstring& text) = 0;

  // Set/get whether or not the info bar is visible. See also the ChromeFrame
  // method InfoBarVisibilityChanged and TabContents::IsInfoBarVisible.
  virtual void SetInfoBarVisible(bool visible) = 0;
  virtual bool IsInfoBarVisible() const = 0;

  // Create the InfoBarView and returns it if none has been created.
  // Just returns existing InfoBarView if it is already created.
  // TODO(brettw) this probably shouldn't be here. There should be methods to
  // tell us what we need to display instead.
  virtual InfoBarView* GetInfoBarView() = 0;

  // Sets the page title for the native widgets corresponding to the view. This
  // is not strictly necessary and isn't expected to be displayed anywhere, but
  // can aid certain debugging tools such as Spy++ on Windows where you are
  // trying to find a specific window.
  virtual void SetPageTitle(const std::wstring& title) = 0;

  // Schedules a complete repaint of the window. This is used for cases where
  // the existing contents became invalid due to an external event, such as the
  // renderer crashing.
  virtual void Invalidate() = 0;

 protected:
  WebContentsView() {}  // Abstract interface.

  // Internal interface for some functions in the RenderViewHostDelegate::View
  // interface. Subclasses should implement this rather than the corresponding
  // ...::View functions directly, since the routing stuff will already be
  // computed. They should implement the rest of the functions as normal.
  //
  // The only difference is that the Create functions return the newly
  // created objects so that they can be associated with the given routes. When
  // they are shown later, we'll look them up again and pass the objects to
  // the Show functions rather than the route ID.
  virtual WebContents* CreateNewWindowInternal(int route_id,
                                               HANDLE modal_dialog_event) = 0;
  virtual RenderWidgetHostView* CreateNewWidgetInternal(int route_id) = 0;
  virtual void ShowCreatedWindowInternal(WebContents* new_web_contents,
                                         WindowOpenDisposition disposition,
                                         const gfx::Rect& initial_pos,
                                         bool user_gesture) = 0;
  virtual void ShowCreatedWidgetInternal(RenderWidgetHostView* widget_host_view,
                                         const gfx::Rect& initial_pos) = 0;

 private:
  // We implement these functions on RenderViewHostDelegate::View directly and
  // do some book-keeping associated with the request. The request is then
  // forwarded to *Internal which does platform-specific work.
  virtual void CreateNewWindow(int route_id, HANDLE modal_dialog_event);
  virtual void CreateNewWidget(int route_id);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id, const gfx::Rect& initial_pos);

  // Tracks created WebContents objects that have not been shown yet. They are
  // identified by the route ID passed to CreateNewWindow.
  typedef std::map<int, WebContents*> PendingContents;
  PendingContents pending_contents_;

  // These maps hold on to the widgets that we created on behalf of the
  // renderer that haven't shown yet.
  typedef std::map<int, RenderWidgetHostView*> PendingWidgetViews;
  PendingWidgetViews pending_widget_views_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsView);
};

#endif  // CHROME_BROWSER_WEB_CONTENTS_VIEW_H_
