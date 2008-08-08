// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H__
#define CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H__

#include <queue>

#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/common/ipc_channel.h"
#include "chrome/common/plugin_messages.h"
#include "third_party/npapi/bindings/npapi.h"

class GURL;
class PluginChannel;
class WebPluginProxy;
class WebPluginDelegateImpl;
struct PluginMsg_Init_Params;
struct PluginMsg_Paint_Params;
struct PluginMsg_DidReceiveResponseParams;
class WebCursor;

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
  void OnDidReceiveData(int id, const std::vector<char>& buffer);
  void OnDidFinishLoading(int id);
  void OnDidFail(int id);

  void OnDidFinishLoadWithReason(int reason);
  void OnSetFocus();
  void OnHandleEvent(const NPEvent& event, bool* handled,
                     WebCursor* cursor);

  void OnPaint(const PluginMsg_Paint_Params& params);

  void OnPrint(PluginMsg_PrintResponse_Params* params);

  // Paints the plugin into a buffer. It roughly does the same as OnPaint (i.e.
  // painting a plugin) except that the plugin window is always renderered into
  // an EMF buffer and that it is effective for windowed plugins too.
  void OnPaintIntoSharedMemory(const PluginMsg_Paint_Params& params,
                               SharedMemoryHandle* emf_buffer, size_t* bytes);
  // Paints a windowed plugin into a device context.
  void WindowedPaint(HDC hdc, const gfx::Rect& window_rect);
  // Paints a windowless plugin into a device context.
  void WindowlessPaint(HDC hdc,
                       const PluginMsg_Paint_Params& params);
  void OnUpdateGeometry(const gfx::Rect& window_rect,
                        const gfx::Rect& clip_rect, bool visible);
  void OnGetPluginScriptableObject(int* route_id, void** npobject_ptr);
  void OnSendJavaScriptStream(const std::string& url,
                              const std::wstring& result,
                              bool success, bool notify_needed,
                              int notify_data);

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
                          HANDLE notify_data);

  void CreateSharedBuffer(size_t size,
                          SharedMemory* shared_buf,
                          SharedMemoryHandle* remote_handle);

  int instance_id_;
  std::string mime_type_;

  scoped_refptr<PluginChannel> channel_;

  WebPluginDelegateImpl* delegate_;
  WebPluginProxy* webplugin_;

  DISALLOW_EVIL_CONSTRUCTORS(WebPluginDelegateStub);
};

#endif  // CHROME_PLUGIN_WEBPLUGIN_DELEGATE_STUB_H__
