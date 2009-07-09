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


// Contains windows-specific code for setting up the Client object
// used in the unit tests.  Defines WinMain and a WindowProc for running
// the GUnit tests

#include "core/cross/client_info.h"
#include "core/cross/class_manager.h"
#include "core/cross/evaluation_counter.h"
#include "core/cross/install_check.h"
#include "core/cross/features.h"
#include "core/cross/object_manager.h"
#include "core/cross/profiler.h"
#include "core/cross/renderer.h"
#include "core/cross/renderer_platform.h"
#include "core/cross/service_locator.h"
#include "core/cross/types.h"

o3d::String *g_program_path = NULL;
o3d::String *g_program_name = NULL;
o3d::Renderer* g_renderer = NULL;
o3d::ServiceLocator* g_service_locator = NULL;
o3d::DisplayWindow* g_display_window = NULL;

Display *g_display = NULL;
Window g_window = 0;

static char kOffScreenRenderer[] = "O3D_D3D9_OFF_SCREEN";

extern int test_main(int argc, char **argv);

int main(int argc, char *argv[]) {
  std::string error;
  if (!o3d::RendererInstallCheck(&error)) {
    return false;
  }

  std::string program_path(argv[0]);
  std::string program_name;

  // Remove all characters starting with last '/'.
  size_t backslash_pos = program_path.rfind('/');
  if (backslash_pos != o3d::String::npos) {
    program_name = program_path.substr(backslash_pos + 1);
    if (backslash_pos == 0) {
      program_path = "/";
    } else {
      program_path.erase(backslash_pos);
    }
  } else {
    program_name = program_path;
    program_path = ".";
  }

  g_program_path = &program_path;
  g_program_name = &program_name;

  o3d::ServiceLocator service_locator;
  g_service_locator = &service_locator;

  o3d::EvaluationCounter evaluation_counter(g_service_locator);
  o3d::ClassManager class_manager(g_service_locator);
  o3d::ClientInfoManager client_info_manager(g_service_locator);
  o3d::ObjectManager object_manager(g_service_locator);
  o3d::Profiler profiler(g_service_locator);
  o3d::Features features(g_service_locator);

  // create a renderer device based on the current platform
  g_renderer = o3d::Renderer::CreateDefaultRenderer(g_service_locator);

  g_display = ::XOpenDisplay(0);
  int attribs[] = {
    GLX_RGBA,
    GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    None
  };
  XVisualInfo *visualInfo = ::glXChooseVisual(g_display,
                                              DefaultScreen(g_display),
                                              attribs);
  Window root_window = RootWindow(g_display, visualInfo->screen);
  Colormap colorMap = ::XCreateColormap(g_display,
                                        root_window,
                                        visualInfo->visual,
                                        AllocNone);

  XSetWindowAttributes windowAttributes;
  windowAttributes.colormap = colorMap;
  windowAttributes.border_pixel = 0;
  windowAttributes.event_mask = StructureNotifyMask;
  g_window = ::XCreateWindow(g_display,
                             root_window,
                             0, 0, 800, 600,
                             0,
                             visualInfo->depth,
                             InputOutput,
                             visualInfo->visual,
                             CWBorderPixel|CWColormap|CWEventMask,
                             &windowAttributes);
  ::XFree(visualInfo);
  ::XMapWindow(g_display, g_window);
  ::XSync(g_display, True);

  o3d::DisplayWindowLinux* display_window =
      new o3d::DisplayWindowLinux();
  display_window->set_display(g_display);
  display_window->set_window(g_window);
  g_display_window = display_window;

  // Initialize the renderer for off-screen rendering if kOffScreenRenderer
  // is in the environment.
  bool success;
  if (getenv(kOffScreenRenderer)) {
    success =
        g_renderer->Init(*g_display_window, true) == o3d::Renderer::SUCCESS;
  } else {
    success =
        g_renderer->Init(*g_display_window, false) == o3d::Renderer::SUCCESS;
  }

  int ret = EXIT_FAILURE;
  if (!success) {
      ::fprintf(stdout, "Failed to initialize renderer\n");
  } else {
    ret = test_main(argc, argv);
  }

  g_renderer->Destroy();
  delete g_renderer;
  g_renderer = NULL;

  delete display_window;
  g_display_window = NULL;
  g_program_path = NULL;
  g_program_name = NULL;
  ::XCloseDisplay(g_display);

  return ret;
}
