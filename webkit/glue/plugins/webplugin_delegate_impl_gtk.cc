// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HACK: we need this #define in place before npapi.h is included for
// plugins to work. However, all sorts of headers include npapi.h, so
// the only way to be certain the define is in place is to put it
// here.  You might ask, "Why not set it in npapi.h directly, or in
// this directory's SConscript, then?"  but it turns out this define
// makes npapi.h include Xlib.h, which in turn defines a ton of symbols
// like None and Status, causing conflicts with the aforementioned
// many headers that include npapi.h.  Ugh.
// See also plugin_host.cc.
#define MOZ_X11 1

#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include <string>
#include <vector>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
// #include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

WebPluginDelegate* WebPluginDelegate::Create(
    const FilePath& filename,
    const std::string& mime_type,
    gfx::NativeView containing_view) {
  scoped_refptr<NPAPI::PluginLib> plugin =
      NPAPI::PluginLib::CreatePluginLib(filename);
  if (plugin.get() == NULL)
    return NULL;

  NPError err = plugin->NP_Initialize();
  if (err != NPERR_NO_ERROR)
    return NULL;

  scoped_refptr<NPAPI::PluginInstance> instance =
      plugin->CreateInstance(mime_type);
  return new WebPluginDelegateImpl(containing_view, instance.get());
}

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::NativeView containing_view,
    NPAPI::PluginInstance *instance)
    :
      windowed_handle_(0),
      windowed_did_set_window_(false),
      windowless_(false),
      plugin_(NULL),
      instance_(instance),
      parent_(containing_view),
      quirks_(0)
 {
  memset(&window_, 0, sizeof(window_));

}

WebPluginDelegateImpl::~WebPluginDelegateImpl() {
  DestroyInstance();

  if (!windowless_)
    WindowedDestroyWindow();
}

void WebPluginDelegateImpl::PluginDestroyed() {
  delete this;
}

bool WebPluginDelegateImpl::Initialize(const GURL& url,
                                       char** argn,
                                       char** argv,
                                       int argc,
                                       WebPlugin* plugin,
                                       bool load_manually) {
  plugin_ = plugin;

  instance_->set_web_plugin(plugin);
  NPAPI::PluginInstance* old_instance =
      NPAPI::PluginInstance::SetInitializingInstance(instance_);

  bool start_result = instance_->Start(url, argn, argv, argc, load_manually);

  NPAPI::PluginInstance::SetInitializingInstance(old_instance);

  if (!start_result)
    return false;

  windowless_ = instance_->windowless();
  if (windowless_) {
    // For windowless plugins we should set the containing window handle
    // as the instance window handle. This is what Safari does. Not having
    // a valid window handle causes subtle bugs with plugins which retreive
    // the window handle and validate the same. The window handle can be
    // retreived via NPN_GetValue of NPNVnetscapeWindow.
    NOTIMPLEMENTED() << "windowless not implemented";
    return false;
    // instance_->set_window_handle(parent_);
    // CreateDummyWindowForActivation();
    // handle_event_pump_messages_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  } else {
    if (!WindowedCreatePlugin())
      return false;
  }

  plugin->SetWindow(windowed_handle_, /* unused event param */ NULL);
  plugin_url_ = url.spec();

  return true;
}

void WebPluginDelegateImpl::DestroyInstance() {
  if (instance_ && (instance_->npp()->ndata != NULL)) {
    // Shutdown all streams before destroying so that
    // no streams are left "in progress".  Need to do
    // this before calling set_web_plugin(NULL) because the
    // instance uses the helper to do the download.
    instance_->CloseStreams();

    // TODO(evanm): I played with this for quite a while but couldn't
    // figure out a way to make Flash not crash unless I didn't call
    // NPP_SetWindow.  Perhaps it just should be marked with the quirk
    // that wraps the NPP_SetWindow call.
    // window_.window = NULL;
    // if (!(quirks_ & PLUGIN_QUIRK_DONT_SET_NULL_WINDOW_HANDLE_ON_DESTROY)) {
    //   instance_->NPP_SetWindow(&window_);
    // }

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    instance_ = 0;
  }
}

void WebPluginDelegateImpl::UpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (windowless_) {
    WindowlessUpdateGeometry(window_rect, clip_rect);
  } else {
    WindowedUpdateGeometry(window_rect, clip_rect);
  }
}

void WebPluginDelegateImpl::Paint(void* dc, const gfx::Rect& rect) {
  if (windowless_) {
    // TODO(port): windowless painting.
    // WindowlessPaint(dc, rect);
  }
}

void WebPluginDelegateImpl::Print(void* dc) {
  NOTIMPLEMENTED();
}

