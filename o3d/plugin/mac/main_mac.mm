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
// the Macintosh platform.

#include "plugin/cross/main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"

#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>
#include <AGL/aglRenderers.h>

#include "core/cross/event.h"
#include "statsreport/metrics.h"
#include "plugin/cross/plugin_logging.h"
#include "plugin/cross/plugin_metrics.h"
#include "plugin/cross/out_of_memory.h"
#include "plugin/mac/plugin_mac.h"

o3d::PluginLogging* g_logger = NULL;
bool g_logging_initialized = false;

using glue::_o3d::PluginObject;
using glue::StreamManager;
using o3d::DisplayWindowMac;
using o3d::Event;

namespace {
// We would normally make this a stack variable in main(), but in a
// plugin, that's not possible, so we allocate it dynamically and
// destroy it explicitly.
scoped_ptr<base::AtExitManager> g_at_exit_manager;
}  // end anonymous namespace

// if defined, in AGL mode we do double buffered drawing
// #define USE_AGL_DOUBLE_BUFFER

#define CFTIMER
// #define DEFERRED_DRAW_ON_NULLEVENTS

// currently drawing with the timer doesn't play well with USE_AGL_DOUBLE_BUFFER
#ifdef CFTIMER
#undef USE_AGL_DOUBLE_BUFFER
#endif

static void DrawPlugin(PluginObject* obj) {
  obj->client()->RenderClient();
#ifdef USE_AGL_DOUBLE_BUFFER
  // In AGL mode, we have to call aglSwapBuffers to guarantee that our
  // pixels make it to the screen.
  if (obj->mac_agl_context_ != NULL)
    aglSwapBuffers(obj->mac_agl_context_);
#endif
}

void RenderOnDemandCallbackHandler::Run() {
  obj_->SetWantsRedraw(true);
}

static unsigned char GetMacEventKeyChar(const EventRecord *the_event) {
  unsigned char the_char = the_event->message & charCodeMask;
  return the_char;
}

static unsigned char GetMacEventKeyCode(const EventRecord *the_event) {
  unsigned char the_key_code = (the_event->message & keyCodeMask) >> 8;
  return the_key_code;
}


// Cocoa key events for things like arrow key presses can have strange unicode
// values in the 0xF700-0xF747 range, eg NSUpArrowFunctionKey is 0xF700.
// These values are defined in NSEvent.h.
// Map the ones we care about to the more commonly understood values in
// the Mac system header Events.h, eg kUpArrowCharCode is 30.
static int TranslateMacUnicodeControlChar(int theChar) {
  switch(theChar) {
    case NSUpArrowFunctionKey:
      return kUpArrowCharCode;
    case NSDownArrowFunctionKey:
      return kDownArrowCharCode;
    case NSLeftArrowFunctionKey:
      return kLeftArrowCharCode;
    case NSRightArrowFunctionKey:
      return kRightArrowCharCode;
  }
  return theChar;
}


// The Mac standard char codes for arrow keys are different from the
// web standard.
// Also in the browser world the enter key gets mapped to be the same as the
// return key.
static int TranslateMacControlCharToWebChar(int theChar) {
  switch(theChar) {
    case kUpArrowCharCode:
      return 38;
    case kDownArrowCharCode:
      return 40;
    case kLeftArrowCharCode:
      return 37;
    case kRightArrowCharCode:
      return 39;
    case kEnterCharCode:
      return kReturnCharCode;
  }
  return theChar;
}


// Given an instance, and some event data, calls Javascript methods
// placed on the object tag so that the keystrokes can be handled in
// Javascript.
static void DispatchKeyboardEvent(PluginObject* obj,
                                  EventKind kind,
                                  int theChar,
                                  int theKeyCode,
                                  EventModifiers mods) {
  theChar = TranslateMacUnicodeControlChar(theChar);
  theChar = TranslateMacControlCharToWebChar(theChar);
  int upperChar = (theChar >= 'a' && theChar <='z') ? theChar - 32 : theChar;

  Event::Type type;
  switch (kind) {
    case keyDown:
      // We'll also have to send a keypress below.
      type = Event::TYPE_KEYDOWN;
      break;
    case autoKey:
      type = Event::TYPE_KEYPRESS;
      break;
    case keyUp:
      type = Event::TYPE_KEYUP;
      break;
  }
  Event event(type);

  switch (kind) {
    case keyDown:
    case keyUp:
      event.set_key_code(upperChar);
      break;
    case autoKey:
      event.set_char_code(theChar);
      break;
    default:
      LOG(FATAL) << "Unknown keyboard event: " << kind;
  }

  int modifier_state = 0;
  if (mods & controlKey) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (mods & shiftKey) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (mods & optionKey) {
    modifier_state |= Event::MODIFIER_ALT;
  }
  if (mods & cmdKey) {
    modifier_state |= Event::MODIFIER_META;
  }
  event.set_modifier_state(modifier_state);
  obj->client()->AddEventToQueue(event);
  if (type == Event::TYPE_KEYDOWN) {
    event.clear_key_code();
    event.set_char_code(theChar);
    event.set_type(Event::TYPE_KEYPRESS);
    obj->client()->AddEventToQueue(event);
  }
}

