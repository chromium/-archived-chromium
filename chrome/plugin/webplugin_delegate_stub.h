// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_
#define CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_

#include <queue>

#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/transport_dib.h"
#include "googleurl/src/gurl.h"
#include "third_party/npapi/bindings/npapi.h"

class PluginChannel;
class WebPluginProxy;
class WebPluginDelegate;
struct PluginMsg_Init_Params;
struct PluginMsg_DidReceiveResponseParams;
struct PluginMsg_URLRequestReply_Params;
class WebCursor;

namespace WebKit {
class WebInputEvent;
}

// Converts the IPC messages from WebPluginDelegateProxy into calls to the
// actual WebPluginDelegate object.
class WebPluginDelegateStub : public IPC::Channel::Listener,
                              public IPC::Message::Sender,
                              public base::RefCounted<WebPluginDelegateStub> {
 public:
  WebPluginDelegateStub(const std::string& mime_type, int instance_id,
                        PluginChannel* channel);
  ~WebPluginDelegateStub();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  int instance_id() { return instance_id_; }

 private:
  // Message handlers for the WebPluginDelegate calls that are proxied from the
  // renderer over the IPC channel.
  void OnInit(const PluginMsg_Init_Params& params, bool* result);

  void OnWillSendRequest(int id, const GURL& url);
  void OnDidReceiveResponse(const PluginMsg_DidReceiveResponseParams& params,
                            bool* cancel);
  void OnDidReceiveData(int id, const std::vector<char>& buffer,
                        int data_offset);
  void OnDidFinishLoading(int id);
  void OnDidFail(int id);

  void OnDidFinishLoadWithReason(int reason);
  void OnSetFocus();
  void OnHandleInputEvent(const WebKit::WebInputEvent* event,
                          bool* handled, WebCursor* cursor);

  void OnPaint(const gfx::Rect& damaged_rect);
  void OnDidPaint();

  void OnPrint(base::SharedMemoryHandle* shared_memory, size_t* size);

  void OnUpdateGeometry(const gfx::Rect& window_rect,
                        const gfx::Rect& clip_rect,
                        const TransportDIB::Id& windowless_buffer,
                        const TransportDIB::Id& background_buffer);
  void OnGetPluginScriptableObject(int* route_id, intptr_t* npobject_ptr);
  void OnSendJavaScriptStream(const std::string& url,
                              const std::wstring& result,
                              bool success, bool notify_needed,
                              intptr_t notify_data);

  void OnDidReceiveManualResponse(
      const std::string& url,
      const PluginMsg_DidReceiveResponseParams& params);
  void OnDidReceiveManualData(const std::vector<char>& buffer);
  void OnDidFinishManualLoading();
  void OnDidManualLoadFail();
  void OnInstallMissingPlugin();

  void OnHandleURLRequestReply(
      const PluginMsg_URLRequestReply_Params& params);

  void OnURLRequestRouted(const std::string& url, bool notify_needed,
                          intptr_t notify_data);

  void CreateSharedBuffer(size_t size,
                          base::SharedMemory* shared_buf,
                          base::SharedMemoryHandle* remote_handle);

  std::string mime_type_;
  int instance_id_;

  scoped_refptr<PluginChannel> channel_;

  WebPluginDelegate* delegate_;
  WebPluginProxy* webplugin_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebPluginDelegateStub);
};

#endif  // CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H_