NPObject* WebPluginDelegateImpl::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegateImpl::DidFinishLoadWithReason(NPReason reason) {
  instance()->DidFinishLoadWithReason(reason);
}

int WebPluginDelegateImpl::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return base::GetCurrentProcId();
}

void WebPluginDelegateImpl::SendJavaScriptStream(const std::string& url,
                                                 const std::wstring& result,
                                                 bool success,
                                                 bool notify_needed,
                                                 int notify_data) {
  instance()->SendJavaScriptStream(url, result, success, notify_needed,
                                   notify_data);
}

void WebPluginDelegateImpl::DidReceiveManualResponse(
    const std::string& url, const std::string& mime_type,
    const std::string& headers, uint32 expected_length, uint32 last_modified) {
  if (!windowless_) {
    // Calling NPP_WriteReady before NPP_SetWindow causes movies to not load in
    // Flash.  See http://b/issue?id=892174.
    DCHECK(windowed_did_set_window_);
  }

  instance()->DidReceiveManualResponse(url, mime_type, headers,
                                       expected_length, last_modified);
}

void WebPluginDelegateImpl::DidReceiveManualData(const char* buffer,
                                                 int length) {
  instance()->DidReceiveManualData(buffer, length);
}

void WebPluginDelegateImpl::DidFinishManualLoading() {
  instance()->DidFinishManualLoading();
}

void WebPluginDelegateImpl::DidManualLoadFail() {
  instance()->DidManualLoadFail();
}

FilePath WebPluginDelegateImpl::GetPluginPath() {
  return instance()->plugin_lib()->plugin_info().path;
}

void WebPluginDelegateImpl::InstallMissingPlugin() {
  /* XXX NPEvent evt;
  evt.event = PluginInstallerImpl::kInstallMissingPluginMessage;
  evt.lParam = 0;
  evt.wParam = 0;
  instance()->NPP_HandleEvent(&evt); */
}

void WebPluginDelegateImpl::WindowedUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (WindowedReposition(window_rect, clip_rect) ||
      !windowed_did_set_window_) {
    // Let the plugin know that it has been moved
    WindowedSetWindow();
  }
}

namespace {

// This is just a GtkSocket, with size_request overridden, so that we always
// control the size of the widget.
class GtkFixedSocket {
 public:
  // Create a new instance of our GTK widget object.
  static GtkWidget* CreateNewWidget() {
    return GTK_WIDGET(g_object_new(GetType(), NULL));
  }

 private:
  // Create and register our custom container type with GTK.
  static GType GetType() {
    static GType type = 0;  // We only want to register our type once.
    if (!type) {
      static const GTypeInfo info = {
        sizeof(GtkSocketClass),
        NULL, NULL,
        static_cast<GClassInitFunc>(&ClassInit),
        NULL, NULL,
        sizeof(GtkSocket),  // We are identical to a GtkSocket.
        0, NULL,
      };
      type = g_type_register_static(GTK_TYPE_SOCKET,
                                    "GtkFixedSocket",
                                    &info,
                                    static_cast<GTypeFlags>(0));
    }
    return type;
  }

  // Implementation of the class initializer.
  static void ClassInit(gpointer klass, gpointer class_data_unusued) {
    GtkWidgetClass* widget_class = reinterpret_cast<GtkWidgetClass*>(klass);
    widget_class->size_request = &HandleSizeRequest;
  }

  // Report our allocation size during size requisition.  This means we control
  // the size, from calling gtk_widget_size_allocate in WindowedReposition().
  static void HandleSizeRequest(GtkWidget* widget,
                                GtkRequisition* requisition) {
    requisition->width = widget->allocation.width;
    requisition->height = widget->allocation.height;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(GtkFixedSocket);
};

gboolean PlugRemovedCallback(GtkSocket* socket) {
  // This is called when the other side of the socket goes away.
  // We return TRUE to indicate that we don't want to destroy our side.
  return TRUE;
}

}  // namespace

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  DCHECK(!windowed_handle_);

  bool xembed;
  NPError err = instance_->NPP_GetValue(NPPVpluginNeedsXEmbed, &xembed);
  DCHECK(err == NPERR_NO_ERROR);
  if (!xembed) {
    NOTIMPLEMENTED() << "Windowed plugin but without xembed.";
    return false;
  }

  windowed_handle_ = GtkFixedSocket::CreateNewWidget();
  g_signal_connect(GTK_SOCKET(windowed_handle_), "plug-removed",
                   G_CALLBACK(PlugRemovedCallback), NULL);
  gtk_container_add(GTK_CONTAINER(parent_), windowed_handle_);
  // TODO(evanm): connect to signals on the socket, like when the other side
  // goes away.