// Given an instance, and a MacOS keyboard event, calls Javascript methods
// placed on the object tag so that the keystrokes can be handled in
// Javascript.
static void DispatchMacKeyboardEvent(PluginObject* obj,
                                     EventRecord* the_event) {
  DispatchKeyboardEvent(obj,
                        the_event->what,
                        GetMacEventKeyChar(the_event),
                        GetMacEventKeyCode(the_event),
                        the_event->modifiers);
}



static void HandleMouseEvent(PluginObject* obj,
                             EventRecord* the_event) {
  DCHECK(obj);
  DCHECK(obj->client());
  int screen_x = the_event->where.h;
  int screen_y = the_event->where.v;
  static Point last_mouse_loc = {0,0};

  o3d::Event::Type type;
  switch (the_event->what) {
    case mouseDown:
      type = o3d::Event::TYPE_MOUSEDOWN;
      break;
    case mouseUp:
      type = o3d::Event::TYPE_MOUSEUP;
      break;
    case nullEvent:
    case osEvt:  // can be various things but in this context it's mouse move
      if (last_mouse_loc.h == screen_x && last_mouse_loc.v == screen_y)
        return;
      type = o3d::Event::TYPE_MOUSEMOVE;
      break;
    default:
      LOG(FATAL) << "Unknown mouse event: " << the_event->what;
      return;
  }

  last_mouse_loc = the_event->where;
  bool in_plugin = false;  // Did the event happen within our drawing region?

  int x, y;
  // now make x and y plugin relative coords
  if (obj->GetFullscreenMacWindow()) {
    Rect  wBounds;
    GetWindowBounds(obj->GetFullscreenMacWindow(), kWindowGlobalPortRgn,
                    &wBounds);
    x = screen_x - wBounds.left;
    y = screen_y - wBounds.top;
    in_plugin = true;
  } else {
    Rect  wBounds;
    GetWindowBounds(obj->mac_window_, kWindowGlobalPortRgn, &wBounds);
    x = screen_x - wBounds.left - obj->last_plugin_loc_.h;
    y = screen_y - wBounds.top - obj->last_plugin_loc_.v;
    in_plugin = x >=0 && y >= 0 && x < obj->width() && y < obj->height();
  }

  o3d::Event event(type);
  int modifier_state = 0;
  if (the_event->modifiers & controlKey) {
    modifier_state |= o3d::Event::MODIFIER_CTRL;
  }
  if (the_event->modifiers & shiftKey) {
    modifier_state |= o3d::Event::MODIFIER_SHIFT;
  }
  if (the_event->modifiers & optionKey) {
    modifier_state |= o3d::Event::MODIFIER_ALT;
  }
  if (the_event->modifiers & cmdKey) {
    modifier_state |= o3d::Event::MODIFIER_META;
  }

  event.set_modifier_state(modifier_state);

  // TODO: Figure out how to detect and set buttons properly.
  if (the_event->modifiers & btnState) {
    event.set_button(o3d::Event::BUTTON_LEFT);
  }

  event.set_position(x, y, screen_x, screen_y, in_plugin);
  obj->client()->AddEventToQueue(event);

  if (in_plugin && type == Event::TYPE_MOUSEDOWN &&
      obj->HitFullscreenClickRegion(x, y)) {
    obj->RequestFullscreenDisplay();
  }
}

