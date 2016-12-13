/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the entry to the big test program, for linux.

#include "command_buffer/service/cross/big_test_helpers.h"
#include "command_buffer/service/cross/gl/gapi_gl.h"
#include "command_buffer/service/linux/x_utils.h"

namespace o3d {
namespace command_buffer {

String *g_program_path = NULL;
GAPIInterface *g_gapi = NULL;

bool ProcessSystemMessages() {
  return true;
}

}  // namespace command_buffer
}  // namespace o3d

using o3d::String;
using o3d::command_buffer::g_program_path;
using o3d::command_buffer::g_gapi;
using o3d::command_buffer::GAPIGL;
using o3d::command_buffer::XWindowWrapper;


// Creates a GL-compatible window of specified dimensions.
Window CreateWindow(Display *display, unsigned int width, unsigned int height) {
  int attribs[] = {
    GLX_RGBA,
    GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    None
  };
  XVisualInfo *visualInfo = glXChooseVisual(display,
                                            DefaultScreen(display),
                                            attribs);
  Window root_window = RootWindow(display, visualInfo->screen);
  Colormap colorMap = XCreateColormap(display, root_window, visualInfo->visual,
                                      AllocNone);

  XSetWindowAttributes windowAttributes;
  windowAttributes.colormap = colorMap;
  windowAttributes.border_pixel = 0;
  windowAttributes.event_mask = StructureNotifyMask;
  Window window = XCreateWindow(display, root_window,
                                0, 0, width, height, 0, visualInfo->depth,
                                InputOutput, visualInfo->visual,
                                CWBorderPixel|CWColormap|CWEventMask,
                                &windowAttributes);
  if (!window) return 0;
  XMapWindow(display, window);
  XSync(display, True);
  return window;
}

// Creates a window, initializes the GAPI instance.
int main(int argc, char *argv[]) {
  String program_path = argv[0];

  // Remove all characters starting with last '/'.
  size_t backslash_pos = program_path.rfind('/');
  if (backslash_pos != String::npos) {
    program_path.erase(backslash_pos);
  }
  g_program_path = &program_path;

  GAPIGL gl_gapi;
  g_gapi = &gl_gapi;

  Display *display = XOpenDisplay(0);
  if (!display) {
    LOG(FATAL) << "Could not open the display.";
    return 1;
  }

  Window window = CreateWindow(display, 300, 300);
  if (!window) {
    LOG(FATAL) << "Could not create a window.";
    return 1;
  }

  XWindowWrapper wrapper(display, window);
  gl_gapi.set_window_wrapper(&wrapper);

  int ret = big_test_main(argc, argv);

  g_gapi = NULL;
  g_program_path = NULL;
  return ret;
}
