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


// This file implements the platform specific parts of the plugin for
// the Windows platform.

#include "plugin/cross/main.h"

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "core/cross/display_mode.h"
#include "core/cross/event.h"
#include "plugin/cross/plugin_logging.h"
#include "plugin/cross/out_of_memory.h"
#include "statsreport/metrics.h"
#include "v8/include/v8.h"
#include "breakpad/win/bluescreen_detector.h"

using glue::_o3d::PluginObject;
using glue::StreamManager;
using o3d::DisplayWindowWindows;
using o3d::Event;

o3d::PluginLogging* g_logger = NULL;
bool g_logging_initialized = false;
o3d::BluescreenDetector *g_bluescreen_detector = NULL;

namespace {
// We would normally make this a stack variable in main(), but in a
// plugin, that's not possible, so we allocate it dynamically and
// destroy it explicitly.
scoped_ptr<base::AtExitManager> g_at_exit_manager;
}  // end anonymous namespace

void RenderOnDemandCallbackHandler::Run() {
  ::InvalidateRect(obj_->GetHWnd(), NULL, TRUE);
}

static int HandleKeyboardEvent(PluginObject *obj,
                               HWND hWnd,
                               UINT Msg,
                               WPARAM wParam,
                               LPARAM lParam) {
  DCHECK(obj);
  DCHECK(obj->client());
  Event::Type type;
  // First figure out which kind of event to create, and do any event-specific
  // processing that can be done prior to creating it.
  switch (Msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      type = Event::TYPE_KEYDOWN;
      break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      type = Event::TYPE_KEYUP;
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      type = Event::TYPE_KEYPRESS;
      break;
    default:
      LOG(FATAL) << "Unknown keyboard event: " << Msg;
  }
  Event event(type);
  switch (Msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
      event.set_key_code(wParam);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      event.set_char_code(wParam);
      break;
    default:
      LOG(FATAL) << "Unknown keyboard event: " << Msg;
  }
  // TODO: Try out TranslateAccelerator to see if that causes
  // accelerators to start working.  They would then evade JavaScript handlers,
  // though, so perhaps we'd have to check to see if we want to handle them
  // first?  That would require going around the event queue, at least
  // partially.  OTOH I see hints in blog posts that only Firefox allows
  // handlers to suppress syskeys anyway, so maybe it's not that big a
  // deal...unless apps want to use those keys...and depending on what happens
  // in other browsers.

  unsigned char keyboard_state[256];
  if (!::GetKeyboardState(static_cast<PBYTE>(keyboard_state))) {
    LOG(ERROR) << "GetKeyboardState failed.";
    return 1;
  }

  int modifier_state = 0;
  if (keyboard_state[VK_CONTROL] & 0x80) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (keyboard_state[VK_SHIFT] & 0x80) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (keyboard_state[VK_MENU] & 0x80) {
    modifier_state |= Event::MODIFIER_ALT;
  }
  event.set_modifier_state(modifier_state);

  if (event.type() == Event::TYPE_KEYDOWN &&
      (wParam == VK_ESCAPE ||
      (wParam == VK_F4 && (modifier_state & Event::MODIFIER_ALT)))) {
    obj->CancelFullscreenDisplay();
  }

  obj->client()->AddEventToQueue(event);
  return 0;
}