// Handle a mouse-related NPCocoaEvent.
// These events come from the new Cocoa revision of the NPAPI spec,
// currently implemented only in Safari.
// See https://wiki.mozilla.org/Mac:NPAPI_Event_Models
static void HandleCocoaMouseEvent(PluginObject* obj,
                                  NPCocoaEvent* the_event) {
  DCHECK(obj);
  DCHECK(obj->client());
  int screen_x = the_event->data.mouse.pluginX;
  int screen_y = the_event->data.mouse.pluginY;

  o3d::Event::Type type;
  switch (the_event->type) {
    case NPCocoaEventMouseDown:
      type = o3d::Event::TYPE_MOUSEDOWN;
      break;
    case NPCocoaEventMouseUp:
      type = o3d::Event::TYPE_MOUSEUP;
      break;
    // The Mac makes a distinction between moved and dragged (ie moved with
    // the button down), but the O3D event model does not.
    case NPCocoaEventMouseMoved:
    case NPCocoaEventMouseDragged:
      type = o3d::Event::TYPE_MOUSEMOVE;
      break;
    case NPCocoaEventScrollWheel:
      type = o3d::Event::TYPE_WHEEL;
      break;
    // Don't care about these currently.
    case NPCocoaEventMouseEntered:
    case NPCocoaEventMouseExited:
    default:
      return;
  }

  int x = the_event->data.mouse.pluginX;
  int y = the_event->data.mouse.pluginY;

  // Did the event happen within our drawing region?
  bool in_plugin = x >= 0 && y >= 0 && x < obj->width() && y < obj->height();

  o3d::Event event(type);
  int modifier_state = 0;
  if (the_event->data.mouse.modifierFlags & NSControlKeyMask) {
    modifier_state |= o3d::Event::MODIFIER_CTRL;
  }
  if (the_event->data.mouse.modifierFlags &
      (NSAlphaShiftKeyMask | NSShiftKeyMask)) {
    modifier_state |= o3d::Event::MODIFIER_SHIFT;
  }
  if (the_event->data.mouse.modifierFlags & NSAlternateKeyMask) {
    modifier_state |= o3d::Event::MODIFIER_ALT;
  }
  if (the_event->data.mouse.modifierFlags & NSCommandKeyMask) {
    modifier_state |= o3d::Event::MODIFIER_META;
  }

  event.set_modifier_state(modifier_state);

  if (the_event->type == NPCocoaEventScrollWheel) {
    if (the_event->data.mouse.deltaX && the_event->data.mouse.deltaY) {
      if (abs(the_event->data.mouse.deltaX) >
          abs(the_event->data.mouse.deltaY)) {
        the_event->data.mouse.deltaY = 0;
      } else {
        the_event->data.mouse.deltaX = 0;
      }
    }
    event.set_delta(the_event->data.mouse.deltaX * 10.0,
                    the_event->data.mouse.deltaY * 10.0);
  }

  if ((the_event->type == NPCocoaEventMouseDown) ||
      (the_event->type == NPCocoaEventMouseUp)) {
    event.set_button(
        MacOSMouseButtonNumberToO3DButton(
            the_event->data.mouse.buttonNumber));
  }

  event.set_position(x, y, screen_x, screen_y, in_plugin);
  obj->client()->AddEventToQueue(event);

  if (in_plugin && type == Event::TYPE_MOUSEDOWN &&
      obj->HitFullscreenClickRegion(x, y)) {
    obj->RequestFullscreenDisplay();
  }
}




// Convert an NSEvent style modifiers field to an EventRecord style one.
// Not all bits have a representation in the target type, eg NSFunctionKeyMask
// but we just need to convert the basic modifiers that need to be passed on
// to Javascript.
static EventModifiers CocoaToEventRecordModifiers(NSUInteger inMods) {
  EventModifiers outMods = 0;

  // NSEvent distinuishes between the shift key being down and the capslock key,
  // but the W3C event spec does not make this distinction.
  if (inMods & (NSAlphaShiftKeyMask | NSShiftKeyMask))
    outMods |= shiftKey;
  if (inMods & NSControlKeyMask)
    outMods |= controlKey;
  if (inMods & NSAlternateKeyMask)
    outMods |= optionKey;
  if (inMods & NSCommandKeyMask)
    outMods |= cmdKey;

  return outMods;
}

// Handle an NPCocoaEvent style event. The Cocoa event interface is
// a recent addition to the NAPI spec.
// See https://wiki.mozilla.org/Mac:NPAPI_Event_Models for further details.
// The principle advantages are that we can get scrollwheel messages,
// mouse-moved messages, and can tell which mouse button was pressed.
// This API will also be required for a carbon-free 64 bit version for 10.6.
bool HandleCocoaEvent(NPP instance, NPCocoaEvent* the_event) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  WindowRef fullscreen_window = obj->GetFullscreenMacWindow();
  bool handled = false;

  if (g_logger) g_logger->UpdateLogging();

  obj->MacEventReceived();
  switch (the_event->type) {
    case NPCocoaEventDrawRect:
      DrawPlugin(obj);
      handled = true;
      break;
    case NPCocoaEventMouseDown:
    case NPCocoaEventMouseUp:
    case NPCocoaEventMouseMoved:
    case NPCocoaEventMouseDragged:
    case NPCocoaEventMouseEntered:
    case NPCocoaEventMouseExited:
    case NPCocoaEventScrollWheel:
      HandleCocoaMouseEvent(obj, the_event);
      break;
    case NPCocoaEventKeyDown:
      // Hard-coded trapdoor to get out of fullscreen mode if user hits escape.
      if (fullscreen_window) {
        NSString *chars =
            (NSString*) the_event->data.key.charactersIgnoringModifiers;

        if ([chars characterAtIndex:0] == '\e') {
          obj->CancelFullscreenDisplay();
          break;
        }
      }  // otherwise fall through
    case NPCocoaEventKeyUp: {
      EventKind eventKind = (the_event->type == NPCocoaEventKeyUp) ? keyUp :
                            (the_event->data.key.isARepeat) ? autoKey : keyDown;

      EventModifiers modifiers =
          CocoaToEventRecordModifiers(the_event->data.key.modifierFlags);

      NSString *chars =
          (NSString*) the_event->data.key.charactersIgnoringModifiers;

      DispatchKeyboardEvent(obj,
                            eventKind,
                            [chars characterAtIndex:0],
                            the_event->data.key.keyCode,
                            modifiers);
      break;
    }
    case NPCocoaEventFlagsChanged:
    case NPCocoaEventFocusChanged:
    case NPCocoaEventWindowFocusChanged:
      // Safari tab switching recovery code.
      if (obj->mac_surface_hidden_) {
        obj->mac_surface_hidden_ = false;
        NPN_ForceRedraw(instance);  // invalidate to cause a redraw
      }

      // Auto-recovery for any situation where another window comes in front
      // of the fullscreen window and we need to exit fullscreen mode.
      // This can happen because another browser window has come forward, or
      // because another app has been called to the front.
      // TODO: We'll have problems with this when dealing with e.g.
      // Japanese text input IME windows.
      if (fullscreen_window && fullscreen_window != ActiveNonFloatingWindow()) {
        obj->CancelFullscreenDisplay();
      }

      break;
  }

  return handled;
}

