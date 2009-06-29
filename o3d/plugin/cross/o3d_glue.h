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


#ifndef O3D_PLUGIN_CROSS_O3D_GLUE_H_
#define O3D_PLUGIN_CROSS_O3D_GLUE_H_

#include <build/build_config.h>

#ifdef OS_MACOSX
#include <OpenGL/OpenGL.h>
#include <AGL/agl.h>
#endif

#ifdef OS_LINUX
#include <GL/glx.h>
#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#endif


#include <map>
#include <set>
#include <string>
#include <vector>
#include "base/scoped_ptr.h"
#include "base/cross/std_hash.h"
#include "core/cross/display_mode.h"
#include "core/cross/display_window.h"
#include "core/cross/object_base.h"
#include "core/cross/service_locator.h"
#include "core/cross/evaluation_counter.h"
#include "core/cross/class_manager.h"
#include "core/cross/features.h"
#include "core/cross/object_manager.h"
#include "core/cross/error.h"
#include "core/cross/profiler.h"
#include "plugin/cross/np_v8_bridge.h"
#include "client_glue.h"
#include "third_party/nixysa/files/static_glue/npapi/common.h"

namespace o3d {
class Client;
class Renderer;
}

namespace glue {
class StreamManager;

namespace _o3d {
using o3d::Id;
using o3d::ObjectBase;
using o3d::Client;
using o3d::ClassManager;
using o3d::EvaluationCounter;
using o3d::Features;
using o3d::EvaluationCounter;
using o3d::ObjectManager;
using o3d::Profiler;
using o3d::Renderer;
using o3d::ServiceLocator;

class NPAPIObject: public NPObject {
  NPP npp_;
  Id id_;
  bool mapped_;
 public:
  explicit NPAPIObject(NPP npp): npp_(npp), id_(0), mapped_(false) {}
  void Initialize(const ObjectBase *object) { id_ = object->id(); }
  NPP npp() const { return npp_; }
  Id id() const { return id_; }
  bool mapped() const { return mapped_; }
  void set_mapped(bool mapped) { mapped_ = mapped; }
};
void RegisterType(NPP npp, const ObjectBase::Class *clientclass,
                  NPClass *npclass);
bool CheckObject(NPP npp, NPObject *npobject,
                 const ObjectBase::Class *clientclass);
NPAPIObject *GetNPObject(NPP npp, ObjectBase *object);
NPObject *Allocate(NPP npp, NPClass *npclass);
void Deallocate(NPObject *object);
ServiceLocator *GetServiceLocator(NPP npp);
Client *GetClient(NPP npp);
void InitializeGlue(NPP npp);

typedef glue::namespace_o3d::class_Client::NPAPIObject ClientNPObject;

class PluginObject: public NPObject {
  typedef o3d::base::hash_map<Id, NPAPIObject *> ClientObjectMap;
  typedef o3d::base::hash_map<const ObjectBase::Class *, NPClass *>
      ClientToNPClassMap;
  typedef o3d::base::hash_map<NPClass *, const ObjectBase::Class *>
      NPToClientClassMap;

  NPP npp_;
  ServiceLocator service_locator_;
  EvaluationCounter evaluation_counter_;
  ClassManager class_manager_;
  ObjectManager object_manager_;
  Profiler profiler_;
  bool fullscreen_;  // Are we rendered fullscreen or in the plugin region?
  Renderer *renderer_;
  Client *client_;
  Features* features_;
  ClientObjectMap object_map_;
  ClientToNPClassMap client_to_np_class_map_;
  NPToClientClassMap np_to_client_class_map_;
  NPObject *globals_npobject_;
  ClientNPObject *client_npobject_;
  std::string user_agent_;
  Renderer::InitStatus renderer_init_status_;
  // The current cursor type.
  o3d::Cursor::CursorType cursor_type_;

  // Last known dimensions of plugin
  int prev_width_;
  int prev_height_;

  o3d::NPV8Bridge np_v8_bridge_;
  scoped_ptr<StreamManager> stream_manager_;

 public:
#ifdef OS_WIN
  void SetDefaultPluginWindowProc(WNDPROC proc) {
    default_plugin_window_proc_ = proc;
  }
  WNDPROC GetDefaultPluginWindowProc() {
    return default_plugin_window_proc_;
  }
  void SetHWnd(HWND hWnd) {
    hWnd_ = hWnd;
  }
  HWND GetHWnd() {
    return hWnd_;
  }
  void SetFullscreenHWnd(HWND hWnd) {
    fullscreen_hWnd_ = hWnd;
  }
  HWND GetFullscreenHWnd() {
    return fullscreen_hWnd_;
  }
  void SetParentHWnd(HWND hWnd) {
    parent_hWnd_ = hWnd;
  }
  HWND GetParentHWnd() {
    return parent_hWnd_;
  }
  void SetPluginHWnd(HWND hWnd) {
    plugin_hWnd_ = hWnd;
  }
  HWND GetPluginHWnd() {
    return plugin_hWnd_;
  }
  bool RecordPaint() {
    bool painted = painted_once_;
    painted_once_ = true;
    return painted;
  }
  bool got_dblclick() const { return got_dblclick_; }
  void set_got_dblclick(bool got_dblclick) { got_dblclick_ = got_dblclick; }
#endif
#ifdef OS_MACOSX
  void SetFullscreenOverlayMacWindow(WindowRef window) {
    mac_fullscreen_overlay_window_ = window;
  }