static void HandleMouseEvent(PluginObject *obj,
                             HWND hWnd,
                             UINT Msg,
                             WPARAM wParam,
                             LPARAM lParam) {
  DCHECK(obj);
  DCHECK(obj->client());
  bool fake_dblclick = false;
  Event::Type type;
  int x = GET_X_LPARAM(lParam);
  int y = GET_Y_LPARAM(lParam);
  int screen_x, screen_y;
  bool in_plugin = false;
  {
    RECT rect;
    if (!::GetWindowRect(hWnd, &rect)) {
      DCHECK(false);
      return;
    }
    if (Msg == WM_MOUSEWHEEL || Msg == WM_MOUSEHWHEEL ||
        Msg == WM_CONTEXTMENU) {
      // These messages return screen-relative coordinates, not
      // window-relative coordinates.
      screen_x = x;
      screen_y = y;
      x -= rect.left;
      y -= rect.top;
    } else {
      screen_x = x + rect.left;
      screen_y = y + rect.top;
    }
    if (x >= 0 && x < rect.right - rect.left &&
        y >= 0 && y < rect.bottom - rect.top) {
      // x, y are 0-based from the top-left corner of the plugin.  Rect is in
      // screen coordinates, with bottom > top, right > left.
      in_plugin = true;
    }
  }
  // First figure out which kind of event to create, and do any event-specific
  // processing that can be done prior to creating it.
  switch (Msg) {
    case WM_MOUSEMOVE:
      type = Event::TYPE_MOUSEMOVE;
      break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDOWN:
#endif
      type = Event::TYPE_MOUSEDOWN;
      obj->set_got_dblclick(false);
      SetCapture(hWnd);  // Capture mouse to make sure we get the mouseup.
      break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONUP:
#endif
      type = Event::TYPE_MOUSEUP;
      if (obj->got_dblclick()) {
        fake_dblclick = in_plugin;
        obj->set_got_dblclick(false);
      }
      ReleaseCapture();
      break;

    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDBLCLK:
#endif
      // On a double-click, windows produces: down, up, move, dblclick, up.
      // JavaScript should receive: down, up, [optional move, ] click, down,
      // up, click, dblclick.
      // The EventManager turns (down, up) into click, since we need that on all
      // platforms.  Windows then has to turn (dblclick, up) into (down, up,
      // click, dblclick) IFF both events took place in the plugin.  If only the
      // dblclick did, it just turns into a down.  If only the up did, it's just
      // an up, and we shouldn't be passing along the down/dblclick anyway.  So
      // we turn the doubleclick into a mousedown, store the fact that it was a
      // doubleclick, and wait for the corresponding mouseup to finish off the
      // sequence.  If we get anything that indicates that we missed the mouseup
      // [because it went to a different window or element], we forget about the
      // dblclick.
      DCHECK(in_plugin);
      obj->set_got_dblclick(true);
      type = Event::TYPE_MOUSEDOWN;
      SetCapture(hWnd);  // Capture mouse to make sure we get the mouseup.
      break;

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      type = Event::TYPE_WHEEL;
      break;

    case WM_CONTEXTMENU:
      type = Event::TYPE_CONTEXTMENU;
      break;

    default:
      LOG(FATAL) << "Unknown mouse event: " << Msg;
  }
  Event event(type);
  // Now do any event-specific code that requires an Event object.
  switch (Msg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
      event.set_button(Event::BUTTON_LEFT);
      break;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
      event.set_button(Event::BUTTON_RIGHT);
      break;

    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
      event.set_button(Event::BUTTON_MIDDLE);
      break;

#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
      if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
        event.set_button(Event::BUTTON_4);
      } else {
        event.set_button(Event::BUTTON_5);
      }
      break;
#endif
    case WM_MOUSEWHEEL:
      event.set_delta(0, GET_WHEEL_DELTA_WPARAM(wParam));
      break;
    case WM_MOUSEHWHEEL:
      event.set_delta(GET_WHEEL_DELTA_WPARAM(wParam), 0);
      break;

    default:
      break;
  }

  if (event.type() != WM_CONTEXTMENU) {
    // Only the context menu event doesn't get this information.
    int modifier_state = 0;
    if (wParam & MK_CONTROL) {
      modifier_state |= Event::MODIFIER_CTRL;
    }
    if (wParam & MK_SHIFT) {
      modifier_state |= Event::MODIFIER_SHIFT;
    }
    if (::GetKeyState(VK_MENU) < 0) {
      modifier_state |= Event::MODIFIER_ALT;
    }
    event.set_modifier_state(modifier_state);
  }

  event.set_position(x, y, screen_x, screen_y, in_plugin);
  obj->client()->AddEventToQueue(event);
  if (fake_dblclick) {
    event.set_type(Event::TYPE_DBLCLICK);
    obj->client()->AddEventToQueue(event);
  }
  if (in_plugin && type == Event::TYPE_MOUSEDOWN &&
      obj->HitFullscreenClickRegion(x, y)) {
    obj->RequestFullscreenDisplay();
  }
}

