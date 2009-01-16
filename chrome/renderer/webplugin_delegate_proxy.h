// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__
#define CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__

#include <set>
#include <string>

#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webplugin_delegate.h"

class GURL;
struct PluginHostMsg_RouteToFrame_Params;
class RenderView;
class SkBitmap;

namespace base {
class WaitableEvent;
}

namespace skia {
class PlatformCanvasWin;
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

  // Called to flush any deferred geometry changes to the plugin process.
  virtual void FlushGeometryUpdates();

  // WebPluginDelegate implementation:
  virtual void PluginDestroyed();
  virtual bool Initialize(const GURL& url, char** argn, char** argv, int argc,
                          WebPlugin* plugin, bool load_manually);
  virtual void UpdateGeometry(const gfx::Rect& window_rect,
                              const gfx::Rect& clip_rect);
  virtual void Paint(HDC hdc, const gfx::Rect& rect);
  virtual void Print(HDC hdc);
  virtual NPObject* GetPluginScriptableObject();
  virtual void DidFinishLoadWithReason(NPReason reason);
  virtual void SetFocus();
  virtual bool HandleEvent(NPEvent* event, WebCursor* cursor);
  virtual int GetProcessId();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  virtual void SendJavaScriptStream(const std::string& url,
                                    const std::wstring& result,
                                    bool success, bool notify_needed,
                                    int notify_data);

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
                                                        void* notify_data,
                                                        void* existing_stream);

  // Notifies the delegate about a Get/Post URL request getting routed
  virtual void URLRequestRouted(const std::string&url, bool notify_needed,
                                void* notify_data);

 protected:
  template<class WebPluginDelegateProxy> friend class DeleteTask;
  ~WebPluginDelegateProxy();

 private:
  WebPluginDelegateProxy(const std::string& mime_type,
                         const std::string& clsid,
                         RenderView* render_view);

  // Message handlers for messages that proxy WebPlugin methods, which
  // we translate into calls to the real WebPlugin.
  void OnSetWindow(HWND window, HANDLE modal_loop_pump_messages_event);
  void OnCompleteURL(const std::string& url_in, std::string* url_out,
                     bool* result);
  void OnHandleURLRequest(const PluginHostMsg_URLRequest_Params& params);
  void OnCancelResource(int id);
  void OnInvalidateRect(const gfx::Rect& rect);
  void OnGetWindowScriptNPObject(int route_id, bool* success, void** npobject_ptr);
  void OnGetPluginElement(int route_id, bool* success, void** npobject_ptr);
  void OnSetCookie(const GURL& url,
                   const GURL& policy_url,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url, const GURL& policy_url,
                    std::string* cookies);
  void OnShowModalHTMLDialog(const GURL& url, int width, int height,
                             const std::string& json_arguments,
                             std::string* json_retval);
  void OnMissingPluginStatus(int status);
  void OnGetCPBrowsingContext(uint32* context);
  void OnCancelDocumentLoad();
  void OnInitiateHTTPRangeRequest(const std::string& url,
                                  const std::string& range_info,
                                  HANDLE existing_stream, bool notify_needed,
                                  HANDLE notify_data);

  // Draw a graphic indicating a crashed plugin.
  void PaintSadPlugin(HDC hdc, const gfx::Rect& rect);

  // Returns true if the given rectangle is different in the hdc and the
  // current background bitmap.
  bool BackgroundChanged(HDC hdc, const gfx::Rect& rect);

  // Copies the given rectangle from the transport bitmap to the backing store.
  void CopyFromTransportToBacking(const gfx::Rect& rect);

  // Clears the shared memory section and canvases used for windowless plugins.
  void ResetWindowlessBitmaps();

  // Creates a shared memory section and canvas.
  bool CreateBitmap(scoped_ptr<base::SharedMemory>* memory,
                    scoped_ptr<skia::PlatformCanvasWin>* canvas);

  RenderView* render_view_;
  WebPlugin* plugin_;
  bool windowless_;
  scoped_refptr<PluginChannelHost> channel_host_;
  std::string mime_type_;
  std::string clsid_;
  int instance_id_;
  FilePath plugin_path_;

  gfx::Rect plugin_rect_;
  gfx::Rect deferred_clip_rect_;
  bool send_deferred_update_geometry_;

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
  scoped_ptr<skia::PlatformCanvasWin> backing_store_canvas_;
  scoped_ptr<base::SharedMemory> transport_store_;
  scoped_ptr<skia::PlatformCanvasWin> transport_store_canvas_;
  scoped_ptr<base::SharedMemory> background_store_;
  scoped_ptr<skia::PlatformCanvasWin> background_store_canvas_;
  // This lets us know which portion of the backing store has been painted into.
  gfx::Rect backing_store_painted_;

  DISALLOW_EVIL_CONSTRUCTORS(WebPluginDelegateProxy);
};

#endif  // #ifndef CHROME_RENDERER_WEBPLUGIN_DELEGATE_PROXY_H__