  WindowRef GetFullscreenOverlayMacWindow() {
    return mac_fullscreen_overlay_window_;
  }

  void SetFullscreenMacWindow(WindowRef window) {
    mac_fullscreen_window_ = window;
  }

  WindowRef GetFullscreenMacWindow() {
    return mac_fullscreen_window_;
  }

  // Always returns |true| if RENDERMODE_CONTINUOUS, otherwise
  // only if client->render() has been called and we haven't yet
  // handled it
  bool WantsRedraw();
  void SetWantsRedraw(bool wants) { wants_redraw_ = wants; }

  bool ScrollIsInProgress() { return scroll_is_in_progress_; }
  void SetScrollIsInProgress(bool state) { scroll_is_in_progress_ = state; }
  bool scroll_is_in_progress_;

  bool RendererIsSoftware() {return renderer_is_software_;}
  bool SetRendererIsSoftware(bool state) {renderer_is_software_ = state;}
  bool renderer_is_software_;

  NPDrawingModel drawing_model_;
  NPEventModel event_model_;
  WindowRef mac_window_;  // may be NULL in the Chrome case
  // these vars needed for the Safari tab switch detection hack
  CFDateRef last_mac_event_time_;
  bool wants_redraw_;
  void * mac_cocoa_window_;
  void* mac_window_selected_tab_;
  bool mac_surface_hidden_;
  // end of Safari tab detection vars
  GLint last_buffer_rect_[4];
  Point last_plugin_loc_;
  // can be a CGrafPtr, a CGContextRef or NULL depending on drawing_model
  void* mac_2d_context_;
  // either can be NULL depending on drawing_model
  AGLContext mac_agl_context_;
  CGLContextObj mac_cgl_context_;

  // Fullscreen related stuff.

  // FullscreenIdle gets repeatedly called while we are in fullscreen mode.
  // Currently its only task is to hide the fullscreen message at the right
  // time.
  void FullscreenIdle();
  double  time_to_hide_overlay_;
  WindowRef mac_fullscreen_window_;  // NULL if not in fullscreen modee
  WindowRef mac_fullscreen_overlay_window_;  // NULL if not in fullscreen mode
  Ptr mac_fullscreen_state_;

#endif  //  OS_MACOSX
#ifdef OS_LINUX
  Display *display_;
  Window window_;

  // Xt mode
  Widget xt_widget_;
  XtAppContext xt_app_context_;
  XtIntervalId xt_interval_;
  Time last_click_time_;

  // XEmbed mode
  GtkWidget *gtk_container_;
  bool got_double_click_[3];
  guint timeout_id_;

  bool draw_;
  bool in_plugin_;
#endif
  explicit PluginObject(NPP npp);
  ~PluginObject();
  void Init(int argc, char* argn[], char* argv[]);
  void TearDown();
  ServiceLocator* service_locator() { return &service_locator_; }
  Client *client() { return client_; }
  Renderer *renderer() { return renderer_; }
  NPP npp() { return npp_; }
  void RegisterType(const ObjectBase::Class *clientclass, NPClass *npclass);
  bool CheckObject(NPObject *npobject,
                   const ObjectBase::Class *clientclass) const;
  NPAPIObject *GetNPObject(ObjectBase *object);
  void UnmapObject(NPAPIObject *npobject);
  void UnmapAll();
  NPClass *GetNPClass(const ObjectBase::Class *clientclass);

  NPObject *globals_npobject() { return globals_npobject_; }
  ClientNPObject *client_npobject() { return client_npobject_; }

  static const char* kOnRenderEventName;

  static PluginObject *Create(NPP npp);

  StreamManager *stream_manager() const { return stream_manager_.get(); }

  static void LogAssertHandlerFunction(const std::string& str);

  Renderer::InitStatus renderer_init_status() const {
    return renderer_init_status_;
  }

  // Plugin has been resized.
  void Resize(int width, int height);

  int width() const;
  int height() const;

  // Fullscreen stuff
  bool fullscreen() const {
    return fullscreen_;
  }

  // Fetch one mode by externally visible id, returning true on success.
  bool GetDisplayMode(int id, o3d::DisplayMode *mode);

  // Get a vector of the available fullscreen display modes.
  // Clears *modes on error.
  void GetDisplayModes(std::vector<o3d::DisplayMode> *modes);