// This returns 0 on success, 1 on failure, to match WindowProc.
static LRESULT ForwardEvent(PluginObject *obj,
                            HWND hWnd,
                            UINT Msg,
                            WPARAM wParam,
                            LPARAM lParam,
                            bool translateCoords) {
  DCHECK(obj);
  DCHECK(obj->GetPluginHWnd());
  HWND dest_hwnd = obj->GetParentHWnd();
  DCHECK(hWnd);
  DCHECK(dest_hwnd);
  bool fullscreen = hWnd == obj->GetFullscreenHWnd();
  if (fullscreen) {
    dest_hwnd = obj->GetPluginHWnd();
  } else if (obj->IsChrome()) {
    // When trying to find the parent window of the Chrome plugin, new Chrome is
    // different than old Chrome; it's got an extra wrapper window around the
    // plugin that didn't used to be there.  The wrapper won't listen to events,
    // so if we see it, we have to go one window up the tree from there in order
    // to find someone who'll listen to us.  The new behavior is seen in nightly
    // builds of Chromium as of 2.0.163.0 (9877) [but went in some time before
    // that]; the old behavior is still exhibited by Chrome as of 1.0.154.48.
    wchar_t chrome_class_name[] = L"WrapperNativeWindowClass";
    wchar_t buffer[sizeof(chrome_class_name) / sizeof(chrome_class_name[0])];
    if (!GetClassName(dest_hwnd, buffer, sizeof(buffer) / sizeof(buffer[0]))) {
      return 1;
    }
    if (!wcscmp(chrome_class_name, buffer)) {
      dest_hwnd = ::GetParent(dest_hwnd);
    }
  }
  if (translateCoords) {
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    RECT rect0, rect1;
    if (!::GetWindowRect(hWnd, &rect0)) {
      DCHECK(false);
      return 1;
    }
    if (!::GetWindowRect(dest_hwnd, &rect1)) {
      DCHECK(false);
      return 1;
    }
    int width = rect0.right - rect0.left;
    int width_1 = rect1.right - rect1.left;

    int x_1;
    int y_1;

    if (!fullscreen) {  // Translate from plugin to browser offset coords.
      x_1 = x - rect1.left + rect0.left;
    } else {  // Translate from screen to plugin offset coords.
      // The plugin and the fullscreen window each fill their respective entire
      // window, so there aren't any offsets to add or subtract.
      x_1 = x * width_1 / width;
    }
    int height = rect0.bottom - rect0.top;
    int height_1 = rect1.bottom - rect1.top;
    if (!fullscreen) {  // Translate from plugin to browser offset coords.
      y_1 = y - rect1.top + rect0.top;
    } else {  // Translate from screen to plugin offset coords.
      // The plugin and the fullscreen window each fill their respective entire
      // window, so there aren't any offsets to add or subtract.
      y_1 = y * height_1 / height;
    }

    lParam = MAKELPARAM(x_1, y_1);
  }
  return !::PostMessage(dest_hwnd, Msg, wParam, lParam);
}

