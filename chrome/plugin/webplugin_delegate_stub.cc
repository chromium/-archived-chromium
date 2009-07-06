// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/webplugin_delegate_stub.h"

#include "build/build_config.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/npobject_stub.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/plugin/plugin_thread.h"
#include "chrome/plugin/webplugin_proxy.h"
#include "printing/native_metafile.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "skia/ext/platform_device.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin_delegate.h"

class FinishDestructionTask : public Task {
 public:
  FinishDestructionTask(WebPluginDelegate* delegate, WebPlugin* webplugin)
    : delegate_(delegate), webplugin_(webplugin) { }

  void Run() {
    // WebPlugin must outlive WebPluginDelegate.
    if (delegate_)
      delegate_->PluginDestroyed();

    delete webplugin_;
  }

 private:
  WebPluginDelegate* delegate_;
  WebPlugin* webplugin_;
};

WebPluginDelegateStub::WebPluginDelegateStub(
    const std::string& mime_type, int instance_id, PluginChannel* channel) :
    mime_type_(mime_type),
    instance_id_(instance_id),
    channel_(channel),
    delegate_(NULL),
    webplugin_(NULL) {
  DCHECK(channel);
}

WebPluginDelegateStub::~WebPluginDelegateStub() {
  if (channel_->in_send()) {
    // The delegate or an npobject is in the callstack, so don't delete it
    // right away.
    MessageLoop::current()->PostNonNestableTask(FROM_HERE,
        new FinishDestructionTask(delegate_, webplugin_));
  } else {
    // Safe to delete right away.
    if (delegate_)
      delegate_->PluginDestroyed();

    delete webplugin_;
  }
}

void WebPluginDelegateStub::OnMessageReceived(const IPC::Message& msg) {
  // A plugin can execute a script to delete itself in any of its NPP methods.
  // Hold an extra reference to ourself so that if this does occur and we're
  // handling a sync message, we don't crash when attempting to send a reply.
  AddRef();

  IPC_BEGIN_MESSAGE_MAP(WebPluginDelegateStub, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_Init, OnInit)
    IPC_MESSAGE_HANDLER(PluginMsg_WillSendRequest, OnWillSendRequest)
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveResponse, OnDidReceiveResponse)
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveData, OnDidReceiveData)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFinishLoading, OnDidFinishLoading)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFail, OnDidFail)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFinishLoadWithReason,
                        OnDidFinishLoadWithReason)
    IPC_MESSAGE_HANDLER(PluginMsg_SetFocus, OnSetFocus)
    IPC_MESSAGE_HANDLER(PluginMsg_HandleInputEvent, OnHandleInputEvent)
    IPC_MESSAGE_HANDLER(PluginMsg_Paint, OnPaint)
    IPC_MESSAGE_HANDLER(PluginMsg_DidPaint, OnDidPaint)
    IPC_MESSAGE_HANDLER(PluginMsg_Print, OnPrint)
    IPC_MESSAGE_HANDLER(PluginMsg_GetPluginScriptableObject,
                        OnGetPluginScriptableObject)
    IPC_MESSAGE_HANDLER(PluginMsg_UpdateGeometry, OnUpdateGeometry)
    IPC_MESSAGE_HANDLER(PluginMsg_SendJavaScriptStream,
                        OnSendJavaScriptStream)
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveManualResponse,
                        OnDidReceiveManualResponse)
    IPC_MESSAGE_HANDLER(PluginMsg_DidReceiveManualData, OnDidReceiveManualData)
    IPC_MESSAGE_HANDLER(PluginMsg_DidFinishManualLoading,
                        OnDidFinishManualLoading)
    IPC_MESSAGE_HANDLER(PluginMsg_DidManualLoadFail, OnDidManualLoadFail)
    IPC_MESSAGE_HANDLER(PluginMsg_InstallMissingPlugin, OnInstallMissingPlugin)
    IPC_MESSAGE_HANDLER(PluginMsg_HandleURLRequestReply,
                        OnHandleURLRequestReply)
    IPC_MESSAGE_HANDLER(PluginMsg_URLRequestRouted, OnURLRequestRouted)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  Release();
}

