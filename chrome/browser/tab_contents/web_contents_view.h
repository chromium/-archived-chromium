// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_H_
#define CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_H_

#include <windows.h>

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "chrome/browser/render_view_host_delegate.h"

class Browser;
class RenderViewHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class RenderWidgetHostViewWin;  // TODO(brettw) this should not be necessary.
class WebContents;
struct WebDropData;
class WebKeyboardEvent;

namespace base {
class WaitableEvent;
}

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

  virtual WebContents* GetWebContents() = 0;

  virtual void CreateView() = 0;

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

  // Called when the WebContents is being destroyed. This should clean up child
  // windows that are part of the view.
  //
  // TODO(brettw) It seems like this might be able to be done internally as the
  // window is being torn down without input from the WebContents. Try to
  // implement functions that way rather than adding stuff here.
  virtual void OnContentsDestroy() = 0;

  // Sets the page title for the native widgets corresponding to the view. This
  // is not strictly necessary and isn't expected to be displayed anywhere, but
  // can aid certain debugging tools such as Spy++ on Windows where you are
  // trying to find a specific window.
  virtual void SetPageTitle(const std::wstring& title) = 0;

  // Schedules a complete repaint of the window. This is used for cases where
  // the existing contents became invalid due to an external event, such as the
  // renderer crashing.
  virtual void Invalidate() = 0;

  // TODO(brettw) this is a hack. It's used in two places at the time of this
  // writing: (1) when render view hosts switch, we need to size the replaced
  // one to be correct, since it wouldn't have known about sizes that happened
  // while it was hidden; (2) in constrained windows.
  //
  // (1) will be fixed once interstitials are cleaned up. (2) seems like it
  // should be cleaned up or done some other way, since this works for normal
  // TabContents without the special code.
  virtual void SizeContents(const gfx::Size& size) = 0;

  // Invoked from the platform dependent web contents view when a
  // RenderWidgetHost is deleted. Removes |host| from internal maps.
  void RenderWidgetHostDestroyed(RenderWidgetHost* host);

  // Find in page --------------------------------------------------------------

  // Opens the find in page window if it isn't already open. It will advance to
  // the next match if |find_next| is set and there is a search string,
  // otherwise, the find window will merely be opened. |forward_direction|
  // indicates the direction to search when find_next is set, otherwise it is
  // ignored.
  virtual void FindInPage(const Browser& browser,
                          bool find_next, bool forward_direction) = 0;

  // Hides the find bar if there is one shown. Does nothing otherwise. The find
  // bar will not be deleted, merely hidden. This ensures that any search terms
  // are preserved if the user subsequently opens the find bar.
  //
  // If |end_session| is true, then the find session will be ended, which
  // indicates the user requested they no longer be in find mode for that tab.
  // The find bar will not be restored when we switch back to the tab.
  // Otherwise, we assume that the find bar is being hidden because the tab is
  // being hidden, and all state like visibility and tickmarks will be restored
  // when the tab comes back.
  virtual void HideFindBar(bool end_session) = 0;

  // Called when the tab is reparented to a new browser window. On MS Windows,
  // we have to change the parent of our find bar to go with the new window.
  //
  // TODO(brettw) this seems like it could be improved. Possibly all doohickies
  // around the tab like this, the download bar etc. should be managed by the
  // BrowserView2 object.
  virtual void ReparentFindWindow(Browser* new_browser) const = 0;

  // Computes the location of the find bar and whether it is fully visible in
  // its parent window. The return value indicates if the window is visible at
  // all. Both out arguments are required.
  //
  // This is used for UI tests of the find bar. If the find bar is not currently
  // shown (return value of false), the out params will be {(0, 0), false}.
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible) const = 0;

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
  virtual WebContents* CreateNewWindowInternal
      (int route_id, base::WaitableEvent* modal_dialog_event) = 0;
  virtual RenderWidgetHostView* CreateNewWidgetInternal(int route_id,
                                                        bool activatable) = 0;
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
  virtual void CreateNewWindow(int route_id,
                               base::WaitableEvent* modal_dialog_event);
  virtual void CreateNewWidget(int route_id, bool activatable);
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

#endif  // CHROME_BROWSER_TAB_CONTENTS_WEB_CONTENTS_VIEW_H_
