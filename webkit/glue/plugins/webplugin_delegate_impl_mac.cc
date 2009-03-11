// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_constants_win.h"
#include "webkit/glue/plugins/plugin_instance.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/webkit_glue.h"

namespace {

const wchar_t kWebPluginDelegateProperty[] = L"WebPluginDelegateProperty";
const wchar_t kPluginNameAtomProperty[] = L"PluginNameAtom";
const wchar_t kDummyActivationWindowName[] = L"DummyWindowForActivation";
const wchar_t kPluginOrigProc[] = L"OriginalPtr";
const wchar_t kPluginFlashThrottle[] = L"FlashThrottle";

// The fastest we are willing to process WM_USER+1 events for Flash.
// Flash can easily exceed the limits of our CPU if we don't throttle it.
// The throttle has been chosen by testing various delays and compromising
// on acceptable Flash performance and reasonable CPU consumption.
//
// I'd like to make the throttle delay variable, based on the amount of
// time currently required to paint Flash plugins.  There isn't a good
// way to count the time spent in aggregate plugin painting, however, so
// this seems to work well enough.
const int kFlashWMUSERMessageThrottleDelayMs = 5;

// The current instance of the plugin which entered the modal loop.
WebPluginDelegateImpl* g_current_plugin_instance = NULL;

// base::LazyInstance<std::list<MSG> > g_throttle_queue(base::LINKER_INITIALIZED);


}  // namespace

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

bool WebPluginDelegateImpl::IsPluginDelegateWindow(gfx::NativeWindow window) {
  return false;
}

bool WebPluginDelegateImpl::GetPluginNameFromWindow(
    gfx::NativeWindow window, std::wstring *plugin_name) {
  if (NULL == plugin_name) {
    return false;
  }
  if (!IsPluginDelegateWindow(window)) {
    return false;
  }
  return false;
}

bool WebPluginDelegateImpl::IsDummyActivationWindow(gfx::NativeWindow window) {
  return false;
}

WebPluginDelegateImpl::WebPluginDelegateImpl(
    gfx::NativeView containing_view,
    NPAPI::PluginInstance *instance)
    : parent_(containing_view),
      instance_(instance),
      quirks_(0),
      plugin_(NULL),
      windowless_(false),
      windowed_handle_(NULL),
      windowed_did_set_window_(false),
      windowless_needs_set_window_(true),
      handle_event_depth_(0),
      user_gesture_message_posted_(this),
      user_gesture_msg_factory_(this) {
  memset(&window_, 0, sizeof(window_));

  const WebPluginInfo& plugin_info = instance_->plugin_lib()->plugin_info();
  std::string filename =
      StringToLowerASCII(plugin_info.path.BaseName().value());

  // plugin_module_handle_ = ::GetModuleHandle(plugin_info.path.value().c_str());
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
    // instance_->set_window_handle(parent_);
  } else {
    if (!WindowedCreatePlugin())
      return false;
  }

  // plugin->SetWindow(windowed_handle_, handle_event_pump_messages_event_);
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

    window_.window = NULL;
    instance_->NPP_SetWindow(&window_);

    instance_->NPP_Destroy();

    instance_->set_web_plugin(NULL);

    if (instance_->plugin_lib()) {
      // Unpatch if this is the last plugin instance.
    }

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

void WebPluginDelegateImpl::Paint(CGContextRef context, const gfx::Rect& rect) {
  if (windowless_)
    WindowlessPaint(context, rect);
}

void WebPluginDelegateImpl::Print(CGContextRef context) {
  // Disabling the call to NPP_Print as it causes a crash in
  // flash in some cases. In any case this does not work as expected
  // as the EMF meta file dc passed in needs to be created with the
  // the plugin window dc as its sibling dc and the window rect
  // in .01 mm units.
}

NPObject* WebPluginDelegateImpl::GetPluginScriptableObject() {
  return instance_->GetPluginScriptableObject();
}

void WebPluginDelegateImpl::DidFinishLoadWithReason(NPReason reason) {
  instance()->DidFinishLoadWithReason(reason);
}

int WebPluginDelegateImpl::GetProcessId() {
  // We are in process, so the plugin pid is this current process pid.
  return getpid();
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
  NPEvent evt;
  instance()->NPP_HandleEvent(&evt);
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

bool WebPluginDelegateImpl::WindowedCreatePlugin() {
  DCHECK(!windowed_handle_);

  // create window
  if (windowed_handle_ == 0)
    return false;

  return true;
}

void WebPluginDelegateImpl::WindowedDestroyWindow() {
  if (windowed_handle_ != NULL) {
    // destroy the window
    windowed_handle_ = 0;
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

  // Clipping is handled by WebPlugin.
  if (window_rect.size() != window_rect_.size()) {
    // resize window
  }

  window_rect_ = window_rect;
  clip_rect_ = clip_rect;

  // Ensure that the entire window gets repainted.
  // invalidate entire window

  return true;
}

void WebPluginDelegateImpl::WindowedSetWindow() {
  if (!instance_)
    return;

  if (!windowed_handle_) {
    NOTREACHED();
    return;
  }

  // instance()->set_window_handle(windowed_handle_);

  DCHECK(!instance()->windowless());

  window_.clipRect.top = clip_rect_.y();
  window_.clipRect.left = clip_rect_.x();
  window_.clipRect.bottom = clip_rect_.y() + clip_rect_.height();
  window_.clipRect.right = clip_rect_.x() + clip_rect_.width();
  window_.height = window_rect_.height();
  window_.width = window_rect_.width();
  window_.x = window_rect_.x();
  window_.y = window_rect_.y();

  cg_context_.context = NULL;
  cg_context_.window = NULL;
  window_.window = &cg_context_;
  window_.type = NPWindowTypeDrawable; // NPWindowTypeWindow;

  // Reset this flag before entering the instance in case of side-effects.
  windowed_did_set_window_ = true;

  NPError err = instance()->NPP_SetWindow(&window_);
}

void WebPluginDelegateImpl::WindowlessUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect) {
  // Only resend to the instance if the geometry has changed.
  if (window_rect == window_rect_ && clip_rect == clip_rect_)
    return;

  // We will inform the instance of this change when we call NPP_SetWindow.
  clip_rect_ = clip_rect;
  cutout_rects_.clear();

  if (window_rect_ != window_rect) {
    window_rect_ = window_rect;

    WindowlessSetWindow(true);

    NPEvent pos_changed_event;

    instance()->NPP_HandleEvent(&pos_changed_event);
  }
}

void WebPluginDelegateImpl::WindowlessPaint(gfx::NativeDrawingContext hdc,
                                            const gfx::Rect& damage_rect) {
  static StatsRate plugin_paint("Plugin.Paint");
  StatsScope<StatsRate> scope(plugin_paint);
  NPEvent paint_event;
  instance()->NPP_HandleEvent(&paint_event);
}

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

  if (!force_set_window)
    // Reset this flag before entering the instance in case of side-effects.
    windowless_needs_set_window_ = false;

  NPError err = instance()->NPP_SetWindow(&window_);
  DCHECK(err == NPERR_NO_ERROR);
}

void WebPluginDelegateImpl::SetFocus() {
  DCHECK(instance()->windowless());

  NPEvent focus_event;

  instance()->NPP_HandleEvent(&focus_event);
}

bool WebPluginDelegateImpl::HandleEvent(NPEvent* event,
                                        WebCursor* cursor) {
  DCHECK(windowless_) << "events should only be received in windowless mode";
  DCHECK(cursor != NULL);

  return true;
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
