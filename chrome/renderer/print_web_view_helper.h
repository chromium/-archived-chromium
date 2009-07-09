// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRINT_WEB_VIEW_DELEGATE_H_
#define CHROME_RENDERER_PRINT_WEB_VIEW_DELEGATE_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "webkit/glue/webview_delegate.h"

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

class RenderView;
class WebView;
struct ViewMsg_Print_Params;
struct ViewMsg_PrintPage_Params;
struct ViewMsg_PrintPages_Params;


// PrintWebViewHelper handles most of the printing grunt work for RenderView.
// We plan on making print asynchronous and that will require copying the DOM
// of the document and creating a new WebView with the contents.
class PrintWebViewHelper : public WebViewDelegate {
 public:
  explicit PrintWebViewHelper(RenderView * render_view)
      : render_view_(render_view),
        user_cancelled_scripted_print_count_(0) {}

  virtual ~PrintWebViewHelper() {}

  void Print(WebFrame* frame, bool script_initiated);

  // Is there a background print in progress?
  bool IsPrinting() {
    return print_web_view_.get() != NULL;
  }

  // Notification when printing is done - signal teardown
  void DidFinishPrinting(bool success);

 protected:
  bool CopyAndPrint(const ViewMsg_PrintPages_Params& params,
                    WebFrame* web_frame);

  // Prints the page listed in |params|.
  void PrintPage(const ViewMsg_PrintPage_Params& params,
                 const gfx::Size& canvas_size,
                 WebFrame* frame);

  // Prints all the pages listed in |params|.
  // It will implicitly revert the document to display CSS media type.
  void PrintPages(const ViewMsg_PrintPages_Params& params, WebFrame* frame);

  // IPC::Message::Sender
  bool Send(IPC::Message* msg);

  int32 routing_id();

  // WebViewDeletegate
  virtual void DidStopLoading(WebView* webview);
  virtual void DidInvalidateRect(WebWidget* webwidget,
                                 const WebKit::WebRect& rect) {}
  virtual void DidScrollRect(WebWidget* webwidget, int dx, int dy,
                             const WebKit::WebRect& clip_rect) {}
  virtual void Show(WebWidget* webwidget, WindowOpenDisposition disposition) {}
  virtual void ShowAsPopupWithItems(WebWidget* webwidget,
                                    const WebKit::WebRect& bounds,
                                    int item_height,
                                    int selected_index,
                                    const std::vector<WebMenuItem>& items) {}
  virtual void CloseWidgetSoon(WebWidget* webwidget) {}
  virtual void Focus(WebWidget* webwidget) {}
  virtual void Blur(WebWidget* webwidget) {}
  virtual void SetCursor(WebWidget* webwidget,
                         const WebKit::WebCursorInfo& cursor) {}
  virtual void GetWindowRect(WebWidget* webwidget, WebKit::WebRect* rect);
  virtual void SetWindowRect(WebWidget* webwidget,
                             const WebKit::WebRect& rect) {}
  virtual void GetRootWindowRect(WebWidget* webwidget, WebKit::WebRect* rect) {}
  virtual void GetRootWindowResizerRect(WebWidget* webwidget,
                                        WebKit::WebRect* rect) {}
  virtual void DidMove(WebWidget* webwidget, const WebPluginGeometry& move) {}
  virtual void RunModal(WebWidget* webwidget) {}
  virtual void AddRef() {}
  virtual void Release() {}
  virtual bool IsHidden(WebWidget* webwidget);
  virtual WebKit::WebScreenInfo GetScreenInfo(WebWidget* webwidget);

 private:
  RenderView* render_view_;
  scoped_ptr<WebView> print_web_view_;
  scoped_ptr<ViewMsg_PrintPages_Params> print_pages_params_;
  base::Time last_cancelled_script_print_;
  int user_cancelled_scripted_print_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintWebViewHelper);
};

#endif  // CHROME_RENDERER_PRINT_WEB_VIEW_DELEGATE_H_
