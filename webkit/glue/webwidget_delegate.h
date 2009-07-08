// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBWIDGET_DELEGATE_H__
#define WEBKIT_GLUE_WEBWIDGET_DELEGATE_H__

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/string16.h"
#include "webkit/glue/window_open_disposition.h"

namespace WebKit {
struct WebCursorInfo;
struct WebRect;
struct WebScreenInfo;
}

class WebWidget;
struct WebPluginGeometry;

struct WebMenuItem {
  // Container for information about entries in an HTML select popup menu.
  // Types must be kept in sync with PopupListBox::ListItemType in
  // WebCore/platform/chromium/PopupMenuChromium.h. This won't change often
  // (if ever).
  enum Type {
    OPTION = 0,
    GROUP,
    SEPARATOR
  };

  string16 label;
  Type type;
  bool enabled;
};

class WebWidgetDelegate {
 public:
  // Returns the view in which the WebWidget is embedded.
  virtual gfx::NativeViewId GetContainingView(WebWidget* webwidget) = 0;

  // Called when a region of the WebWidget needs to be re-painted.
  virtual void DidInvalidateRect(WebWidget* webwidget,
                                 const WebKit::WebRect& rect) = 0;

  // Called when a region of the WebWidget, given by clip_rect, should be
  // scrolled by the specified dx and dy amounts.
  virtual void DidScrollRect(WebWidget* webwidget, int dx, int dy,
                             const WebKit::WebRect& clip_rect) = 0;

  // This method is called to instruct the window containing the WebWidget to
  // show itself as the topmost window.  This method is only used after a
  // successful call to CreateWebWidget.  |disposition| indicates how this new
  // window should be displayed, but generally only means something for
  // WebViews.
  virtual void Show(WebWidget* webwidget,
                    WindowOpenDisposition disposition) = 0;

  // Used for displaying HTML popup menus on Mac OS X (other platforms will use
  // Show() above). |bounds| represents the positioning on the screen (in WebKit
  // coordinates, origin at the top left corner) of the button that will display
  // the menu. It will be used, along with |item_height| (which refers to the
  // size of each entry in the menu), to position the menu on the screen.
  // |selected_index| indicates the menu item that should be drawn as selected
  // when the menu initially is displayed. |items| contains information about
  // each of the entries in the popup menu, such as the type (separator, option,
  // group), the text representation and the item's enabled status.
  virtual void ShowAsPopupWithItems(WebWidget* webwidget,
                                    const WebKit::WebRect& bounds,
                                    int item_height,
                                    int selected_index,
                                    const std::vector<WebMenuItem>& items) = 0;

  // This method is called to instruct the window containing the WebWidget to
  // close.  Note: This method should just be the trigger that causes the
  // WebWidget to eventually close.  It should not actually be destroyed until
  // after this call returns.
  virtual void CloseWidgetSoon(WebWidget* webwidget) = 0;

  // This method is called to focus the window containing the WebWidget so
  // that it receives keyboard events.
  virtual void Focus(WebWidget* webwidget) = 0;

  // This method is called to unfocus the window containing the WebWidget so that
  // it no longer receives keyboard events.
  virtual void Blur(WebWidget* webwidget) = 0;

  virtual void SetCursor(WebWidget* webwidget,
                         const WebKit::WebCursorInfo& cursor) = 0;

  // Returns the rectangle of the WebWidget in screen coordinates.
  virtual void GetWindowRect(WebWidget* webwidget, WebKit::WebRect* rect) = 0;

  // This method is called to re-position the WebWidget on the screen.  The given
  // rect is in screen coordinates.  The implementation may choose to ignore
  // this call or modify the given rect.  This method may be called before Show
  // has been called.
  // TODO(darin): this is more of a request; does this need to take effect
  // synchronously?
  virtual void SetWindowRect(WebWidget* webwidget,
                             const WebKit::WebRect& rect) = 0;

  // Returns the rectangle of the window in which this WebWidget is embeded.
  virtual void GetRootWindowRect(WebWidget* webwidget,
                                 WebKit::WebRect* rect) = 0;

  // Returns the resizer rectangle of the window this WebWidget is in. This
  // is used on Mac to determine if a scrollbar is over the in-window resize
  // area at the bottom right corner.
  virtual void GetRootWindowResizerRect(WebWidget* webwidget,
                                        WebKit::WebRect* rect) = 0;

  // Keeps track of the necessary window move for a plugin window that resulted
  // from a scroll operation.  That way, all plugin windows can be moved at the
  // same time as each other and the page.
  virtual void DidMove(WebWidget* webwidget, const WebPluginGeometry& move) = 0;

  // Suppress input events to other windows, and do not return until the widget
  // is closed.  This is used to support |window.showModalDialog|.
  virtual void RunModal(WebWidget* webwidget) = 0;

  // Owners depend on the delegates living as long as they do, so we ref them.
  virtual void AddRef() = 0;
  virtual void Release() = 0;

  // Returns true if the widget is in a background tab.
  virtual bool IsHidden(WebWidget* webwidget) = 0;

  // Returns information about the screen associated with this widget.
  virtual WebKit::WebScreenInfo GetScreenInfo(WebWidget* webwidget) = 0;

  WebWidgetDelegate() { }
  virtual ~WebWidgetDelegate() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebWidgetDelegate);
};

#endif  // #ifndef WEBKIT_GLUE_WEBWIDGET_DELEGATE_H__