  gtk_widget_show(windowed_handle_);
  gtk_widget_realize(windowed_handle_);

  window_.window = GINT_TO_POINTER(
      gtk_socket_get_id(GTK_SOCKET(windowed_handle_)));

  NPSetWindowCallbackStruct* extra = new NPSetWindowCallbackStruct;
  extra->display = GDK_WINDOW_XDISPLAY(windowed_handle_->window);
  GdkVisual* visual = gdk_drawable_get_visual(windowed_handle_->window);
  extra->visual = GDK_VISUAL_XVISUAL(visual);
  extra->depth = visual->depth;
  extra->colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(windowed_handle_->window));
  window_.ws_info = extra;

  return true;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  if (windowed_handle_ != NULL) {
    gtk_widget_destroy(windowed_handle_);
    windowed_handle_ = NULL;
  }
}

bool WebPluginDelegateImpl::WindowedReposition(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  if (!windowed_handle_) {
    NOTREACHED();
    return false;
  }

  if (window_rect_ == window_rect && clip_rect_ == clip_rect)
    return false;


  if (window_rect.size() != window_rect_.size()) {
    // Clipping is handled by WebPlugin.
    GtkAllocation allocation = { window_rect.x(), window_rect.y(),
                                 window_rect.width(), window_rect.height() };
    // TODO(deanm): we probably want to match Windows here, where x and y is
    // fixed at 0, and we're just sizing the window.
    // Tell our parent GtkFixed container where to place the widget.
    gtk_fixed_move(
        GTK_FIXED(parent_), windowed_handle_, window_rect.x(), window_rect.y());
    gtk_widget_size_allocate(windowed_handle_, &allocation);
  }

  window_rect_ = window_rect;
  clip_rect_ = clip_rect;

  // TODO(deanm): Is this really needed?
  // Ensure that the entire window gets repainted.
  gtk_widget_queue_draw(windowed_handle_);

  return true;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  if (!instance_)
    return;

  if (!windowed_handle_) {
    NOTREACHED();
    return;
  }

  // XXX instance()->set_window_handle(windowed_handle_);

  DCHECK(!instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();

  //window_.window = windowed_handle_;
  window_.type = NPWindowTypeWindow;

  // Reset this flag before entering the instance in case of side-effects.
  windowed_did_set_window_ = true;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;
  /*
  // Set this flag before entering the instance in case of side-effects.
  windowless_needs_set_window_ = true;

  // We will inform the instance of this change when we call NPP_SetWindow.
  clip_rect_ = clip_rect;
  cutout_rects_.clear();

  if (window_rect_ != window_rect) {
    window_rect_ = window_rect;

    WindowlessSetWindow(true);

    WINDOWPOS win_pos = {0};
    win_pos.x = window_rect_.x();
    win_pos.y = window_rect_.y();
    win_pos.cx = window_rect_.width();
    win_pos.cy = window_rect_.height();

    NPEvent pos_changed_event;
    pos_changed_event.event = WM_WINDOWPOSCHANGED;
    pos_changed_event.wParam = 0;
    pos_changed_event.lParam = PtrToUlong(&win_pos);

    instance()->NPP_HandleEvent(&pos_changed_event);
  }
  */
}

#if 0
void WebPluginDelegateImpl::WindowlessPaint(HDC hdc,
                                            const gfx::Rect& damage_rect) {
  DCHECK(hdc);

  RECT damage_rect_win;
  damage_rect_win.left   = damage_rect.x();  // + window_rect_.x();
  damage_rect_win.top    = damage_rect.y();  // + window_rect_.y();
  damage_rect_win.right  = damage_rect_win.left + damage_rect.width();
  damage_rect_win.bottom = damage_rect_win.top + damage_rect.height();

  // We need to pass the HDC to the plugin via NPP_SetWindow in the
  // first paint to ensure that it initiates rect invalidations.
  if (window_.window == NULL)
    windowless_needs_set_window_ = true;

  window_.window = hdc;
  // TODO(darin): we should avoid calling NPP_SetWindow here since it may
  // cause page layout to be invalidated.

  // We really don't need to continually call SetWindow.
  // m_needsSetWindow flags when the geometry has changed.
  if (windowless_needs_set_window_)
    WindowlessSetWindow(false);

  NPEvent paint_event;
  paint_event.event = WM_PAINT;
  // NOTE: NPAPI is not 64bit safe.  It puts pointers into 32bit values.
  paint_event.wParam = PtrToUlong(hdc);
  paint_event.lParam = PtrToUlong(&damage_rect_win);
  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);
  instance()->NPP_HandleEvent(&paint_event);
}
#endif