bool WebPluginDelegateStub::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void WebPluginDelegateStub::OnInit(const PluginMsg_Init_Params& params,
                                   bool* result) {
  *result = false;
  int argc = static_cast<int>(params.arg_names.size());
  if (argc != static_cast<int>(params.arg_values.size())) {
    NOTREACHED();
    return;
  }

  char **argn = new char*[argc];
  char **argv = new char*[argc];
  for (int i = 0; i < argc; ++i) {
    argn[i] = const_cast<char*>(params.arg_names[i].c_str());
    argv[i] = const_cast<char*>(params.arg_values[i].c_str());
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  FilePath path = FilePath::FromWStringHack(
      command_line.GetSwitchValue(switches::kPluginPath));

#if defined(OS_WIN)
  delegate_ = WebPluginDelegate::Create(
      path, mime_type_, gfx::NativeViewFromId(params.containing_window));
#else
  NOTIMPLEMENTED() << " need to figure out nativeview id business";
  delegate_ = WebPluginDelegate::Create(
      path, mime_type_, NULL);
#endif

  if (delegate_) {
    webplugin_ = new WebPluginProxy(channel_, instance_id_, delegate_);
#if defined(OS_WIN)
    if (!webplugin_->SetModalDialogEvent(params.modal_dialog_event))
      return;
#endif
    *result = delegate_->Initialize(
        params.url, argn, argv, argc, webplugin_, params.load_manually);
  }

  delete[] argn;
  delete[] argv;
}

void WebPluginDelegateStub::OnWillSendRequest(int id, const GURL& url) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->WillSendRequest(url);
}

void WebPluginDelegateStub::OnDidReceiveResponse(
    const PluginMsg_DidReceiveResponseParams& params, bool* cancel) {
  *cancel = false;
  WebPluginResourceClient* client = webplugin_->GetResourceClient(params.id);
  if (!client)
    return;

  client->DidReceiveResponse(params.mime_type,
                             params.headers,
                             params.expected_length,
                             params.last_modified,
                             params.request_is_seekable,
                             cancel);
}

void WebPluginDelegateStub::OnDidReceiveData(int id,
                                             const std::vector<char>& buffer,
                                             int data_offset) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->DidReceiveData(&buffer.front(), static_cast<int>(buffer.size()),
                         data_offset);
}

void WebPluginDelegateStub::OnDidFinishLoading(int id) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->DidFinishLoading();
}

void WebPluginDelegateStub::OnDidFail(int id) {
  WebPluginResourceClient* client = webplugin_->GetResourceClient(id);
  if (!client)
    return;

  client->DidFail();
}

void WebPluginDelegateStub::OnDidFinishLoadWithReason(int reason) {
  delegate_->DidFinishLoadWithReason(reason);
}

void WebPluginDelegateStub::OnSetFocus() {
  delegate_->SetFocus();
}

void WebPluginDelegateStub::OnHandleInputEvent(
    const WebKit::WebInputEvent *event,
    bool* handled,
    WebCursor* cursor) {
  *handled = delegate_->HandleInputEvent(*event, cursor);
}

void WebPluginDelegateStub::OnPaint(const gfx::Rect& damaged_rect) {
  webplugin_->Paint(damaged_rect);
}

void WebPluginDelegateStub::OnDidPaint() {
  webplugin_->DidPaint();
}

void WebPluginDelegateStub::OnPrint(base::SharedMemoryHandle* shared_memory,
                                    size_t* size) {
#if defined(OS_WIN)
  printing::NativeMetafile metafile;
  if (!metafile.CreateDc(NULL, NULL)) {
    NOTREACHED();
    return;
  }
  HDC hdc = metafile.hdc();
  skia::PlatformDevice::InitializeDC(hdc);
  delegate_->Print(hdc);
  if (!metafile.CloseDc()) {
    NOTREACHED();
    return;
  }

  *size = metafile.GetDataSize();
  DCHECK(*size);
  base::SharedMemory shared_buf;
  CreateSharedBuffer(*size, &shared_buf, shared_memory);

  // Retrieve a copy of the data.
  bool success = metafile.GetData(shared_buf.memory(), *size);
  DCHECK(success);
#else
  // TODO(port): plugin printing.
  NOTIMPLEMENTED();
#endif
}