  // This isn't exposed directly to JavaScript for security reasons; it gets
  // called by the platform-specific event-handling code if the region set by
  // SetFullscreenClickRegion is clicked.  It requests the mode previously set
  // by SetFullscreenClickRegion(), and fails if there wasn't one.
  bool RequestFullscreenDisplay();

  // Make a region of the plugin area that will invoke fullscreen mode if
  // clicked.  The app developer is responsible for communicating this to the
  // user, as this region has no visible marker.  The user is also responsible
  // for updating this region if the plugin gets resized, as we don't know
  // whether or how to scale it.
  // Fails if the mode_id supplied isn't valid.  Returns true on success.
  bool SetFullscreenClickRegion(int x, int y, int width, int height,
      int mode_id);
  void ClearFullscreenClickRegion() {
    fullscreen_region_valid_ = false;
  }

  // Tests a mouse click location for a hit on the fullscreen click region.
  bool HitFullscreenClickRegion(int x, int y) {
    if (!fullscreen_region_valid_ || fullscreen_) {
      return false;
    }
    return (x >= fullscreen_region_x_) &&
        (x - fullscreen_region_x_ < fullscreen_region_width_) &&
        (y >= fullscreen_region_y_) &&
        (y - fullscreen_region_y_ < fullscreen_region_height_);
  }

  // Deactivate fullscreen mode.
  void CancelFullscreenDisplay();

  // Redirect by setting window.location to file:///<url>.
  void RedirectToFile(const char *url);

  // Get the cursor's shape.
  o3d::Cursor::CursorType cursor() const;

  // Set the cursor's shape.
  void set_cursor(o3d::Cursor::CursorType cursor_type);

  // Sets the cursor to whatever the current cursor is.
  void PlatformSpecificSetCursor();

  const std::string& user_agent() const { return user_agent_; }
  bool IsFirefox() const {
    return user_agent_.find("Firefox") != user_agent_.npos;
  }
  bool IsChrome() const {
    return user_agent_.find("Chrome") != user_agent_.npos;
  }
  bool IsMSIE() const {
    return user_agent_.find("MSIE") != user_agent_.npos;
  }

  v8::Handle<v8::Context> script_context() {
    return np_v8_bridge_.script_context();
  }

  o3d::NPV8Bridge* np_v8_bridge() { return &np_v8_bridge_; }

  // Creates the renderer.
  void CreateRenderer(const o3d::DisplayWindow& display_window);

  // Deletes the renderer.
  void DeleteRenderer();

#ifdef OS_MACOSX
  // Methods needed for the Safari tab-switching hack.
  void MacEventReceived();

  CFTimeInterval TimeSinceLastMacEvent();

  bool DetectTabHiding();
#endif

#ifdef OS_WIN
  // Stashes a pointer to the plugin on the HWND, and keeps a corresponding
  // pointer to the HWND on the plugin.  If the plugin's already registered with
  // an HWND, this will clear out the plugin pointer on the old HWND before
  // setting up the new one.
  static void StorePluginProperty(HWND hWnd, PluginObject *obj);
  // Similar to StorePluginProperty, but doesn't clear out any old pointers.
  static void StorePluginPropertyUnsafe(HWND hWnd, PluginObject *obj);
  // Looks up the plugin stored on this HWND.
  static PluginObject *GetPluginProperty(HWND hWnd);
  // Clears out the plugin stored on this HWND.
  static void ClearPluginProperty(HWND hWnd);
  // WNDCLASSEX used for constructing fullscreen windows.
  static WNDCLASSEX *GetFullscreenWindowClass(
      HINSTANCE instance, WNDPROC window_proc);
  // One bit of state that helps us turn the sequence of events that windows
  // produces on a double click into what JavaScript is expecting.
  bool got_dblclick_;
#endif  // OS_WIN
#ifdef OS_LINUX
  bool in_plugin() const { return in_plugin_; }
  void set_in_plugin(bool value) { in_plugin_ = value; }
  Time last_click_time() const { return last_click_time_; }
  void set_last_click_time(Time value) { last_click_time_ = value; }
#endif

 private:
  bool fullscreen_region_valid_;
  int fullscreen_region_x_;
  int fullscreen_region_y_;
  int fullscreen_region_width_;
  int fullscreen_region_height_;
  int fullscreen_region_mode_id_;
#ifdef OS_WIN
  HWND hWnd_;  // The window we are currenlty drawing to (use this)
  HWND fullscreen_hWnd_;  // The fullscreen window if we are fullscreen
  HWND parent_hWnd_;
  HWND plugin_hWnd_;  // The window we were given inside the browser.
  HCURSOR cursors_[o3d::Cursor::NUM_CURSORS];  // loaded windows cursors.
  HCURSOR hCursor_;
  WNDPROC default_plugin_window_proc_;
  static WNDCLASSEX fullscreen_class_;
  static bool fullscreen_class_registered_;
  bool painted_once_;
#endif  // OS_WIN
};

}  // namespace o3d
}  // namespace glue


#endif  // O3D_PLUGIN_CROSS_O3D_GLUE_H_
