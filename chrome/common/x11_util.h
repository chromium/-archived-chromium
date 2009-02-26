// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_X11_UTIL_H_
#define CHROME_COMMON_X11_UTIL_H_

// This file declares utility functions for X11 (Linux only).
//
// These functions do not require the Xlib headers to be included (which is why
// we use a void* for Visual*). The Xlib headers are highly polluting so we try
// hard to limit their spread into the rest of the code.

typedef struct _GtkWidget GtkWidget;
typedef unsigned long XID;
typedef struct _XDisplay Display;

namespace gfx {
class Size;
}

namespace x11_util {
  // These functions cache their results and must be called from the UI thread.
  // Currently they don't support multiple screens/displays.

  // Return an X11 connection for the current, primary display.
  Display* GetXDisplay();
  // Return true iff the connection supports X shared memory
  bool QuerySharedMemorySupport(Display* dpy);

  // These functions do not cache their results

  // Get the X window id for the default root window
  XID GetX11RootWindow();
  // Get the X window id for the given GTK widget.
  XID GetX11WindowFromGtkWidget(GtkWidget*);
  // Get a Visual from the given widget. Since we don't include the Xlib
  // headers, this is returned as a void*.
  void* GetVisualFromGtkWidget(GtkWidget*);

  // Return a handle to a server side pixmap. |shared_memory_key| is a SysV
  // IPC key. The shared memory region must contain 32-bit pixels.
  XID AttachSharedMemory(Display* display, int shared_memory_support);
  void DetachSharedMemory(Display* display, XID shmseg);

  // Return a handle to an XRender picture where |pixmap| is a handle to a
  // pixmap containing Skia ARGB data.
  XID CreatePictureFromSkiaPixmap(Display* display, XID pixmap);

  void FreePicture(Display* display, XID picture);
  void FreePixmap(Display* display, XID pixmap);
};

#endif  // CHROME_COMMON_X11_UTIL_H_