// List of message types from mozilla's nsplugindefs.h, which is more
// complete than the list in NPAPI.h.
enum nsPluginEventType {
  nsPluginEventType_GetFocusEvent = (osEvt + 16),
  nsPluginEventType_LoseFocusEvent,
  nsPluginEventType_AdjustCursorEvent,
  nsPluginEventType_MenuCommandEvent,
  nsPluginEventType_ClippingChangedEvent,
  nsPluginEventType_ScrollingBeginsEvent,
  nsPluginEventType_ScrollingEndsEvent,
  nsPluginEventType_Idle = 0
};


bool HandleMacEvent(EventRecord* the_event, NPP instance) {
  PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
  bool handled = false;
  WindowRef fullscreen_window = obj->GetFullscreenMacWindow();

  if (g_logger) g_logger->UpdateLogging();

  // Help the plugin keep track of when we last saw an event so the CFTimer can
  // notice if we get cut off, eg by our tab being hidden by Safari, which there
  // is no other way for us to detect.
  obj->MacEventReceived();

  switch (the_event->what) {
    case nullEvent:
#ifdef DEFERRED_DRAW_ON_NULLEVENTS
      GLUE_PROFILE_START(instance, "forceredraw");
      NPN_ForceRedraw(instance);  // invalidate to cause a redraw
      GLUE_PROFILE_STOP(instance, "forceredraw");
#elif defined(CFTIMER)
#else
      DrawPlugin(obj);
#endif
      // Safari tab switching recovery code.
      if (obj->mac_surface_hidden_) {
        obj->mac_surface_hidden_ = false;
        NPN_ForceRedraw(instance);  // invalidate to cause a redraw
      }

      // Auto-recovery for any situation where another window comes in front
      // of the fullscreen window and we need to exit fullscreen mode.
      // This can happen because another browser window has come forward, or
      // because another app has been called to the front.
      if (fullscreen_window && fullscreen_window != ActiveNonFloatingWindow()) {
        obj->CancelFullscreenDisplay();
      }

      // Send nullEvents to HandleMouseEvent so they can be used to simulate
      // mouse moved events. Not needed in fullscreen mode, where we really do
      // get mouse moved events. See the osEvt case below.
      if (!fullscreen_window)
        HandleMouseEvent(obj, the_event);

      handled = true;
      break;
    case updateEvt:
      DrawPlugin(obj);
      handled = true;
      break;
    case osEvt:
      // These are mouse moved messages when so tagged in the high byte.
      if ((the_event->message >> 24) == mouseMovedMessage) {
        HandleMouseEvent(obj, the_event);
        handled = true;
      }
      break;
    case mouseDown:
      HandleMouseEvent(obj, the_event);
      break;
    case mouseUp:
      HandleMouseEvent(obj, the_event);
      handled = true;
      break;
    case keyDown:
      // Hard-coded trapdoor to get out of fullscreen mode if user hits escape.
      if ((GetMacEventKeyChar(the_event) == '\e') && fullscreen_window) {
        obj->CancelFullscreenDisplay();
        break;
      }  // otherwise fall through
    case autoKey:
    case keyUp: {
      DispatchMacKeyboardEvent(obj, the_event);
      handled = true;
      break;
    }
    case nsPluginEventType_ScrollingBeginsEvent:
      obj->SetScrollIsInProgress(true);
      break;
    case nsPluginEventType_ScrollingEndsEvent:
      obj->SetScrollIsInProgress(false);
      break;
    default:
      break;
  }
  return handled;
}

NPError PlatformNPPGetValue(NPP instance, NPPVariable variable, void *value) {
  return NPERR_INVALID_PARAM;
}

