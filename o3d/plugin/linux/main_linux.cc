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
// the Linux platform.

#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "plugin/cross/main.h"
#include "plugin/cross/out_of_memory.h"

using glue::_o3d::PluginObject;
using glue::StreamManager;
using o3d::DisplayWindowLinux;
using o3d::Event;

namespace {
// We would normally make this a stack variable in main(), but in a
// plugin, that's not possible, so we allocate it dynamically and
// destroy it explicitly.
scoped_ptr<base::AtExitManager> g_at_exit_manager;

bool g_xembed_support = false;

}  // end anonymous namespace

static void DrawPlugin(PluginObject *obj) {
  // Limit drawing to no more than once every timer tick.
  if (!obj->draw_) return;
  obj->client()->RenderClient();
  obj->draw_ = false;
}

void RenderOnDemandCallbackHandler::Run() {
  DrawPlugin(obj_);
}

// Xt support functions

void LinuxTimer(XtPointer data, XtIntervalId* id) {
  PluginObject *obj = static_cast<PluginObject *>(data);
  DCHECK(obj->xt_interval_ == *id);
  obj->client()->Tick();
  obj->draw_ = true;
  if (obj->client()->render_mode() == o3d::Client::RENDERMODE_CONTINUOUS) {
    // NOTE: this draws no matter what instead of just invalidating the region,
    // which means it will execute even if the plug-in window is invisible.
    DrawPlugin(obj);
  }
  obj->xt_interval_ =
      XtAppAddTimeOut(obj->xt_app_context_, 10, LinuxTimer, obj);
}

void LinuxExposeHandler(Widget w,
                        XtPointer user_data,
                        XEvent *event,
                        Boolean *cont) {
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  if (event->type != Expose) return;
  XExposeEvent *expose_event = &event->xexpose;
  DrawPlugin(obj);
}

static int KeySymToDOMKeyCode(KeySym key_sym) {
  // See https://developer.mozilla.org/en/DOM/Event/UIEvent/KeyEvent for the
  // DOM values.
  // X keycodes are not useful, because they describe the geometry, not the
  // associated symbol, so a 'Q' on a QWERTY (US) keyboard has the same keycode
  // as a 'A' on an AZERTY (french) one.
  // Key symbols are closer to what the DOM expects, but they depend on the
  // shift/control/alt combination - the same key has several symbols ('a' vs
  // 'A', '1' vs '!', etc.), so we do extra work so that 'a' and 'A' both
  // generate the same DOM keycode.
  if ((key_sym >= XK_0 && key_sym <= XK_Z)) {
    // DOM keycode matches ASCII value, as does the keysym.
    return key_sym;
  } else if ((key_sym >= XK_a && key_sym <= XK_z)) {
    return key_sym - XK_a + XK_A;
  } else if ((key_sym >= XK_KP_0 && key_sym <= XK_KP_9)) {
    return 0x60 + key_sym - XK_KP_0;
  } else if ((key_sym >= XK_F1 && key_sym <= XK_F24)) {
    return 0x70 + key_sym - XK_F1;
  }
  switch (key_sym) {
    case XK_Cancel:
      return 0x03;
    case XK_Help:
      return 0x06;
    case XK_BackSpace:
      return 0x08;
    case XK_Tab:
      return 0x09;
    case XK_Clear:
      return 0x0C;
    case XK_Return:
      return 0x0D;
    case XK_KP_Enter:
      return 0x0E;
    case XK_Shift_L:
    case XK_Shift_R:
      return 0x10;
    case XK_Control_L:
    case XK_Control_R:
      return 0x11;
    case XK_Alt_L:
    case XK_Alt_R:
      return 0x12;
    case XK_Pause:
      return 0x13;
    case XK_Caps_Lock:
      return 0x14;
    case XK_Escape:
      return 0x1B;
    case XK_space:
      return 0x20;
    case XK_Page_Up:
    case XK_KP_Page_Up:
      return 0x21;
    case XK_Page_Down:
    case XK_KP_Page_Down:
      return 0x22;
    case XK_End:
    case XK_KP_End:
      return 0x23;
    case XK_Home:
    case XK_KP_Home:
      return 0x24;
    case XK_Left:
    case XK_KP_Left:
      return 0x25;
    case XK_Up:
    case XK_KP_Up:
      return 0x26;
    case XK_Right:
    case XK_KP_Right:
      return 0x27;
    case XK_Down:
    case XK_KP_Down:
      return 0x28;
    case XK_Print:
      return 0x2C;
    case XK_Insert:
    case XK_KP_Insert:
      return 0x2D;
    case XK_Delete:
    case XK_KP_Delete:
      return 0x2E;
    case XK_Menu:
      return 0x5D;
    case XK_asterisk:
    case XK_KP_Multiply:
      return 0x6A;
    case XK_plus:
    case XK_KP_Add:
      return 0x6B;
    case XK_underscore:
      return 0x6C;
    case XK_minus:
    case XK_KP_Subtract:
      return 0x6E;
    case XK_KP_Decimal:
      return 0x6E;
    case XK_KP_Divide:
      return 0x6F;
    case XK_Num_Lock:
      return 0x90;
    case XK_Scroll_Lock:
      return 0x91;
    case XK_comma:
      return 0xBC;
    case XK_period:
      return 0xBE;
    case XK_slash:
      return 0xBF;
    case XK_grave:
      return 0xC0;
    case XK_bracketleft:
      return 0xDB;
    case XK_backslash:
      return 0xDC;
    case XK_bracketright:
      return 0xDD;
    case XK_apostrophe:
      return 0xDE;
    case XK_Meta_L:
    case XK_Meta_R:
      return 0xE0;
    default:
      return 0;
  }
}