void WebPluginDelegateStub::OnUpdateGeometry(
    const gfx::Rect& window_rect,
    const gfx::Rect& clip_rect,
    const TransportDIB::Id& windowless_buffer_id,
    const TransportDIB::Id& background_buffer_id) {
  webplugin_->UpdateGeometry(
      window_rect, clip_rect,
      windowless_buffer_id, background_buffer_id);
}

void WebPluginDelegateStub::OnGetPluginScriptableObject(int* route_id,
                                                        intptr_t* npobject_ptr) {
  NPObject* object = delegate_->GetPluginScriptableObject();
  if (!object) {
    *route_id = MSG_ROUTING_NONE;
    return;
  }

  *route_id = channel_->GenerateRouteID();
  *npobject_ptr = reinterpret_cast<intptr_t>(object);
  // The stub will delete itself when the proxy tells it that it's released, or
  // otherwise when the channel is closed.
  new NPObjectStub(
      object, channel_.get(), *route_id, webplugin_->modal_dialog_event());

  // Release ref added by GetPluginScriptableObject (our stub holds its own).
  NPN_ReleaseObject(object);
}

void WebPluginDelegateStub::OnSendJavaScriptStream(const std::string& url,
                                                   const std::wstring& result,
                                                   bool success,
                                                   bool notify_needed,
                                                   intptr_t notify_data) {
  delegate_->SendJavaScriptStream(url, result, success, notify_needed,
                                  notify_data);
}

void WebPluginDelegateStub::OnDidReceiveManualResponse(
    const std::string& url,
    const PluginMsg_DidReceiveResponseParams& params) {
  delegate_->DidReceiveManualResponse(url, params.mime_type, params.headers,
                                      params.expected_length,
                                      params.last_modified);
}

void WebPluginDelegateStub::OnDidReceiveManualData(
    const std::vector<char>& buffer) {
  delegate_->DidReceiveManualData(&buffer.front(),
                                  static_cast<int>(buffer.size()));
}

void WebPluginDelegateStub::OnDidFinishManualLoading() {
  delegate_->DidFinishManualLoading();
}

void WebPluginDelegateStub::OnDidManualLoadFail() {
  delegate_->DidManualLoadFail();
}

void WebPluginDelegateStub::OnInstallMissingPlugin() {
  delegate_->InstallMissingPlugin();
}

void WebPluginDelegateStub::CreateSharedBuffer(
    size_t size,
    base::SharedMemory* shared_buf,
    base::SharedMemoryHandle* remote_handle) {
  if (!shared_buf->Create(std::wstring(), false, false, size)) {
    NOTREACHED();
    return;
  }
  if (!shared_buf->Map(size)) {
    NOTREACHED();
    shared_buf->Close();
    return;
  }

#if defined(OS_WIN)
  BOOL result = DuplicateHandle(GetCurrentProcess(),
                                shared_buf->handle(),
                                channel_->renderer_handle(),
                                remote_handle, 0, FALSE,
                                DUPLICATE_SAME_ACCESS);
  DCHECK_NE(result, 0);

  // If the calling function's shared_buf is on the stack, its destructor will
  // close the shared memory buffer handle. This is fine since we already
  // duplicated the handle to the renderer process so it will stay "alive".
#else
  // TODO(port): this should use TransportDIB.
  NOTIMPLEMENTED();
#endif
}

void WebPluginDelegateStub::OnHandleURLRequestReply(
    const PluginMsg_URLRequestReply_Params& params) {
  WebPluginResourceClient* resource_client =
      delegate_->CreateResourceClient(params.resource_id, params.url,
                                      params.notify_needed,
                                      params.notify_data,
                                      params.stream);
  webplugin_->OnResourceCreated(params.resource_id, resource_client);
}

void WebPluginDelegateStub::OnURLRequestRouted(const std::string& url,
                                               bool notify_needed,
                                               intptr_t notify_data) {
  delegate_->URLRequestRouted(url, notify_needed, notify_data);
}
