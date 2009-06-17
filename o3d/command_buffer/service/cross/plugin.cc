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


// This file contains the command renderer service (renderer) plug-in.
// NOTE: this is only implemented on Windows currently.
// TODO: other platforms.

#include <npupp.h>
#include <build/build_config.h>
#ifdef OS_WIN
#include <windows.h>
#endif

#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/common/cross/rpc_imc.h"
#include "command_buffer/service/cross/buffer_rpc.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"
#include "command_buffer/service/cross/gapi_decoder.h"
#ifdef OS_WIN
#include "command_buffer/service/win/d3d9/gapi_d3d9.h"
#endif
#include "third_party/native_client/googleclient/native_client/src/trusted/desc/nrd_all_modules.h"
#include "third_party/nixysa/files/static_glue/npapi/npn_api.h"

namespace o3d {
namespace command_buffer {

// The Plugin class implements the plug-in instance. It derives from NPObject
// to be the scriptable object as well.
class Plugin : public NPObject {
 public:
  // Sets the window used by the plug-in.
  NPError SetWindow(NPWindow *window);

  // Gets the NPClass representing the NPAPI entrypoints to the object.
  static NPClass *GetNPClass() {
    return &class_;
  }

 private:
  explicit Plugin(NPP npp);
  ~Plugin();

  // Creates the renderer using the IMC socket. This is called from Javascript
  // using the create() function.
  void Create(nacl::HtpHandle socket);

  // Destroys the renderer. This is called from Javascript using the destroy()
  // function.
  void Destroy();

  // NPAPI bindings.
  static NPObject *Allocate(NPP npp, NPClass *npclass);
  static void Deallocate(NPObject *object);
  static bool HasMethod(NPObject *header, NPIdentifier name);
  static bool Invoke(NPObject *header, NPIdentifier name,
                     const NPVariant *args, uint32_t argCount,
                     NPVariant *result);
  static bool InvokeDefault(NPObject *header, const NPVariant *args,
                            uint32_t argCount, NPVariant *result);
  static bool HasProperty(NPObject *header, NPIdentifier name);
  static bool GetProperty(NPObject *header, NPIdentifier name,
                          NPVariant *variant);
  static bool SetProperty(NPObject *header, NPIdentifier name,
                          const NPVariant *variant);
  static bool Enumerate(NPObject *header, NPIdentifier **value,
                        uint32_t *count);

#ifdef OS_WIN
  static DWORD WINAPI ThreadMain(void *param) {
    static_cast<Plugin *>(param)->DoThread();
    return 0;
  }
#endif
  // Executes the main renderer thread.
  void DoThread();

  NPP npp_;
  static NPClass class_;
  NPIdentifier create_id_;
  NPIdentifier destroy_id_;
  NPIdentifier handle_id_;

#ifdef OS_WIN
  HWND hwnd_;
  HANDLE thread_;
  DWORD thread_id_;
#endif