static int GetXModifierState(int x_state) {
  int modifier_state = 0;
  if (x_state & ControlMask) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (x_state & ShiftMask) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (x_state & Mod1Mask) {
    modifier_state |= Event::MODIFIER_ALT;
  }
  if (x_state & Mod2Mask) {
    modifier_state |= Event::MODIFIER_META;
  }
  return modifier_state;
}

void LinuxKeyHandler(Widget w,
                     XtPointer user_data,
                     XEvent *xevent,
                     Boolean *cont) {
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  XKeyEvent *key_event = &xevent->xkey;
  Event::Type type;
  switch (xevent->type) {
    case KeyPress:
      type = Event::TYPE_KEYDOWN;
      break;
    case KeyRelease:
      type = Event::TYPE_KEYUP;
      break;
    default:
      return;
  }
  Event event(type);
  char char_code = 0;
  KeySym key_sym;
  int result = XLookupString(key_event, &char_code, sizeof(char_code),
                             &key_sym, NULL);
  event.set_key_code(KeySymToDOMKeyCode(key_sym));
  int modifier_state = GetXModifierState(key_event->state);
  event.set_modifier_state(modifier_state);
  obj->client()->AddEventToQueue(event);
  if (xevent->type == KeyPress && result > 0) {
    event.clear_key_code();
    event.set_char_code(char_code);
    event.set_type(Event::TYPE_KEYPRESS);
    obj->client()->AddEventToQueue(event);
  }
}

// TODO: Any way to query the system for the correct value ? According to
// http://library.gnome.org/devel/gdk/stable/gdk-Event-Structures.html GTK uses
// 250ms.
const unsigned int kDoubleClickTime = 250;  // in ms