void WebPluginDelegateImpl::WindowlessSetWindow(bool force_set_window) {
  if (!instance())
    return;

  if (window_rect_.IsEmpty())  // wait for geometry to be set.
    return;

  DCHECK(instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();
  window_.type = NPWindowTypeDrawable;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::SetFocus() {
  DCHECK(instance()->windowless());

  NOTIMPLEMENTED();
  /*  NPEvent focus_event;
  focus_event.event = WM_SETFOCUS;
  focus_event.wParam = 0;
  focus_event.lParam = 0;

  instance()->NPP_HandleEvent(&focus_event);*/
}

bool WebPluginDelegateImpl::HandleEvent(NPEvent* event,
                                        WebCursor* cursor) {
  NOTIMPLEMENTED();
#if 0
  DCHECK(windowless_) << "events should only be received in windowless mode";
  DCHECK(cursor != NULL);

  // To ensure that the plugin receives keyboard events we set focus to the
  // dummy window.
  // TODO(iyengar) We need a framework in the renderer to identify which
  // windowless plugin is under the mouse and to handle this. This would
  // also require some changes in RenderWidgetHost to detect this in the
  // WM_MOUSEACTIVATE handler and inform the renderer accordingly.
  HWND prev_focus_window = NULL;
  if (event->event == WM_RBUTTONDOWN) {
    prev_focus_window = ::SetFocus(dummy_window_for_activation_);
  }

  if (ShouldTrackEventForModalLoops(event)) {
    // A windowless plugin can enter a modal loop in a NPP_HandleEvent call.
    // For e.g. Flash puts up a context menu when we right click on the
    // windowless plugin area. We detect this by setting up a message filter
    // hook pror to calling NPP_HandleEvent on the plugin and unhook on
    // return from NPP_HandleEvent. If the plugin does enter a modal loop
    // in that context we unhook on receiving the first notification in
    // the message filter hook.
    handle_event_message_filter_hook_ =
        SetWindowsHookEx(WH_MSGFILTER, HandleEventMessageFilterHook, NULL,
                         GetCurrentThreadId());
  }

  bool old_task_reentrancy_state =
      MessageLoop::current()->NestableTasksAllowed();

  current_plugin_instance_ = this;

  handle_event_depth_++;

  bool pop_user_gesture = false;

  if (IsUserGestureMessage(event->event)) {
    pop_user_gesture = true;
    instance()->PushPopupsEnabledState(true);
  }

  bool ret = instance()->NPP_HandleEvent(event) != 0;

  if (event->event == WM_MOUSEMOVE) {
    // Snag a reference to the current cursor ASAP in case the plugin modified
    // it. There is a nasty race condition here with the multiprocess browser
    // as someone might be setting the cursor in the main process as well.
    *cursor = current_windowless_cursor_;
  }

  if (pop_user_gesture) {
    instance()->PopPopupsEnabledState();
  }

  handle_event_depth_--;

  current_plugin_instance_ = NULL;

  MessageLoop::current()->SetNestableTasksAllowed(old_task_reentrancy_state);

  if (handle_event_message_filter_hook_) {
    UnhookWindowsHookEx(handle_event_message_filter_hook_);
    handle_event_message_filter_hook_ = NULL;
  }

  // We could have multiple NPP_HandleEvent calls nested together in case
  // the plugin enters a modal loop. Reset the pump messages event when
  // the outermost NPP_HandleEvent call unwinds.
  if (handle_event_depth_ == 0) {
    ResetEvent(handle_event_pump_messages_event_);
  }

  if (event->event == WM_RBUTTONUP && ::IsWindow(prev_focus_window)) {
    ::SetFocus(prev_focus_window);
  }

  return ret;
#endif
  return 0;
}

WebPluginResourceClient* WebPluginDelegateImpl::CreateResourceClient(
    int resource_id, const std::string &url, bool notify_needed,
    void *notify_data, void* existing_stream) {
  // Stream already exists. This typically happens for range requests
  // initiated via NPN_RequestRead.
  if (existing_stream) {
    NPAPI::PluginStream* plugin_stream =
        reinterpret_cast<NPAPI::PluginStream*>(existing_stream);

    plugin_stream->CancelRequest();

    return plugin_stream->AsResourceClient();
  }

  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
  std::string mime_type;
  NPAPI::PluginStreamUrl *stream = instance()->CreateStream(resource_id,
                                                            url,
                                                            mime_type,
                                                            notify_needed,
                                                            notify_data);
  return stream;
}

void WebPluginDelegateImpl::URLRequestRouted(const std::string&url,
                                             bool notify_needed,
                                             void* notify_data) {
  if (notify_needed) {
    instance()->SetURLLoadData(GURL(url.c_str()), notify_data);
  }
}