static LRESULT HandleDragAndDrop(PluginObject *obj, WPARAM wParam) {
  HDROP hDrop = reinterpret_cast<HDROP>(wParam);
  UINT num_files = ::DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
  if (!num_files) {
    ::DragFinish(hDrop);
    return 0;
  }
  UINT path_len = ::DragQueryFile(hDrop, 0, NULL, 0);
  // Let's limit that length, just in case.
  if (!path_len || path_len > 4096) {
    ::DragFinish(hDrop);
    return 0;
  }
  scoped_array<TCHAR> path(new TCHAR[path_len + 1]);  // Add 1 for the NUL.
  UINT num_chars = ::DragQueryFile(hDrop, 0, path.get(), path_len + 1);
  DCHECK(num_chars == path_len);
  ::DragFinish(hDrop);

  char *path_to_use = NULL;
#ifdef UNICODE  // TCHAR is WCHAR
  // TODO: We definitely need to move this to a string utility class.
  int bytes_needed = ::WideCharToMultiByte(CP_UTF8,
                                           0,
                                           path.get(),
                                           num_chars + 1,
                                           NULL,
                                           0,
                                           NULL,
                                           NULL);
  // Let's limit that length, just in case.
  if (!bytes_needed || bytes_needed > 4096) {
    return 0;
  }
  scoped_array<char> utf8_path(new char[bytes_needed]);
  int bytes_conv = ::WideCharToMultiByte(CP_UTF8,
                                         0,
                                         path.get(),
                                         num_chars + 1,
                                         utf8_path.get(),
                                         bytes_needed,
                                         NULL,
                                         NULL);
  DCHECK(bytes_conv == bytes_needed);
  path_to_use = utf8_path.get();
  num_chars = bytes_conv;
#else
  path_to_use = path.get();
#endif

  for (UINT i = 0; i < num_chars; ++i) {
    if (path_to_use[i] == '\\') {
      path_to_use[i] = '/';
    }
  }
  obj->RedirectToFile(path_to_use);

  return 1;
}