void LinuxMouseButtonHandler(Widget w,
                             XtPointer user_data,
                             XEvent *xevent,
                             Boolean *cont) {
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  XButtonEvent *button_event = &xevent->xbutton;
  Event::Type type;
  switch (xevent->type) {
    case ButtonPress:
      type = Event::TYPE_MOUSEDOWN;
      break;
    case ButtonRelease:
      type = Event::TYPE_MOUSEUP;
      break;
    default:
      return;
  }
  Event event(type);
  switch (button_event->button) {
    case 1:
      event.set_button(Event::BUTTON_LEFT);
      break;
    case 2:
      event.set_button(Event::BUTTON_MIDDLE);
      break;
    case 3:
      event.set_button(Event::BUTTON_RIGHT);
      break;
    case 4:
    case 5:
      // Mouse wheel. 4 is up, 5 is down. Reported by X as Press/Release.
      // Ignore the Press, report the Release as the wheel event.
      if (xevent->type == ButtonPress)
        return;
      event.set_type(Event::TYPE_WHEEL);
      event.set_delta(0, (button_event->button == 4) ? 1 : -1);
      break;
    default:
      return;
  }
  int modifier_state = GetXModifierState(button_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(button_event->x, button_event->y,
                     button_event->x_root, button_event->y_root,
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  if (event.type() == Event::TYPE_MOUSEUP && obj->in_plugin()) {
    // The event manager automatically generates CLICK from MOUSEDOWN, MOUSEUP.
    if (button_event->time < obj->last_click_time() + kDoubleClickTime) {
      obj->set_last_click_time(0);
      event.set_type(Event::TYPE_DBLCLICK);
      obj->client()->AddEventToQueue(event);
    } else {
      obj->set_last_click_time(button_event->time);
    }
  }
}

void LinuxMouseMoveHandler(Widget w,
                           XtPointer user_data,
                           XEvent *xevent,
                           Boolean *cont) {
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  if (xevent->type != MotionNotify)
    return;
  XMotionEvent *motion_event = &xevent->xmotion;
  Event event(Event::TYPE_MOUSEMOVE);
  int modifier_state = GetXModifierState(motion_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(motion_event->x, motion_event->y,
                     motion_event->x_root, motion_event->y_root,
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
}

void LinuxEnterLeaveHandler(Widget w,
                            XtPointer user_data,
                            XEvent *xevent,
                            Boolean *cont) {
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  switch (xevent->type) {
    case EnterNotify:
      obj->set_in_plugin(true);
      break;
    case LeaveNotify:
      obj->set_in_plugin(false);
      break;
    default:
      return;
  }
}

// XEmbed / GTK support functions
static int GetGtkModifierState(int gtk_state) {
  int modifier_state = 0;
  if (gtk_state & GDK_CONTROL_MASK) {
    modifier_state |= Event::MODIFIER_CTRL;
  }
  if (gtk_state & GDK_SHIFT_MASK) {
    modifier_state |= Event::MODIFIER_SHIFT;
  }
  if (gtk_state & GDK_MOD1_MASK) {
    modifier_state |= Event::MODIFIER_ALT;
  }
#if 0
  // TODO: This code is temporarily disabled until we figure out which exact
  // version of GTK to test for: GDK_META_MASK doesn't exist in older (e.g. 2.8)
  // versions.
  if (gtk_state & GDK_META_MASK) {
    modifier_state |= Event::MODIFIER_META;
  }
#endif
  return modifier_state;
}

static gboolean GtkHandleMouseMove(GtkWidget *widget,
                                   GdkEventMotion *motion_event,
                                   PluginObject *obj) {
  Event event(Event::TYPE_MOUSEMOVE);
  int modifier_state = GetGtkModifierState(motion_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(static_cast<int>(motion_event->x),
                     static_cast<int>(motion_event->y),
                     static_cast<int>(motion_event->x_root),
                     static_cast<int>(motion_event->y_root),
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  return TRUE;
}

static gboolean GtkHandleMouseButton(GtkWidget *widget,
                                     GdkEventButton *button_event,
                                     PluginObject *obj) {
  // On a double-click, Gtk produces: BUTTON_PRESS, BUTTON_RELEASE,
  // BUTTON_PRESS, 2BUTTON_PRESS, BUTTON_RELEASE.
  // JavaScript should receive: down, up, [optional move, ] click, down,
  // up, click, dblclick.
  // The EventManager turns (down, up) into click, since we need that on all
  // platforms.
  // So when a 2BUTTON_PRESS occurs, we keep track of this, so that we can
  // issue a corresponding dblclick when BUTTON_RELEASE comes.
  Event::Button button;
  switch (button_event->button) {
    case 1:
      button = Event::BUTTON_LEFT;
      break;
    case 2:
      button = Event::BUTTON_MIDDLE;
      break;
    case 3:
      button = Event::BUTTON_RIGHT;
      break;
    default:
      return FALSE;
  }
  Event::Type type;
  switch (button_event->type) {
    case GDK_BUTTON_PRESS:
      type = Event::TYPE_MOUSEDOWN;
      break;
    case GDK_BUTTON_RELEASE:
      type = Event::TYPE_MOUSEUP;
      break;
    case GDK_2BUTTON_PRESS:
      obj->got_double_click_[button_event->button - 1] = true;
      return TRUE;
    default:
      return FALSE;
  }
  Event event(type);
  int modifier_state = GetGtkModifierState(button_event->state);
  event.set_modifier_state(modifier_state);
  event.set_button(button);
  event.set_position(static_cast<int>(button_event->x),
                     static_cast<int>(button_event->y),
                     static_cast<int>(button_event->x_root),
                     static_cast<int>(button_event->y_root),
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  if (event.type() == Event::TYPE_MOUSEUP && obj->in_plugin() &&
      obj->got_double_click_[button_event->button - 1]) {
    obj->got_double_click_[button_event->button - 1] = false;
    event.set_type(Event::TYPE_DBLCLICK);
    obj->client()->AddEventToQueue(event);
  }
}

static gboolean GtkHandleKey(GtkWidget *widget,
                             GdkEventKey *key_event,
                             PluginObject *obj) {
  Event::Type type;
  switch (key_event->type) {
    case GDK_KEY_PRESS:
      type = Event::TYPE_KEYDOWN;
      break;
    case GDK_KEY_RELEASE:
      type = Event::TYPE_KEYUP;
      break;
    default:
      return FALSE;
  }
  Event event(type);
  // Logically, GTK events and X events use a different namespace for the
  // various values, but in practice, all the keys we use have the same values,
  // because one of the paths in GTK uses straight X to do the translation. So
  // we can use the same function here.
  int key_code = KeySymToDOMKeyCode(key_event->keyval);
  event.set_key_code(key_code);
  int modifier_state = GetGtkModifierState(key_event->state);
  event.set_modifier_state(modifier_state);
  obj->client()->AddEventToQueue(event);
  int char_code = gdk_keyval_to_unicode(key_event->keyval);
  if (key_event->type == GDK_KEY_PRESS && char_code != 0) {
    event.clear_key_code();
    event.set_char_code(char_code);
    event.set_type(Event::TYPE_KEYPRESS);
    obj->client()->AddEventToQueue(event);
  }
  return TRUE;
}

static gboolean GtkHandleScroll(GtkWidget *widget,
                                GdkEventScroll *scroll_event,
                                PluginObject *obj) {
  Event event(Event::TYPE_WHEEL);
  switch (scroll_event->direction) {
    case GDK_SCROLL_UP:
      event.set_delta(0, 1);
      break;
    case GDK_SCROLL_DOWN:
      event.set_delta(0, -1);
      break;
    case GDK_SCROLL_LEFT:
      event.set_delta(-1, 0);
      break;
    case GDK_SCROLL_RIGHT:
      event.set_delta(1, 0);
      break;
    default:
      return FALSE;
  }
  int modifier_state = GetGtkModifierState(scroll_event->state);
  event.set_modifier_state(modifier_state);
  event.set_position(static_cast<int>(scroll_event->x),
                     static_cast<int>(scroll_event->y),
                     static_cast<int>(scroll_event->x_root),
                     static_cast<int>(scroll_event->y_root),
                     obj->in_plugin());
  obj->client()->AddEventToQueue(event);
  return TRUE;
}

static gboolean GtkEventCallback(GtkWidget *widget,
                                 GdkEvent *event,
                                 gpointer user_data) {
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  switch (event->type) {
    case GDK_EXPOSE:
      if (GTK_WIDGET_DRAWABLE(widget)) {
        obj->draw_ = true;
        DrawPlugin(obj);
      }
      return TRUE;
    case GDK_ENTER_NOTIFY:
      obj->set_in_plugin(true);
      return TRUE;
    case GDK_LEAVE_NOTIFY:
      obj->set_in_plugin(false);
      return TRUE;
    case GDK_MOTION_NOTIFY:
      return GtkHandleMouseMove(widget, &event->motion, obj);
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return GtkHandleMouseButton(widget, &event->button, obj);
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return GtkHandleKey(widget, &event->key, obj);
    case GDK_SCROLL:
      return GtkHandleScroll(widget, &event->scroll, obj);
    default:
      return FALSE;
  }
}

static gboolean GtkTimeoutCallback(gpointer user_data) {
  PluginObject *obj = static_cast<PluginObject *>(user_data);
  obj->draw_ = true;
  obj->client()->Tick();
  if (obj->client()->render_mode() ==
      o3d::Client::RENDERMODE_CONTINUOUS) {
    gtk_widget_queue_draw(obj->gtk_container_);
  }
  return TRUE;
}

bool PluginObject::GetDisplayMode(int id, o3d::DisplayMode *mode) {
  return false;
}

// TODO: Where should this really live?  It's platform-specific, but in
// PluginObject, which mainly lives in cross/o3d_glue.h+cc.
bool  PluginObject::RequestFullscreenDisplay() {
  // TODO: Unimplemented.
  return false;
}

void PluginObject::CancelFullscreenDisplay() {
  // TODO: Unimplemented.
}

NPError PlatformNPPGetValue(NPP instance, NPPVariable variable, void *value) {
  switch (variable) {
    case NPPVpluginNeedsXEmbed:
      *static_cast<NPBool *>(value) = g_xembed_support;
      return NPERR_NO_ERROR;
    default:
      return NPERR_INVALID_PARAM;
  }
  return NPERR_NO_ERROR;
}

extern "C" {
  NPError InitializePlugin() {
    if (!o3d::SetupOutOfMemoryHandler())
      return NPERR_MODULE_LOAD_FAILED_ERROR;

    // Initialize the AtExitManager so that base singletons can be
    // destroyed properly.
    g_at_exit_manager.reset(new base::AtExitManager());

    CommandLine::Init(0, NULL);
    InitLogging("debug.log",
                logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                logging::DONT_LOCK_LOG_FILE,
                logging::APPEND_TO_OLD_LOG_FILE);

    DLOG(INFO) << "NP_Initialize";

    // Check for XEmbed support in the browser.
    NPBool xembed_support = 0;
    NPError err = NPN_GetValue(NULL, NPNVSupportsXEmbedBool, &xembed_support);
    if (err != NPERR_NO_ERROR)
      xembed_support = 0;

    if (xembed_support) {
      // Check for Gtk2 toolkit support in the browser.
      NPNToolkitType toolkit = static_cast<NPNToolkitType>(0);
      err = NPN_GetValue(NULL, NPNVToolkit, &toolkit);
      if (err != NPERR_NO_ERROR || toolkit != NPNVGtk2)
        xembed_support = 0;
    }
    g_xembed_support = xembed_support != 0;

    return NPERR_NO_ERROR;
  }

  NPError OSCALL NP_Initialize(NPNetscapeFuncs *browserFuncs,
                               NPPluginFuncs *pluginFuncs) {
    NPError retval = InitializeNPNApi(browserFuncs);
    if (retval != NPERR_NO_ERROR) return retval;
    NP_GetEntryPoints(pluginFuncs);
    return InitializePlugin();
  }

  NPError OSCALL NP_Shutdown(void) {
    HANDLE_CRASHES;
    DLOG(INFO) << "NP_Shutdown";

    CommandLine::Terminate();

    // Force all base singletons to be destroyed.
    g_at_exit_manager.reset(NULL);

    return NPERR_NO_ERROR;
  }

  NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                  char *argn[], char *argv[], NPSavedData *saved) {
    HANDLE_CRASHES;

    PluginObject* pluginObject = glue::_o3d::PluginObject::Create(
        instance);
    instance->pdata = pluginObject;
    glue::_o3d::InitializeGlue(instance);
    pluginObject->Init(argc, argn, argv);

    // Get the metrics for the system setup
    GetUserConfigMetrics();
    return NPERR_NO_ERROR;
  }

  NPError NPP_Destroy(NPP instance, NPSavedData **save) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    if (obj) {
      if (obj->xt_widget_) {
        // NOTE: This crashes. Not sure why, possibly the widget has
        // already been destroyed, but we haven't received a SetWindow(NULL).
        // XtRemoveEventHandler(obj->xt_widget_, ExposureMask, False,
        //                     LinuxExposeHandler, obj);
        obj->xt_widget_ = NULL;
      }
      if (obj->xt_interval_) {
        XtRemoveTimeOut(obj->xt_interval_);
        obj->xt_interval_ = 0;
      }
      if (obj->timeout_id_) {
        g_source_remove(obj->timeout_id_);
        obj->timeout_id_ = 0;
      }
      if (obj->gtk_container_) {
        gtk_widget_destroy(obj->gtk_container_);
        gtk_widget_unref(obj->gtk_container_);
        obj->gtk_container_ = NULL;
      }
      obj->window_ = 0;
      obj->display_ = NULL;

      obj->TearDown();
      NPN_ReleaseObject(obj);
      instance->pdata = NULL;
    }

    return NPERR_NO_ERROR;
  }


  NPError NPP_SetWindow(NPP instance, NPWindow *window) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);

    NPSetWindowCallbackStruct *cb_struct =
        static_cast<NPSetWindowCallbackStruct *>(window->ws_info);
    Window xwindow = reinterpret_cast<Window>(window->window);
    if (xwindow != obj->window_) {
      Display *display = cb_struct->display;
      Window drawable = xwindow;
      if (g_xembed_support) {
        // We asked for a XEmbed plugin, the xwindow is a GtkSocket, we create
        // a GtkPlug to go into it.
        obj->gtk_container_ = gtk_plug_new(xwindow);
        gtk_widget_set_double_buffered(obj->gtk_container_, FALSE);
        gtk_widget_add_events(obj->gtk_container_,
                              GDK_BUTTON_PRESS_MASK |
                              GDK_BUTTON_RELEASE_MASK |
                              GDK_SCROLL_MASK |
                              GDK_KEY_PRESS_MASK |
                              GDK_KEY_RELEASE_MASK |
                              GDK_POINTER_MOTION_MASK |
                              GDK_EXPOSURE_MASK |
                              GDK_ENTER_NOTIFY_MASK |
                              GDK_LEAVE_NOTIFY_MASK);
        g_signal_connect(G_OBJECT(obj->gtk_container_), "event",
                         G_CALLBACK(GtkEventCallback), obj);
        gtk_widget_show(obj->gtk_container_);
        drawable = GDK_WINDOW_XID(obj->gtk_container_->window);
        obj->timeout_id_ = g_timeout_add(10, GtkTimeoutCallback, obj);
      } else {
        // No XEmbed support, the xwindow is a Xt Widget.
        Widget widget = XtWindowToWidget(display, xwindow);
        if (!widget) {
          DLOG(ERROR) << "window is not a Widget";
          return NPERR_MODULE_LOAD_FAILED_ERROR;
        }
        obj->xt_widget_ = widget;
        XtAddEventHandler(widget, ExposureMask, 0, LinuxExposeHandler, obj);
        XtAddEventHandler(widget, KeyPressMask|KeyReleaseMask, 0,
                          LinuxKeyHandler, obj);
        XtAddEventHandler(widget, ButtonPressMask|ButtonReleaseMask, 0,
                          LinuxMouseButtonHandler, obj);
        XtAddEventHandler(widget, PointerMotionMask, 0,
                          LinuxMouseMoveHandler, obj);
        XtAddEventHandler(widget, EnterWindowMask|LeaveWindowMask, 0,
                          LinuxEnterLeaveHandler, obj);
        obj->xt_app_context_ = XtWidgetToApplicationContext(widget);
        obj->xt_interval_ =
            XtAppAddTimeOut(obj->xt_app_context_, 10, LinuxTimer, obj);
      }

      // Create and assign the graphics context.
      o3d::DisplayWindowLinux default_display;
      default_display.set_display(display);
      default_display.set_window(drawable);

      obj->CreateRenderer(default_display);
      obj->client()->Init();
      obj->client()->SetRenderOnDemandCallback(
          new RenderOnDemandCallbackHandler(obj));
      obj->display_ = display;
      obj->window_ = xwindow;
    }
    obj->Resize(window->width, window->height);

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
};  // end extern "C"