extern "C" {
  NPError InitializePlugin() {
    if (!o3d::SetupOutOfMemoryHandler())
      return NPERR_MODULE_LOAD_FAILED_ERROR;

    InitializeBreakpad();

#ifdef CFTIMER
    gRenderTimer.Start();
#endif  // CFTIMER

    // Initialize the AtExitManager so that base singletons can be
    // destroyed properly.
    g_at_exit_manager.reset(new base::AtExitManager());

    // Turn on the logging.
    CommandLine::Init(0, NULL);
    InitLogging("debug.log",
                logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                logging::DONT_LOCK_LOG_FILE,
                logging::APPEND_TO_OLD_LOG_FILE);

    DLOG(INFO) << "NP_Initialize";

    o3d::SetupOutOfMemoryHandler();

    return NPERR_NO_ERROR;
  }

  NPError OSCALL NP_Initialize(NPNetscapeFuncs* browserFuncs) {
    HANDLE_CRASHES;
    NPError retval = InitializeNPNApi(browserFuncs);
    if (retval != NPERR_NO_ERROR) return retval;
    return InitializePlugin();
  }

  // Wrapper that discards the return value to match the expected type of
  // NPP_ShutdownUPP.
  void NPP_ShutdownWrapper() {
    NP_Shutdown();
  }

  // This code is needed to support browsers based on a slightly dated version
  // of the NPAPI such as Firefox 2, and Camino 1.6. These browsers expect there
  // to be a main() to call to do basic setup.
  int main(NPNetscapeFuncs* browserFuncs,
           NPPluginFuncs* pluginFuncs,
           NPP_ShutdownUPP* shutdownProc) {
    HANDLE_CRASHES;
    NPError error = NP_Initialize(browserFuncs);
    if (error == NPERR_NO_ERROR)
      error = NP_GetEntryPoints(pluginFuncs);
    *shutdownProc = NPP_ShutdownWrapper;

    return error;
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

#ifdef CFTIMER
    gRenderTimer.Stop();
#endif

    // Force all base singletons to be destroyed.
    g_at_exit_manager.reset(NULL);

    ShutdownBreakpad();

    return NPERR_NO_ERROR;
  }

  // Negotiates the best plugin event model, sets the browser to use that,
  // and updates the PluginObject so we can remember which one we chose.
  // We favor the newer Cocoa-based model, but can cope with browsers that
  // only support the original event model, or indeed can't even understand
  // what we are asking for.
  // However, right at the minute, we shun the Cocoa event model because its
  // NPP_SetWindow messages don't contain a WindowRef or NSWindow so we would
  // not get enough info to create our AGL context. We'll go back to
  // preferring Cocoa once we have worked out how to deal with that.
  // Cannot actually fail -
  static void Mac_SetBestEventModel(NPP instance, PluginObject* obj) {
    NPError err = NPERR_NO_ERROR;
    NPEventModel event_model = NPEventModelCarbon;
    NPBool supportsCocoaEventModel = FALSE;
    NPBool supportsCarbonEventModel = FALSE;

    // See if browser supports Cocoa event model.
    err = NPN_GetValue(instance,
                       NPNVsupportsCocoaBool,
                       &supportsCocoaEventModel);
    if (err != NPERR_NO_ERROR) {
      supportsCocoaEventModel = FALSE;
      err = NPERR_NO_ERROR;  // browser doesn't support switchable event models
    }

    // See if browser supports Carbon event model.
    err = NPN_GetValue(instance,
                       NPNVsupportsCarbonBool,
                       &supportsCarbonEventModel);
    if (err != NPERR_NO_ERROR) {
      supportsCarbonEventModel = FALSE;
      err = NPERR_NO_ERROR;
    }

    // Now we've collected our data, the decision phase begins.

    // If we didn't successfully get TRUE for either question, the browser
    // just does not know about the new switchable event models, so must only
    // support the old Carbon event model.
    if (!(supportsCocoaEventModel || supportsCarbonEventModel)) {
      supportsCarbonEventModel = TRUE;
      obj->event_model_ = NPEventModelCarbon;
    }

    // Default to Carbon event model, because the new version of the
    // Cocoa event model spec does not supply sufficient window
    // information in its Cocoa NPP_SetWindow calls for us to bind an
    // AGL context to the browser window.
    NPEventModel model_to_use =
        (supportsCarbonEventModel) ? NPEventModelCarbon : NPEventModelCocoa;
    NPN_SetValue(instance, NPPVpluginEventModel,
                 reinterpret_cast<void*>(model_to_use));
    obj->event_model_ = model_to_use;
  }


  // Negotiates the best plugin drawing model, sets the browser to use that,
  // and updates the PluginObject so we can remember which one we chose.
  // Returns NPERR_NO_ERROR (0) if successful, otherwise an NPError code.
  static NPError Mac_SetBestDrawingModel(NPP instance, PluginObject* obj) {
    NPError err = NPERR_NO_ERROR;
    NPBool supportsCoreGraphics = FALSE;
    NPBool supportsOpenGL = FALSE;
    NPBool supportsQuickDraw = FALSE;
    NPDrawingModel drawing_model = NPDrawingModelQuickDraw;

    // test for direct OpenGL support
    err = NPN_GetValue(instance,
                       NPNVsupportsOpenGLBool,
                       &supportsOpenGL);
    if (err != NPERR_NO_ERROR)
      supportsOpenGL = FALSE;

    // test for QuickDraw support
    err = NPN_GetValue(instance,
                       NPNVsupportsQuickDrawBool,
                       &supportsQuickDraw);
    if (err != NPERR_NO_ERROR)
      supportsQuickDraw = FALSE;

    // Test for Core Graphics support
    err = NPN_GetValue(instance,
                       NPNVsupportsCoreGraphicsBool,
                       &supportsCoreGraphics);
    if (err != NPERR_NO_ERROR)
      supportsCoreGraphics = FALSE;


    // In order of preference. Preference is now determined by compatibility,
    // not by modernity, and so is the opposite of the order I first used.
    if (supportsQuickDraw && !(obj->event_model_ == NPEventModelCocoa)) {
      drawing_model = NPDrawingModelQuickDraw;
    } else if (supportsCoreGraphics) {
      drawing_model = NPDrawingModelCoreGraphics;
    } else if (supportsOpenGL) {
      drawing_model = NPDrawingModelOpenGL;
    } else {
      // This case is for browsers that didn't even understand the question
      // eg FF2, so drawing models are not supported, just assume QuickDraw.
      obj->drawing_model_ = NPDrawingModelQuickDraw;
      return NPERR_NO_ERROR;
    }

    err = NPN_SetValue(instance, NPPVpluginDrawingModel,
                       reinterpret_cast<void*>(drawing_model));
    if (err != NPERR_NO_ERROR)
      return err;

    obj->drawing_model_ = drawing_model;

    return NPERR_NO_ERROR;
  }


  NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                  char* argn[], char* argv[], NPSavedData* saved) {
    HANDLE_CRASHES;

    NPError err = NPERR_NO_ERROR;

    if (!g_logging_initialized) {
      GetUserAgentMetrics(instance);
      GetUserConfigMetrics();
      // Create usage stats logs object
      g_logger = o3d::PluginLogging::InitializeUsageStatsLogging();
      g_logging_initialized = true;
    }

    PluginObject* pluginObject = glue::_o3d::PluginObject::Create(
        instance);
    instance->pdata = pluginObject;
    glue::_o3d::InitializeGlue(instance);
    pluginObject->Init(argc, argn, argv);

    Mac_SetBestEventModel(instance,
                          static_cast<PluginObject*>(instance->pdata));

    err = Mac_SetBestDrawingModel(
        instance, static_cast<PluginObject*>(instance->pdata));
    if (err != NPERR_NO_ERROR)
      return err;
    return NPERR_NO_ERROR;
  }

  NPError NPP_Destroy(NPP instance, NPSavedData** save) {
    HANDLE_CRASHES;
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (obj) {
#if defined(CFTIMER)
      gRenderTimer.RemoveInstance(instance);
#endif

      obj->TearDown();
      NPN_ReleaseObject(obj);
      instance->pdata = NULL;
    }

    return NPERR_NO_ERROR;
  }

  static bool CheckForAGLError() {
    return aglGetError() != AGL_NO_ERROR;
  }


  NPError NPP_SetWindow(NPP instance, NPWindow* window) {
    HANDLE_CRASHES;
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    WindowRef new_window = NULL;

    assert(window != NULL);

    if (window->window == NULL)
      return NPERR_NO_ERROR;

    obj->last_plugin_loc_.h = window->x;
    obj->last_plugin_loc_.v = window->y;

    switch (obj->drawing_model_) {
      case NPDrawingModelOpenGL: {
        NP_GLContext* np_gl = reinterpret_cast<NP_GLContext*>(window->window);
        if (obj->event_model_ == NPEventModelCocoa) {
          NSWindow * ns_window = reinterpret_cast<NSWindow*>(np_gl->window);
          new_window = reinterpret_cast<WindowRef>([ns_window windowRef]);
        } else {
          new_window = np_gl->window;
        }
        obj->mac_2d_context_ = NULL;
        obj->mac_cgl_context_ = np_gl->context;
        break;
      }
      case NPDrawingModelCoreGraphics: {
        // Safari 4 sets window->window to NULL when in Cocoa event mode.
        if (window->window != NULL) {
          NP_CGContext* np_cg = reinterpret_cast<NP_CGContext*>(window->window);
          if (obj->event_model_ == NPEventModelCocoa) {
            NSWindow * ns_window = reinterpret_cast<NSWindow*>(np_cg->window);
            new_window = reinterpret_cast<WindowRef>([ns_window windowRef]);
          } else {
            new_window = np_cg->window;
          }
          obj->mac_2d_context_ = np_cg->context;
        }
        break;
      }
      case NPDrawingModelQuickDraw: {
        NP_Port* np_qd = reinterpret_cast<NP_Port*>(window->window);
        obj->mac_2d_context_ = np_qd->port;
        if (np_qd->port)
          new_window = GetWindowFromPort(np_qd->port);
        break;
      }
      default:
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
        break;
    }

    // Whether the target window has changed.
    bool window_changed = new_window != obj->mac_window_;

    // Whether we already had a window before this call.
    bool had_a_window = obj->mac_window_ != NULL;

    obj->mac_window_ = new_window;

    if (obj->drawing_model_ == NPDrawingModelOpenGL) {
      CGLSetCurrentContext(obj->mac_cgl_context_);
    } else if (!had_a_window && obj->mac_agl_context_ == NULL) {  // setup AGL context
      AGLPixelFormat myAGLPixelFormat = NULL;

    // We need to spec out a few similar but different sets of renderer
    // specifications - define some macros to lessen the duplication.
#define O3D_DEPTH_SETTINGS AGL_RGBA, AGL_DEPTH_SIZE, 24,
#define O3D_STENCIL_SETTINGS AGL_STENCIL_SIZE, 8,
#define O3D_HARDWARE_RENDERER AGL_ACCELERATED, AGL_NO_RECOVERY,
#define O3D_SOFTWARE_RENDERER AGL_RENDERER_ID, AGL_RENDERER_GENERIC_FLOAT_ID,
#define O3D_MULTISAMPLE AGL_MULTISAMPLE, \
    AGL_SAMPLE_BUFFERS_ARB, 1, AGL_SAMPLES_ARB, 4, AGL_MULTISAMPLE,
#define O3D_END AGL_NONE

#ifdef USE_AGL_DOUBLE_BUFFER
#define O3D_DOUBLE_BUFFER_SETTINGS AGL_DOUBLEBUFFER,
#else
#define O3D_DOUBLE_BUFFER_SETTINGS
#endif

      if (!UseSoftwareRenderer()) {
        // Try to create a hardware context with the following
        // specification
        GLint attributes[] = {
          O3D_DEPTH_SETTINGS
          O3D_STENCIL_SETTINGS
          O3D_DOUBLE_BUFFER_SETTINGS
          O3D_HARDWARE_RENDERER
          O3D_MULTISAMPLE
          O3D_END
        };
        myAGLPixelFormat = aglChoosePixelFormat(NULL,
                                                0,
                                                attributes);

        // If this fails, then don't try to be as ambitious
        // (so don't ask for multi-sampling), but still require hardware
        if (myAGLPixelFormat == NULL) {
          GLint low_end_attributes[] = {
            O3D_DEPTH_SETTINGS
            O3D_STENCIL_SETTINGS
            O3D_DOUBLE_BUFFER_SETTINGS
            O3D_HARDWARE_RENDERER
            O3D_END
          };
          myAGLPixelFormat = aglChoosePixelFormat(NULL,
                                                  0,
                                                  low_end_attributes);
        }
      }

      if (myAGLPixelFormat == NULL) {
        // Fallback to software renderer either if the vendorID/gpuID
        // is known to be problematic, or if the hardware context failed
        // to get created
        //
        // Note that we don't request multisampling since it's too slow.
        GLint software_renderer_attributes[] = {
          O3D_DEPTH_SETTINGS
          O3D_STENCIL_SETTINGS
          O3D_DOUBLE_BUFFER_SETTINGS
          O3D_SOFTWARE_RENDERER
          O3D_END
        };
        myAGLPixelFormat = aglChoosePixelFormat(NULL,
                                                0,
                                                software_renderer_attributes);

        obj->SetRendererIsSoftware(true);
      }

      if (myAGLPixelFormat == NULL || CheckForAGLError())
        return NPERR_MODULE_LOAD_FAILED_ERROR;

      obj->mac_agl_context_ = aglCreateContext(myAGLPixelFormat, NULL);

      if (CheckForAGLError())
        return NPERR_MODULE_LOAD_FAILED_ERROR;

      aglDestroyPixelFormat(myAGLPixelFormat);

      if (!SetWindowForAGLContext(obj->mac_agl_context_, obj->mac_window_))
        return NPERR_MODULE_LOAD_FAILED_ERROR;

      aglSetCurrentContext(obj->mac_agl_context_);

      GetOpenGLMetrics();

#ifdef USE_AGL_DOUBLE_BUFFER
      GLint swapInterval = 1;   // request synchronization
      aglSetInteger(obj->mac_agl_context_, AGL_SWAP_INTERVAL, &swapInterval);
#endif
    }

    int clipHeight = window->clipRect.bottom - window->clipRect.top;
    int clipWidth = window->clipRect.right - window->clipRect.left;

    int x_offset = window->clipRect.left - window->x;
    int y_offset = window->clipRect.top - window->y;
    int gl_x_origin = x_offset;
    int gl_y_origin = window->clipRect.bottom - (window->y + window->height);

    // Firefox calls us with an empty cliprect all the time, toggling our
    // cliprect back and forth between empty and the normal value, particularly
    // during scrolling.
    // As we need to make our AGL surface track the clip rect, honoring all of
    // these calls would result in spectacular flashing.
    // The goal here is to try to spot the calls we can safely ignore.
    // The bogus empty cliprects always have left and top of the real clip.
    // A null cliprect should always be honored ({0,0,0,0} means a hidden tab),
    // as should the first ever call to this function,
    // or an attempt to change the window.
    // The call at the end of a scroll step is also honored.
    // Otherwise, skip this change and do not hide our context.
    bool is_empty_cliprect = (clipHeight <= 0 || clipWidth <= 0);
    bool is_null_cliprect = (window->clipRect.bottom == 0 &&
                             window->clipRect.top == 0 &&
                             window->clipRect.left == 0 &&
                             window->clipRect.right == 0);

    if (is_empty_cliprect && (!is_null_cliprect) &&
        had_a_window && (!window_changed) && !obj->ScrollIsInProgress()) {
      return NPERR_NO_ERROR;
    }

    // Workaround - the Apple software renderer crashes if you set the drawing
    // area to 0x0, so use 1x1.
    if (is_empty_cliprect && obj->RendererIsSoftware())
      clipWidth = clipHeight = 1;

    // update size and location of the agl context
    if (obj->mac_agl_context_) {

      // We already had a window, and now we've got a different window -
      // this can happen when the user drags out a tab in Safari into its own
      // window.
      if (had_a_window && window_changed)
        SetWindowForAGLContext(obj->mac_agl_context_, obj->mac_window_);

      aglSetCurrentContext(obj->mac_agl_context_);

      Rect windowRect = {0, 0, 0, 0};
      if (obj->drawing_model_ == NPDrawingModelQuickDraw)
        GetWindowBounds(obj->mac_window_, kWindowContentRgn, &windowRect);
      else
        GetWindowBounds(obj->mac_window_, kWindowStructureRgn, &windowRect);

      int windowHeight = windowRect.bottom - windowRect.top;

      // These are in window coords, the weird bit being that agl wants the
      // location of the bottom of the GL context in y flipped coords,
      // eg 100 would mean the bottom of the GL context was 100 up from the
      // bottom of the window.
      obj->last_buffer_rect_[0] = window->x + x_offset;
      obj->last_buffer_rect_[1] = (windowHeight - (window->y + clipHeight))
                                  - y_offset;  // this param is y flipped
      obj->last_buffer_rect_[2] = clipWidth;
      obj->last_buffer_rect_[3] = clipHeight;
      obj->mac_surface_hidden_ = false;

      // If in fullscreen, just remember the desired change in geometry so
      // we restore to the right rectangle.
      if (obj->GetFullscreenMacWindow() != NULL)
        return NPERR_NO_ERROR;

      aglSetInteger(obj->mac_agl_context_, AGL_BUFFER_RECT,
                      obj->last_buffer_rect_);
      aglEnable(obj->mac_agl_context_, AGL_BUFFER_RECT);
    }

    // Renderer is already initialized from a previous call to this function,
    // just update size and position and return.
    if (had_a_window) {
      if (obj->renderer()) {
        obj->renderer()->SetClientOriginOffset(gl_x_origin, gl_y_origin);
        obj->Resize(window->width, window->height);
      }
      return NPERR_NO_ERROR;
    }

    // Create and assign the graphics context.
    o3d::DisplayWindowMac default_display;
    default_display.set_agl_context(obj->mac_agl_context_);
    default_display.set_cgl_context(obj->mac_cgl_context_);

    obj->CreateRenderer(default_display);

    // if the renderer cannot be created (maybe the features are not supported)
    // then we can proceed no further
    if (!obj->renderer()) {
      if (obj->mac_agl_context_) {
        ::aglDestroyContext(obj->mac_agl_context_);
        obj->mac_agl_context_ = NULL;
      }
    }

    obj->client()->Init();
    obj->client()->SetRenderOnDemandCallback(
        new RenderOnDemandCallbackHandler(obj));

    if (obj->renderer()) {
      obj->renderer()->SetClientOriginOffset(gl_x_origin, gl_y_origin);
      obj->Resize(window->width, window->height);

#ifdef CFTIMER
      // now that the grahics context is setup, add this instance to the timer
      // list so it gets drawn repeatedly
      gRenderTimer.AddInstance(instance);
#endif  // CFTIMER
    }

    return NPERR_NO_ERROR;
  }

  // Called when the browser has finished attempting to stream data to
  // a file as requested. If fname == NULL the attempt was not successful.
  void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
    HANDLE_CRASHES;
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    StreamManager* stream_manager = obj->stream_manager();

    // Some browsers give us an absolute HFS path in fname, some give us an
    // absolute Posix path, so convert to Posix if needed.
    if ((!fname) || (fname[0] == '/') || !fname[0]) {
      stream_manager->SetStreamFile(stream, fname);
    } else {
      const char* converted_fname = CreatePosixFilePathFromHFSFilePath(fname);
      if (converted_fname == NULL)
        return;  // TODO should also log error if we ever get here
      stream_manager->SetStreamFile(stream, converted_fname);
      delete converted_fname;
    }
  }

  int16 NPP_HandleEvent(NPP instance, void* event) {
    HANDLE_CRASHES;
    PluginObject* obj = static_cast<PluginObject*>(instance->pdata);
    if (obj->event_model_ == NPEventModelCarbon) {
      EventRecord* theEvent = static_cast<EventRecord*>(event);
      return HandleMacEvent(theEvent, instance) ? 1 : 0;
    } else if (obj->event_model_ == NPEventModelCocoa){
      return HandleCocoaEvent(instance, (NPCocoaEvent*)event) ? 1 : 0;
    }
    return 0;
  }
};  // end extern "C"
