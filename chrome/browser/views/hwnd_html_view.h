// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_HWND_HTML_VIEW_H_
#define CHROME_BROWSER_VIEWS_HWND_HTML_VIEW_H_

#include "googleurl/src/gurl.h"
#include "chrome/views/hwnd_view.h"

class RenderViewHost;
class RenderViewHostDelegate;

// A simple view that wraps a RenderViewHost in an HWNDView to facilitate
// rendering HTML as arbitrary browser views.
// TODO(timsteele): (bug 1317303). This should replace DOMView.
class HWNDHtmlView : public views::HWNDView {
 public:
  HWNDHtmlView(const GURL& content_url, RenderViewHostDelegate* delegate,
               bool allow_dom_ui_bindings)
      : render_view_host_(NULL),
        content_url_(content_url),
        allow_dom_ui_bindings_(allow_dom_ui_bindings),
        delegate_(delegate),
        initialized_(false) {
  }
  virtual ~HWNDHtmlView();

  RenderViewHost* render_view_host() { return render_view_host_; }

  // Initialize the view without a parent window.  Used for extensions that
  // don't display UI.
  void InitHidden();

 protected:
  // View overrides.
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

  // Called just before we create the RenderView, to give subclasses an
  // opportunity to do some setup.
  virtual void CreatingRenderer() {}

 private:
  // Initialize the view, parented to |parent|, and show it.
  void Init(HWND parent);

  // The URL of the HTML content to render and show in this view.
  GURL content_url_;

  // Our HTML rendering component.
  RenderViewHost* render_view_host_;

  // Whether or not the rendered content is permitted to send messages back to
  // the view, through |delegate_| via ProcessDOMUIMessage.
  bool allow_dom_ui_bindings_;

  // True after Init() has completed.
  bool initialized_;

  // The delegate for our render_view_host.
  RenderViewHostDelegate* delegate_;

  DISALLOW_EVIL_CONSTRUCTORS(HWNDHtmlView);
};

#endif  // CHROME_BROWSER_VIEWS_HWND_HTML_VIEW_H_
