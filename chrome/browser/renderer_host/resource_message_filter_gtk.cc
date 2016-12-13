// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include "base/clipboard.h"
#include "base/gfx/gtk_native_view_id_manager.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/x11_util.h"

#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/x11/WebScreenInfoFactory.h"

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

// Called on the IO thread.
void ResourceMessageFilter::SendDelayedReply(IPC::Message* reply_msg) {
  Send(reply_msg);
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetScreenInfo(gfx::NativeViewId view,
                                              IPC::Message* reply_msg) {
  Display* display = x11_util::GetSecondaryDisplay();
  int screen = x11_util::GetDefaultScreen(display);
  WebScreenInfo results = WebScreenInfoFactory::screenInfo(display, screen);
  ViewHostMsg_GetScreenInfo::WriteReplyParams(reply_msg, results);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetWindowRect(gfx::NativeViewId view,
                                              IPC::Message* reply_msg) {
  // This is called to get the x, y offset (in screen coordinates) of the given
  // view and its width and height.
  gfx::Rect rect;
  XID window;

  if (Singleton<GtkNativeViewManager>()->GetXIDForId(&window, view)) {
    if (window) {
      int x, y;
      unsigned width, height;
      if (x11_util::GetWindowGeometry(&x, &y, &width, &height, window))
        rect = gfx::Rect(x, y, width, height);
    }
  }

  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg, rect);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Return the top-level parent of the given window. Called on the
// BACKGROUND_X11 thread.
static XID GetTopLevelWindow(XID window) {
  bool parent_is_root;
  XID parent_window;

  if (!x11_util::GetWindowParent(&parent_window, &parent_is_root, window))
    return 0;
  if (parent_is_root)
    return window;

  return GetTopLevelWindow(parent_window);
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetRootWindowRect(gfx::NativeViewId view,
                                                  IPC::Message* reply_msg) {
  // This is called to get the screen coordinates and size of the browser
  // window itself.
  gfx::Rect rect;
  XID window;

  if (Singleton<GtkNativeViewManager>()->GetXIDForId(&window, view)) {
    if (window) {
      const XID toplevel = GetTopLevelWindow(window);
      int x, y;
      unsigned width, height;
      if (x11_util::GetWindowGeometry(&x, &y, &width, &height, toplevel))
        rect = gfx::Rect(x, y, width, height);
    }
  }

  ViewHostMsg_GetRootWindowRect::WriteReplyParams(reply_msg, rect);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardIsFormatAvailable(
    Clipboard::FormatType format, IPC::Message* reply_msg) {
  const bool result = GetClipboard()->IsFormatAvailable(format);

  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply_msg, result);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardReadText(IPC::Message* reply_msg) {
  string16 result;
  GetClipboard()->ReadText(&result);

  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply_msg, result);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardReadAsciiText(
    IPC::Message* reply_msg) {
  std::string result;
  GetClipboard()->ReadAsciiText(&result);

  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply_msg, result);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the UI thread.
void ResourceMessageFilter::DoOnClipboardReadHTML(IPC::Message* reply_msg) {
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(&markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply_msg, markup, src_url);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendDelayedReply, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnGetScreenInfo(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  ChromeThread::GetMessageLoop(ChromeThread::BACKGROUND_X11)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::DoOnGetScreenInfo, view, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnGetWindowRect(gfx::NativeViewId view,
                                            IPC::Message* reply_msg) {
  ChromeThread::GetMessageLoop(ChromeThread::BACKGROUND_X11)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::DoOnGetWindowRect, view, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnGetRootWindowRect(gfx::NativeViewId view,
                                                IPC::Message* reply_msg) {
  ChromeThread::GetMessageLoop(ChromeThread::BACKGROUND_X11)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::DoOnGetRootWindowRect, view, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardIsFormatAvailable(
    Clipboard::FormatType format, IPC::Message* reply_msg) {
  ui_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::DoOnClipboardIsFormatAvailable, format,
        reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardReadText(IPC::Message* reply_msg) {
  ui_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::DoOnClipboardReadText, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardReadAsciiText(IPC::Message* reply_msg) {
  ui_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::DoOnClipboardReadAsciiText, reply_msg));
}

// Called on the IO thread.
void ResourceMessageFilter::OnClipboardReadHTML(IPC::Message* reply_msg) {
  ui_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::DoOnClipboardReadHTML, reply_msg));
}
