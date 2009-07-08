// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__
#define CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_message.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "googleurl/src/gurl.h"
#include "skia/ext/platform_canvas.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webplugin_delegate.h"

struct NPObject;
class NPObjectStub;
struct NPVariant_Param;
struct PluginHostMsg_URLRequest_Params;
class RenderView;
class SkBitmap;

namespace base {
class SharedMemory;
class WaitableEvent;
}

// An implementation of WebPluginDelegate that proxies all calls to
// the plugin process.
class WebPluginDelegateProxy : public WebPluginDelegate,
                               public IPC::Channel::Listener,
                               public IPC::Message::Sender  {
 public:
  static WebPluginDelegateProxy* Create(const GURL& url,
                                        const std::string& mime_type,
                                        const std::string& clsid,
                                        RenderView* render_view);

  // Called to drop our back-pointer to the containing RenderView.
  void DropRenderView() { render_view_ = NULL; }

  // Called to drop our pointer to the window script object.
  void DropWindowScriptObject() { window_script_object_ = NULL; }

  // WebPluginDelegate implementation:
  virtual void PluginDestroyed();
  virtual bool Initialize(const GURL& url, char** argn, char** argv, int argc,
                          WebPlugin* plugin, bool load_manually);
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(gfx::NativeDrawingContext context, const gfx::Rect& rect);
  virtual void Print(gfx::NativeDrawingContext context);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(NPReason reason);
  virtual void SetFocus();
  virtual bool HandleInputEvent(const WebKit::WebInputEvent& event,
                                WebCursor* cursor);
  virtual int GetProcessId();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  virtual void SendJavaScriptStream(const std::string& url,
                                    const std::wstring& result,
                                    bool success, bool notify_needed,
                                    intptr_t notify_data);

  virtual void DidReceiveManualResponse(const std::string& url,
                                        const std::string& mime_type,
                                        const std::string& headers,
                                        uint32 expected_length,
                                        uint32 last_modified);
  virtual void DidReceiveManualData(const char* buffer, int length);
  virtual void DidFinishManualLoading();
  virtual void DidManualLoadFail();
  virtual FilePath GetPluginPath();
  virtual void InstallMissingPlugin();
  virtual WebPluginResourceClient* CreateResourceClient(int resource_id,
                                                        const std::string &url,
                                                        bool notify_needed,
                                                        intptr_t notify_data,
                                                        intptr_t existing_stream);

  // Notifies the delegate about a Get/Post URL request getting routed
  virtual void URLRequestRouted(const std::string&url, bool notify_needed,
                                intptr_t notify_data);

 protected:
  template<class WebPluginDelegateProxy> friend class DeleteTask;
  ~WebPluginDelegateProxy();

 private:
  WebPluginDelegateProxy(const std::string& mime_type,
                         const std::string& clsid,
                         RenderView* render_view);

  // Message handlers for messages that proxy WebPlugin methods, which
  // we translate into calls to the real WebPlugin.
  void OnSetWindow(gfx::PluginWindowHandle window);
#if defined(OS_LINUX)
  void OnCreatePluginContainer(gfx::PluginWindowHandle* container);
  void OnDestroyPluginContainer(gfx::PluginWindowHandle container);
#endif
#if defined(OS_WIN)
  void OnSetWindowlessPumpEvent(HANDLE modal_loop_pump_messages_event);
#endif
  void OnCompleteURL(const std::string& url_in, std::string* url_out,
                     bool* result);
  void OnHandleURLRequest(const PluginHostMsg_URLRequest_Params& params);
  void OnCancelResource(int id);
  void OnInvalidateRect(const gfx::Rect& rect);
  void OnGetWindowScriptNPObject(int route_id, bool* success,
                                 intptr_t* npobject_ptr);
  void OnGetPluginElement(int route_id, bool* success, intptr_t* npobject_ptr);
  void OnSetCookie(const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url, const GURL& first_party_for_cookies,
                    std::string* cookies);
  void OnShowModalHTMLDialog(const GURL& url, int width, int height,
                             const std::string& json_arguments,
                             std::string* json_retval);
  void OnGetDragData(const NPVariant_Param& event, bool add_data,
                     std::vector<NPVariant_Param>* values, bool* success);
  void OnSetDropEffect(const NPVariant_Param& event, int effect,
                       bool* success);
  void OnMissingPluginStatus(int status);
  void OnGetCPBrowsingContext(uint32* context);
  void OnCancelDocumentLoad();
  void OnInitiateHTTPRangeRequest(const std::string& url,
                                  const std::string& range_info,
                                  intptr_t existing_stream,
                                  bool notify_needed,
                                  intptr_t notify_data);

  // Draw a graphic indicating a crashed plugin.
  void PaintSadPlugin(gfx::NativeDrawingContext context, const gfx::Rect& rect);

#if defined(OS_WIN)
  // Returns true if the given rectangle is different in the hdc and the
  // current background bitmap.
  bool BackgroundChanged(HDC hdc, const gfx::Rect& rect);
#else
  // TODO(port): this should be portable; just avoiding windowless plugins for
  // now.
#endif

  // Copies the given rectangle from the transport bitmap to the backing store.
  void CopyFromTransportToBacking(const gfx::Rect& rect);

  // Clears the shared memory section and canvases used for windowless plugins.
  void ResetWindowlessBitmaps();

  // Creates a shared memory section and canvas.
  bool CreateBitmap(scoped_ptr<base::SharedMemory>* memory,
                    scoped_ptr<skia::PlatformCanvas>* canvas);

  RenderView* render_view_;
  WebPlugin* plugin_;
  bool windowless_;
  scoped_refptr<PluginChannelHost> channel_host_;
  std::string mime_type_;
  std::string clsid_;
  int instance_id_;
  FilePath plugin_path_;

  gfx::Rect plugin_rect_;

  NPObject* npobject_;
  NPObjectStub* window_script_object_;

  // Event passed in by the plugin process and is used to decide if
  // messages need to be pumped in the NPP_HandleEvent sync call.
  scoped_ptr<base::WaitableEvent> modal_loop_pump_messages_event_;

  // Bitmap for crashed plugin
  SkBitmap* sad_plugin_;

  // True if we got an invalidate from the plugin and are waiting for a paint.
  bool invalidate_pending_;

  // Used to desynchronize windowless painting.  When WebKit paints, we bitblt
  // from our backing store of what the plugin rectangle looks like.  The
  // plugin paints into the transport store, and we copy that to our backing
  // store when we get an invalidate from it.  The background bitmap is used
  // for transparent plugins, as they need the backgroud data during painting.
  bool transparent_;
  scoped_ptr<base::SharedMemory> backing_store_;
  scoped_ptr<skia::PlatformCanvas> backing_store_canvas_;
  scoped_ptr<base::SharedMemory> transport_store_;
  scoped_ptr<skia::PlatformCanvas> transport_store_canvas_;
  scoped_ptr<base::SharedMemory> background_store_;
  scoped_ptr<skia::PlatformCanvas> background_store_canvas_;
  // This lets us know which portion of the backing store has been painted into.
  gfx::Rect backing_store_painted_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;

  DISALLOW_EVIL_CONSTRUCTORS(WebPluginDelegateProxy);
};

#endif  // CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H_