static LRESULT CALLBACK
WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
  PluginObject *obj = PluginObject::GetPluginProperty(hWnd);
  if (obj == NULL) {                   // It's not my window
    return 1;  // 0 often means we handled it.
  }

  // Limit the ways in which we can be reentrant.  Note that this WindowProc
  // may be called by different threads.  For example, IE will register plugin
  // instances on separate threads.
  o3d::Client::ScopedIncrement reentrance_count(obj->client());

  switch (Msg) {
    case WM_PAINT: {
      if (reentrance_count.get() > 1) {
        // In Chrome, alert dialogs raised from JavaScript cause
        // reentrant WM_PAINT messages to be dispatched and 100% CPU
        // to be consumed unless we call this
        ::ValidateRect(hWnd, NULL);
        break;  // Ignore this message; we're reentrant.
      }
      PAINTSTRUCT paint_struct;
      HDC hdc = ::BeginPaint(hWnd, &paint_struct);
      if (paint_struct.rcPaint.right - paint_struct.rcPaint.left != 0 ||
          paint_struct.rcPaint.bottom - paint_struct.rcPaint.top != 0) {
        if (obj->renderer()) {
          // It appears to be necessary to use GDI to paint something at least
          // once before D3D rendering will work in Vista with Aero.
          if (!obj->RecordPaint()) {
            ::SetPixelV(hdc, 0, 0, RGB(0, 0, 0));
          }

          obj->client()->RenderClient();
        } else {
          // If there Client has no Renderer associated with it, paint the draw
          // area gray.
          ::SelectObject(paint_struct.hdc, GetStockObject(DKGRAY_BRUSH));
          ::Rectangle(paint_struct.hdc,
                      paint_struct.rcPaint.left,
                      paint_struct.rcPaint.top,
                      paint_struct.rcPaint.right,
                      paint_struct.rcPaint.bottom);
        }
      }
      ::EndPaint(hWnd, &paint_struct);
      break;
    }
    case WM_SETCURSOR: {
      obj->set_cursor(obj->cursor());
      return 1;
    }
    case WM_ERASEBKGND: {
      return 1;  // tell windows we don't need the background cleared
    }
    case WM_SIZE: {
      // Resize event called
      if (reentrance_count.get() > 1) {
        break;  // Ignore this message; we're reentrant.
      }

      // get new dimensions of window
      int window_width = LOWORD(lParam);
      int window_height = HIWORD(lParam);

      // Tell the plugin that it has been resized
      obj->Resize(window_width, window_height);
      break;
    }
    case WM_TIMER: {
      if (reentrance_count.get() > 1) {
        break;  // Ignore this message; we're reentrant.
      }
      // DoOnFrameCallback(obj);

      // TODO: Only logging for windows until we figure out the proper
      //            mac way
      if (g_logger) g_logger->UpdateLogging();

      obj->client()->Tick();
      if (obj->client()->render_mode() ==
          o3d::Client::RENDERMODE_CONTINUOUS) {
        // Must invalidate GetHWnd()'s drawing area, no matter which window is
        // receiving this event.  It turns out that we have to set the timer on
        // the window we're using for drawing anyway, whichever that is, but
        // it's possible that an extra event will slip through.
        ::InvalidateRect(obj->GetHWnd(), NULL, TRUE);
      }
      // Calling UpdateWindow to force a WM_PAINT here causes problems in
      // Firefox 2 if rendering takes too long. WM_PAINT will be sent anyway
      // when there are no other messages to process.
      break;
    }
    case WM_NCDESTROY: {
      // Win32 docs say we must remove all our properties before destruction.
      // However, this message doesn't appear to come early enough to be useful
      // when running in Chrome, at least.
      PluginObject::ClearPluginProperty(hWnd);
      break;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONDOWN:
    case WM_XBUTTONDBLCLK:
#endif
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
      // Without this SetFocus, if you alt+tab away from Firefox, then click
      // back in the plugin, Firefox will get keyboard focus but we won't.
      // However, if we do it on mouseup as well, then the click that triggers
      // fullscreen, and is followed by a mouseup in the plugin, which will grab
      // the focus back from the fullscreen window.
      ::SetFocus(hWnd);
    // FALL THROUGH
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
#if (_WIN32_WINNT >= 0x0500)
    case WM_XBUTTONUP:
#endif
    case WM_MOUSEMOVE:
    case WM_CONTEXTMENU:
      HandleMouseEvent(obj, hWnd, Msg, wParam, lParam);
      break;

    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
    case WM_UNICHAR:
      // I believe these are redundant; TODO: Test this on a non-US
      // keyboard.
      break;

    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      return HandleKeyboardEvent(obj, hWnd, Msg, wParam, lParam);

#if(_WIN32_WINNT >= 0x0500)
    case WM_APPCOMMAND:
#endif /* _WIN32_WINNT >= 0x0500 */
      return ForwardEvent(obj, hWnd, Msg, wParam, lParam, false);

    case WM_DROPFILES:
      return HandleDragAndDrop(obj, wParam);

    case WM_KILLFOCUS:
      // If we lose focus [which also happens on alt+f4 killing the fullscreen
      // window] fall back to plugin mode to avoid lost-device awkwardness.
      // TODO: We'll have problems with this when dealing with e.g.
      // Japanese text input IME windows.
      if (hWnd == obj->GetFullscreenHWnd()) {
        obj->CancelFullscreenDisplay();
        return 0;
      }
      // FALL THROUGH
    case WM_SETFOCUS:
    default:
      // Decrement reentrance_count here.  It's OK if this call
      // boomerangs back to us, given that we're not in the middle of doing
      // anything caused by this message.  Since we decrement reentrance_count
      // manually, its destructor will know not to.
      reentrance_count.decrement();

      if (hWnd == obj->GetFullscreenHWnd()) {
        return ::CallWindowProc(::DefWindowProc,
                                hWnd,
                                Msg,
                                wParam,
                                lParam);
      } else {
        return ::CallWindowProc(obj->GetDefaultPluginWindowProc(),
                                hWnd,
                                Msg,
                                wParam,
                                lParam);
      }
  }
  return 0;
}

NPError PlatformNPPGetValue(NPP instance, NPPVariable variable, void *value) {
  return NPERR_INVALID_PARAM;
}