  nacl::HtpHandle handle_;
  scoped_ptr<GAPIInterface> gapi_;
};


Plugin::Plugin(NPP npp)
    : npp_(npp),
#ifdef OS_WIN
      hwnd_(NULL),
      thread_(NULL),
      thread_id_(0),
#endif
      handle_(nacl::kInvalidHtpHandle) {
  const char *names[3] = {"create", "destroy", "handle"};
  NPIdentifier ids[3];
  NPN_GetStringIdentifiers(names, 3, ids);
  create_id_ = ids[0];
  destroy_id_ = ids[1];
  handle_id_ = ids[2];
}

Plugin::~Plugin() {
  if (gapi_.get()) Destroy();
}

NPError Plugin::SetWindow(NPWindow *window) {
#ifdef OS_WIN
  HWND hWnd = window ? static_cast<HWND>(window->window) : NULL;
  hwnd_ = hWnd;
  return NPERR_NO_ERROR;
#endif  // OS_WIN
}

// Creates the renderer. This spawns a thread that answers requests (the D3D
// context is created in that other thread, so that we don't need to enable
// multi-threading on it).
void Plugin::Create(nacl::HtpHandle handle) {
  if (gapi_.get()) return;
  handle_ = handle;
#ifdef OS_WIN
  if (!hwnd_) return;
  GAPID3D9 *gapi_d3d = new GAPID3D9;
  gapi_d3d->set_hwnd(hwnd_);
  gapi_.reset(gapi_d3d);
  // TODO: use chrome/base threads.
  thread_ = ::CreateThread(NULL, 0, ThreadMain, this, 0, &thread_id_);
#endif
}

// Destroys the renderer. This terminates the renderer thread, and waits until
// it is finished.
void Plugin::Destroy() {
  if (!gapi_.get()) return;
#ifdef OS_WIN
  ::PostThreadMessage(thread_id_, WM_USER, 0, 0);
  ::WaitForSingleObject(thread_, INFINITE);
  ::CloseHandle(thread_);
#endif
  gapi_.reset(NULL);
}

// Executes the main renderer thread: answers requests, executes commands.
void Plugin::DoThread() {
  scoped_ptr<GAPIDecoder> decoder(new GAPIDecoder(gapi_.get()));
  scoped_ptr<CommandBufferEngine> engine(
      new CommandBufferEngine(decoder.get()));
  decoder->set_engine(engine.get());

  IMCMessageProcessor processor(handle_, engine->rpc_impl());
  engine->set_process_interface(&processor);
  IMCSender sender(handle_);
  engine->set_client_rpc(&sender);

  gapi_->Initialize();
  while (true) {
    bool done = false;
#ifdef OS_WIN
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_USER) {
        done = true;
        break;
      }
    }
#endif
    if (done) break;
    // NOTE: DoWork will block when there is nothing to do. This can be an
    // issue at termination if the browser tries to kill the plug-in before the
    // NaCl module, because then this thread won't terminate, and it will block
    // the main (browser) thread. Workaround: kill the NaCl module (kill the
    // sel_ldr window).
    // TODO: Fix this. It needs select()/poll() or a timeout in the
    // IMC library, and that doesn't exist currently. We could use non-blocking,
    // e.g. HasWork() and sleep if there is nothing to do, but that would
    // translate into unacceptable latencies - 10ms per call.
    if (!engine->DoWork()) break;
  }
  gapi_->Destroy();
}

NPClass Plugin::class_ = {
  NP_CLASS_STRUCT_VERSION,
  Plugin::Allocate,
  Plugin::Deallocate,
  0,
  Plugin::HasMethod,
  Plugin::Invoke,
  0,
  Plugin::HasProperty,
  Plugin::GetProperty,
  Plugin::SetProperty,
  0,
  Plugin::Enumerate,
};

NPObject *Plugin::Allocate(NPP npp, NPClass *npclass) {
  return new Plugin(npp);
}

void Plugin::Deallocate(NPObject *object) {
  delete static_cast<Plugin *>(object);
}

bool Plugin::HasMethod(NPObject *header, NPIdentifier name) {
  Plugin *plugin = static_cast<Plugin *>(header);
  // 2 methods supported: create(handle) and destroy().
  return (name == plugin->create_id_ ||
          name == plugin->destroy_id_);
}

bool Plugin::Invoke(NPObject *header, NPIdentifier name,
                    const NPVariant *args, uint32_t argCount,
                    NPVariant *result) {
  Plugin *plugin = static_cast<Plugin *>(header);
  VOID_TO_NPVARIANT(*result);
  if (name == plugin->create_id_ && argCount == 1 &&
      NPVARIANT_IS_OBJECT(args[0])) {
    // create(handle) was called.
    //
    // Temporary ugly hack: the NPObject is a wrapper around a HtpHandle, but
    // to get that handle we need to get the "handle" property on it which is a
    // string that represents the address in memory of that HtpHandle.
    NPObject *object = NPVARIANT_TO_OBJECT(args[0]);

    NPVariant handle_prop;
    bool result = NPN_GetProperty(plugin->npp_, object, plugin->handle_id_,
                                  &handle_prop);
    if (!result || !NPVARIANT_IS_STRING(handle_prop))
      return false;
    String handle_string(NPVARIANT_TO_STRING(handle_prop).utf8characters,
                         NPVARIANT_TO_STRING(handle_prop).utf8length);
    intptr_t handle_value = strtol(handle_string.c_str(), NULL, 0);
    nacl::HtpHandle handle = reinterpret_cast<nacl::HtpHandle>(handle_value);
    plugin->Create(handle);
    return true;
  } else if (name == plugin->destroy_id_ && argCount == 0) {
    // destroy() was called.
    plugin->Destroy();
    return true;
  } else {
    return false;
  }
}

bool Plugin::InvokeDefault(NPObject *header, const NPVariant *args,
                           uint32_t argCount, NPVariant *result) {
  return false;
}

