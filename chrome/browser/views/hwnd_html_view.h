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

#ifndef CHROME_BROWSER_VIEWS_HWND_HTML_VIEW_H_
#define CHROME_BROWSER_VIEWS_HWND_HTML_VIEW_H_

#include "googleurl/src/gurl.h"
#include "chrome/views/hwnd_view.h"

class RenderViewHost;
class RenderViewHostDelegate;

// A simple view that wraps a RenderViewHost in an HWNDView to facilitate
// rendering HTML as arbitrary browser views.
// TODO(timsteele): (bug 1317303). This should replace DOMView.
class HWNDHtmlView : public ChromeViews::HWNDView {
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

 protected:
   // View overrides.
   virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

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