extern "C" {
  BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_DETACH) {
       // Teardown V8 when the plugin dll is unloaded.
       // NOTE: NP_Shutdown would have been a good place for this code but
       //       unfortunately it looks like it gets called even when the dll
       //       isn't really unloaded.  This is a problem since after calling
       //       V8::Dispose(), V8 cannot be initialized again.
       bool v8_disposed = v8::V8::Dispose();
       if (!v8_disposed)
         DLOG(ERROR) << "Failed to release V8 resources.";
       return true;
    }
    return true;
  }
  
  NPError InitializePlugin() {
    if (!o3d::SetupOutOfMemoryHandler())
      return NPERR_MODULE_LOAD_FAILED_ERROR;

    // Setup crash handler
    if (!g_exception_manager) {
      g_exception_manager = new ExceptionManager(false);
      g_exception_manager->StartMonitoring();
    }

    // Initialize the AtExitManager so that base singletons can be
    // destroyed properly.
    g_at_exit_manager.reset(new base::AtExitManager());

    // Turn on the logging.
    CommandLine::Init(0, NULL);
    InitLogging(L"debug.log",
                logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                logging::DONT_LOCK_LOG_FILE,
                logging::APPEND_TO_OLD_LOG_FILE);

    DLOG(INFO) << "NP_Initialize";

    // Limit the current thread to one processor (the current one). This ensures
    // that timing code runs on only one processor, and will not suffer any ill
    // effects from power management. See "Game Timing and Multicore Processors"
    // in the DirectX docs for more details
    {
      HANDLE current_process_handle = GetCurrentProcess();

      // Get the processor affinity mask for this process
      DWORD_PTR process_affinity_mask = 0;
      DWORD_PTR system_affinity_mask = 0;

      if (GetProcessAffinityMask(current_process_handle,
                                 &process_affinity_mask,
                                 &system_affinity_mask) != 0 &&
          process_affinity_mask) {
        // Find the lowest processor that our process is allows to run against
        DWORD_PTR affinity_mask = (process_affinity_mask &
                                   ((~process_affinity_mask) + 1));

        // Set this as the processor that our thread must always run against
        // This must be a subset of the process affinity mask
        HANDLE hCurrentThread = GetCurrentThread();
        if (INVALID_HANDLE_VALUE != hCurrentThread) {
          SetThreadAffinityMask(hCurrentThread, affinity_mask);
          CloseHandle(hCurrentThread);
        }
      }

      CloseHandle(current_process_handle);
    }

    return NPERR_NO_ERROR;
  }

  NPError OSCALL NP_Initialize(NPNetscapeFuncs *browserFuncs) {
    HANDLE_CRASHES;
    NPError retval = InitializeNPNApi(browserFuncs);
    if (retval != NPERR_NO_ERROR) return retval;
    return InitializePlugin();
  }

  NPError OSCALL NP_Shutdown(void) {
    HANDLE_CRASHES;
    DLOG(INFO) << "NP_Shutdown";
    if (g_logger) {
      // Do a last sweep to aggregate metrics before we shut down
      g_logger->ProcessMetrics(true, false);
      delete g_logger;
      g_logger = NULL;
      g_logging_initialized = false;
      stats_report::g_global_metrics.Uninitialize();
    }

    CommandLine::Terminate();

    // Force all base singletons to be destroyed.
    g_at_exit_manager.reset(NULL);

    // TODO : This is commented out until we can determine if
    // it's safe to shutdown breakpad at this stage (Gears, for
    // example, never deletes...)
    // Shutdown breakpad
    // delete g_exception_manager;

    // Strictly speaking, on windows, it's not really necessary to call
    // Stop(), but we do so for completeness
    if (g_bluescreen_detector) {
      g_bluescreen_detector->Stop();
      delete g_bluescreen_detector;
      g_bluescreen_detector = NULL;
    }

    return NPERR_NO_ERROR;
  }

  NPError NPP_New(NPMIMEType pluginType,
                  NPP instance,
                  uint16 mode,
                  int16 argc,
                  char *argn[],
                  char *argv[],
                  NPSavedData *saved) {
    HANDLE_CRASHES;

    if (!g_logging_initialized) {
      // Get user config metrics. These won't be stored though unless the user
      // opts-in for usagestats logging
      GetUserAgentMetrics(instance);
      GetUserConfigMetrics();
      // Create usage stats logs object
      g_logger = o3d::PluginLogging::InitializeUsageStatsLogging();
      if (g_logger) {
        // Setup blue-screen detection
        g_bluescreen_detector = new o3d::BluescreenDetector();
        g_bluescreen_detector->Start();
      }
      g_logging_initialized = true;
    }
    PluginObject* pluginObject = glue::_o3d::PluginObject::Create(
        instance);
    instance->pdata = pluginObject;
    glue::_o3d::InitializeGlue(instance);
    pluginObject->Init(argc, argn, argv);
    return NPERR_NO_ERROR;
  }

  void CleanupFullscreenWindow(PluginObject *obj) {
    DCHECK(obj->GetFullscreenHWnd());
    obj->StorePluginProperty(obj->GetPluginHWnd(), obj);
    ::DestroyWindow(obj->GetFullscreenHWnd());
    obj->SetFullscreenHWnd(NULL);
  }

  void CleanupAllWindows(PluginObject *obj) {
    DCHECK(obj->GetHWnd());
    DCHECK(obj->GetPluginHWnd());
    ::KillTimer(obj->GetHWnd(), 0);
    if (obj->GetFullscreenHWnd()) {
      CleanupFullscreenWindow(obj);
    }
    PluginObject::ClearPluginProperty(obj->GetHWnd());
    ::SetWindowLongPtr(obj->GetPluginHWnd(),
                       GWL_WNDPROC,
                       reinterpret_cast<LONG_PTR>(
                           obj->GetDefaultPluginWindowProc()));
    obj->SetPluginHWnd(NULL);
    obj->SetHWnd(NULL);
  }

  NPError NPP_Destroy(NPP instance, NPSavedData **save) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    if (obj) {
      if (obj->GetHWnd()) {
        CleanupAllWindows(obj);
      }

      obj->TearDown();
      NPN_ReleaseObject(obj);
      instance->pdata = NULL;
    }

    return NPERR_NO_ERROR;
  }

  bool PluginObject::GetDisplayMode(int mode_id, o3d::DisplayMode *mode) {
    return renderer()->GetDisplayMode(mode_id, mode);
  }


  HWND CreateFullscreenWindow(PluginObject *obj,
                              int mode_id) {
    o3d::DisplayMode mode;
    if (!obj->renderer()->GetDisplayMode(mode_id, &mode)) {
      return NULL;
    }
    CHECK(mode.width() > 0 && mode.height() > 0);

    HINSTANCE instance =
        reinterpret_cast<HINSTANCE>(
            ::GetWindowLongPtr(obj->GetPluginHWnd(), GWLP_HINSTANCE));
    WNDCLASSEX *wcx = obj->GetFullscreenWindowClass(instance, WindowProc);
    HWND hWnd = CreateWindowEx(NULL,
                               wcx->lpszClassName,
                               L"O3D Test Fullscreen Window",
                               WS_POPUP,
                               0, 0,
                               mode.width(),
                               mode.height(),
                               NULL,
                               NULL,
                               instance,
                               NULL);

    ShowWindow(hWnd, SW_SHOW);
    return hWnd;
  }

  // TODO: Where should this really live?  It's platform-specific, but in
  // PluginObject, which mainly lives in cross/o3d_glue.h+cc.
  bool PluginObject::RequestFullscreenDisplay() {
    bool success = false;
    DCHECK(GetPluginHWnd());
    if (!fullscreen_ && renderer_ && fullscreen_region_valid_) {
      DCHECK(renderer_->fullscreen() == fullscreen_);
      DCHECK(!GetFullscreenHWnd());
      HWND drawing_hwnd =
        CreateFullscreenWindow(this, fullscreen_region_mode_id_);
      if (drawing_hwnd) {
        ::KillTimer(GetHWnd(), 0);
        SetFullscreenHWnd(drawing_hwnd);
        StorePluginPropertyUnsafe(drawing_hwnd, this);

        DisplayWindowWindows display;
        display.set_hwnd(GetHWnd());
        if (renderer_->SetFullscreen(true, display,
              fullscreen_region_mode_id_)) {
          fullscreen_ = true;
          client()->SendResizeEvent(renderer_->width(), renderer_->height(),
              true);
          success = true;
        } else {
          CleanupFullscreenWindow(this);
        }
        prev_width_ = renderer_->width();
        prev_height_ = renderer_->height();
        ::SetTimer(GetHWnd(), 0, 10, NULL);
      } else {
        LOG(ERROR) << "Failed to create fullscreen window.";
      }
    }
    return success;
  }

  void PluginObject::CancelFullscreenDisplay() {
    DCHECK(GetPluginHWnd());
    if (fullscreen_) {
      DCHECK(renderer());
      DCHECK(renderer()->fullscreen());
      ::KillTimer(GetHWnd(), 0);
      DisplayWindowWindows display;
      display.set_hwnd(GetPluginHWnd());
      if (!renderer_->SetFullscreen(false, display, 0)) {
        LOG(FATAL) << "Failed to get the renderer out of fullscreen mode!";
      }
      CleanupFullscreenWindow(this);
      prev_width_ = renderer_->width();
      prev_height_ = renderer_->height();
      client()->SendResizeEvent(prev_width_, prev_height_, false);
      ::SetTimer(GetHWnd(), 0, 10, NULL);
      fullscreen_ = false;
    }
  }

  NPError NPP_SetWindow(NPP instance, NPWindow *window) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);

    HWND hWnd = static_cast<HWND>(window->window);
    if (!hWnd) {
      // Chrome calls us this way before NPP_Destroy.
      if (obj->GetHWnd()) {
        CleanupAllWindows(obj);
      }
      return NPERR_NO_ERROR;
    }
    if (obj->GetHWnd() == hWnd) {
      return NPERR_NO_ERROR;
    }
    if (obj->fullscreen()) {
      // We can get here if the user alt+tabs away from the fullscreen plugin
      // window or JavaScript resizes the plugin window.
      DCHECK(obj->GetPluginHWnd());
      DCHECK(obj->GetFullscreenHWnd());
      DCHECK(obj->GetPluginHWnd() == hWnd);
      return NPERR_NO_ERROR;
    }
    DCHECK(!obj->GetPluginHWnd());
    obj->SetPluginHWnd(hWnd);
    obj->SetParentHWnd(::GetParent(hWnd));
    PluginObject::StorePluginProperty(hWnd, obj);
    obj->SetDefaultPluginWindowProc(
        reinterpret_cast<WNDPROC>(
            ::SetWindowLongPtr(hWnd,
                               GWL_WNDPROC,
                               reinterpret_cast<LONG_PTR>(WindowProc))));

    // create and assign the graphics context
    DisplayWindowWindows default_display;
    default_display.set_hwnd(obj->GetHWnd());

    obj->CreateRenderer(default_display);
    obj->client()->Init();
    obj->client()->SetRenderOnDemandCallback(
        new RenderOnDemandCallbackHandler(obj));

    // we set the timer to 10ms or 100fps. At the time of this comment
    // the renderer does a vsync the max fps it will run will be the refresh
    // rate of the monitor or 100fps, which ever is lower.
    ::SetTimer(obj->GetHWnd(), 0, 10, NULL);

    return NPERR_NO_ERROR;
  }

  // Called when the browser has finished attempting to stream data to
  // a file as requested. If fname == NULL the attempt was not successful.
  void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    StreamManager *stream_manager = obj->stream_manager();

    stream_manager->SetStreamFile(stream, fname);
  }

  int16 NPP_HandleEvent(NPP instance, void *event) {
    HANDLE_CRASHES;
    return 0;
  }
}  // end extern "C"
