// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include <gtk/gtk.h>

#include "chrome/browser/chrome_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/x11_util.h"

#include "third_party/WebKit/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/x11/WebScreenInfoFactory.h"

using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;

// We get null window_ids passed into the two functions below; please see
// http://crbug.com/9060 for more details.

// Called on the IO thread.
void ResourceMessageFilter::SendBackgroundX11Reply(IPC::Message* reply_msg) {
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
        this, &ResourceMessageFilter::SendBackgroundX11Reply, reply_msg));
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetWindowRect(gfx::NativeViewId view,
                                              IPC::Message* reply_msg) {
  gfx::Rect rect;

  if (view) {
    XID window = x11_util::GetX11WindowFromGtkWidget(
        gfx::NativeViewFromId(view));

    int x, y;
    unsigned width, height;
    x11_util::GetWindowGeometry(&x, &y, &width, &height, window);
    rect = gfx::Rect(x, y, width, height);
  }

  ViewHostMsg_GetWindowRect::WriteReplyParams(reply_msg, rect);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendBackgroundX11Reply, reply_msg));
}

// Called on the BACKGROUND_X11 thread.
void ResourceMessageFilter::DoOnGetRootWindowRect(gfx::NativeViewId view,
                                                  IPC::Message* reply_msg) {
  gfx::Rect rect;

  if (view && gfx::NativeViewFromId(view)->window) {
    // Windows uses GetAncestor(window, GA_ROOT) here which probably means
    // we want the top level window.
    // TODO(agl): calling GTK from this thread is not safe. However, we still
    // have to solve the issue where we pass GtkWidget* into the renderer and
    // the solution to that should also fix this problem.
    GdkWindow* gdk_window =
        gdk_window_get_toplevel(gfx::NativeViewFromId(view)->window);
    XID window = x11_util::GetX11WindowFromGdkWindow(gdk_window);
    int x, y;
    unsigned width, height;
    x11_util::GetWindowGeometry(&x, &y, &width, &height, window);
    rect = gfx::Rect(x, y, width, height);
  }

  ViewHostMsg_GetRootWindowRect::WriteReplyParams(reply_msg, rect);

  ChromeThread::GetMessageLoop(ChromeThread::IO)->PostTask(
      FROM_HERE, NewRunnableMethod(
        this, &ResourceMessageFilter::SendBackgroundX11Reply, reply_msg));
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