bool Plugin::HasProperty(NPObject *header, NPIdentifier name) {
  return false;
}

bool Plugin::GetProperty(NPObject *header, NPIdentifier name,
                         NPVariant *variant) {
  return false;
}

bool Plugin::SetProperty(NPObject *header, NPIdentifier name,
                         const NPVariant *variant) {
  return false;
}

bool Plugin::Enumerate(NPObject *header, NPIdentifier **value,
                       uint32_t *count) {
  Plugin *plugin = static_cast<Plugin *>(header);
  *count = 2;
  NPIdentifier *ids = static_cast<NPIdentifier *>(
      NPN_MemAlloc(*count * sizeof(NPIdentifier)));
  ids[0] = plugin->create_id_;
  ids[1] = plugin->destroy_id_;
  *value = ids;
  return true;
}

}  // namespace command_buffer
}  // namespace o3d

using o3d::command_buffer::Plugin;

extern "C" {
// NPAPI entry points.

char *NP_GetMIMEDescription(void) {
  return "application/vnd.cmdbuf::CommandBuffer MIME";
}

NPError OSCALL NP_Initialize(NPNetscapeFuncs *browserFuncs) {
  NPError retval = InitializeNPNApi(browserFuncs);
  if (retval != NPERR_NO_ERROR) return retval;
  return NPERR_NO_ERROR;
}

NPError OSCALL NP_GetEntryPoints(NPPluginFuncs *pluginFuncs) {
  pluginFuncs->version = 11;
  pluginFuncs->size = sizeof(pluginFuncs);
  pluginFuncs->newp = NPP_New;
  pluginFuncs->destroy = NPP_Destroy;
  pluginFuncs->setwindow = NPP_SetWindow;
  pluginFuncs->newstream = NPP_NewStream;
  pluginFuncs->destroystream = NPP_DestroyStream;
  pluginFuncs->asfile = NPP_StreamAsFile;
  pluginFuncs->writeready = NPP_WriteReady;
  pluginFuncs->write = NPP_Write;
  pluginFuncs->print = NPP_Print;
  pluginFuncs->event = NPP_HandleEvent;
  pluginFuncs->urlnotify = NPP_URLNotify;
  pluginFuncs->getvalue = NPP_GetValue;
  pluginFuncs->setvalue = NPP_SetValue;

  return NPERR_NO_ERROR;
}

NPError OSCALL NP_Shutdown(void) {
  return NPERR_NO_ERROR;
}

// Creates a plugin instance.
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                char *argn[], char *argv[], NPSavedData *saved) {
  NPObject *object = NPN_CreateObject(instance, Plugin::GetNPClass());
  if (object == NULL) {
    return NPERR_OUT_OF_MEMORY_ERROR;
  }
  instance->pdata = object;
  return NPERR_NO_ERROR;
}

// Destroys a plugin instance.
NPError NPP_Destroy(NPP instance, NPSavedData **save) {
  Plugin *obj = static_cast<Plugin*>(instance->pdata);
  if (obj) {
    obj->SetWindow(NULL);
    NPN_ReleaseObject(obj);
    instance->pdata = NULL;
  }

  return NPERR_NO_ERROR;
}

// Sets the window used by the plugin instance.
NPError NPP_SetWindow(NPP instance, NPWindow *window) {
  Plugin *obj = static_cast<Plugin*>(instance->pdata);
  obj->SetWindow(window);
  return NPERR_NO_ERROR;
}

// Gets the scriptable object for the plug-in instance.
NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  switch (variable) {
    case NPPVpluginScriptableNPObject: {
      void **v = static_cast<void **>(value);
      Plugin *obj = static_cast<Plugin*>(instance->pdata);
      NPN_RetainObject(obj);
      *v = obj;
      return NPERR_NO_ERROR;
    }
    default:
      break;
  }
  return NPERR_GENERIC_ERROR;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream *stream,
                      NPBool seekable, uint16 *stype) {
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason) {
  return NPERR_NO_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream *stream) {
  return 4096;
}

int32 NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len,
                void *buffer) {
  return len;
}

void NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname) {
}

void NPP_Print(NPP instance, NPPrint *platformPrint) {
}

int16 NPP_HandleEvent(NPP instance, void *event) {
  return 0;
}

void NPP_URLNotify(NPP instance, const char *url, NPReason reason,
                   void *notifyData) {
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  return NPERR_GENERIC_ERROR;
}
}  // extern "C"
