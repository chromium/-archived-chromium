// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_instance.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread_local.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/plugins/plugin_stream_url.h"
#include "webkit/glue/plugins/plugin_string_stream.h"
#if defined(OS_WIN)
#include "webkit/glue/plugins/mozilla_extensions.h"
#endif
#include "net/base/escape.h"

namespace NPAPI {

// Use TLS to store the PluginInstance object during its creation.  We need to
// pass this instance to the service manager (MozillaExtensionApi) created as a
// result of NPN_GetValue in the context of NP_Initialize.
static base::LazyInstance<base::ThreadLocalPointer<PluginInstance> > lazy_tls(
    base::LINKER_INITIALIZED);

PluginInstance::PluginInstance(PluginLib *plugin, const std::string &mime_type)
    : plugin_(plugin),
      npp_(0),
      host_(PluginHost::Singleton()),
      npp_functions_(plugin->functions()),
#if defined(OS_WIN)
      hwnd_(0),
#endif
      windowless_(false),
      transparent_(true),
      webplugin_(0),
      mime_type_(mime_type),
      get_notify_data_(NULL),
      use_mozilla_user_agent_(false),
      message_loop_(MessageLoop::current()),
      load_manually_(false),
      in_close_streams_(false) {
  npp_ = new NPP_t();
  npp_->ndata = 0;
  npp_->pdata = 0;

  memset(&zero_padding_, 0, sizeof(zero_padding_));
  DCHECK(message_loop_);
}

PluginInstance::~PluginInstance() {
  CloseStreams();

  if (npp_ != 0) {
    delete npp_;
    npp_ = 0;
  }

  if (plugin_)
    plugin_->CloseInstance();
}

PluginStreamUrl *PluginInstance::CreateStream(int resource_id,
                                              const std::string &url,
                                              const std::string &mime_type,
                                              bool notify_needed,
                                              void *notify_data) {
  PluginStreamUrl *stream = new PluginStreamUrl(
      resource_id, GURL(url), this, notify_needed, notify_data);

  AddStream(stream);
  return stream;
}

void PluginInstance::AddStream(PluginStream* stream) {
  open_streams_.push_back(stream);
}

void PluginInstance::RemoveStream(PluginStream* stream) {
  if (in_close_streams_)
    return;

  std::vector<scoped_refptr<PluginStream> >::iterator stream_index;
  for (stream_index = open_streams_.begin();
       stream_index != open_streams_.end(); ++stream_index) {
    if (*stream_index == stream) {
      open_streams_.erase(stream_index);
      break;
    }
  }
}

bool PluginInstance::IsValidStream(const NPStream* stream) {
  std::vector<scoped_refptr<PluginStream> >::iterator stream_index;
  for (stream_index = open_streams_.begin();
          stream_index != open_streams_.end(); ++stream_index) {
    if ((*stream_index)->stream() == stream)
      return true;
  }

  return false;
}

void PluginInstance::CloseStreams() {
  in_close_streams_ = true;
  for (unsigned int index = 0; index < open_streams_.size(); ++index) {
    // Close all streams on the way down.
    open_streams_[index]->Close(NPRES_USER_BREAK);
  }
  open_streams_.clear();
  in_close_streams_ = false;
}

#if defined(OS_WIN)
bool PluginInstance::HandleEvent(UINT message, WPARAM wParam, LPARAM lParam) {
  if (!windowless_)
    return false;

  NPEvent windowEvent;
  windowEvent.event = message;
  windowEvent.lParam = static_cast<uint32>(lParam);
  windowEvent.wParam = static_cast<uint32>(wParam);
  return NPP_HandleEvent(&windowEvent) != 0;
}
#endif

bool PluginInstance::Start(const GURL& url,
                           char** const param_names,
                           char** const param_values,
                           int param_count,
                           bool load_manually) {
  load_manually_ = load_manually;
  instance_url_ = url;
  unsigned short mode = load_manually_ ? NP_FULL : NP_EMBED;
  npp_->ndata = this;

  NPError err = NPP_New(mode, param_count,
      const_cast<char **>(param_names), const_cast<char **>(param_values));
  return err == NPERR_NO_ERROR;
}

NPObject *PluginInstance::GetPluginScriptableObject() {
  NPObject *value = NULL;
  NPError error = NPP_GetValue(NPPVpluginScriptableNPObject, &value);
  if (error != NPERR_NO_ERROR || value == NULL)
    return NULL;
  return value;
}

void PluginInstance::SetURLLoadData(const GURL& url,
                                    void* notify_data) {
  get_url_ = url;
  get_notify_data_ = notify_data;
}

// WebPluginLoadDelegate methods
void PluginInstance::DidFinishLoadWithReason(NPReason reason) {
  if (!get_url_.is_empty()) {
    NPP_URLNotify(get_url_.spec().c_str(), reason, get_notify_data_);
  }

  get_url_ = GURL();
  get_notify_data_ = NULL;
}

// NPAPI methods
NPError PluginInstance::NPP_New(unsigned short mode,
                                short argc,
                                char *argn[],
                                char *argv[]) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->newp != 0);
  DCHECK(argc >= 0);

  if (npp_functions_->newp != 0) {
    return npp_functions_->newp(
        (NPMIMEType)mime_type_.c_str(), npp_, mode,  argc, argn, argv, NULL);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

void PluginInstance::NPP_Destroy() {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->newp != 0);

  if (npp_functions_->destroy != 0) {
    NPSavedData *savedData = 0;
    npp_functions_->destroy(npp_, &savedData);

    // TODO: Support savedData.  Technically, these need to be
    //       saved on a per-URL basis, and then only passed
    //       to new instances of the plugin at the same URL.
    //       Sounds like a huge security risk.  When we do support
    //       these, we should pass them back to the PluginLib
    //       to be stored there.
    DCHECK(savedData == 0);
  }

#if defined(OS_WIN)
  // Clean up back references to this instance if any
  if (mozilla_extenstions_) {
    mozilla_extenstions_->DetachFromInstance();
    mozilla_extenstions_ = NULL;
  }
#endif

  for (unsigned int file_index = 0; file_index < files_created_.size();
       file_index++) {
    file_util::Delete(files_created_[file_index], false);
  }
}

NPError PluginInstance::NPP_SetWindow(NPWindow *window) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->setwindow != 0);

  if (npp_functions_->setwindow != 0) {
    return npp_functions_->setwindow(npp_, window);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

NPError PluginInstance::NPP_NewStream(NPMIMEType type,
                                      NPStream *stream,
                                      NPBool seekable,
                                      unsigned short *stype) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->newstream != 0);
  if (npp_functions_->newstream != 0) {
      return npp_functions_->newstream(npp_, type, stream, seekable, stype);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

NPError PluginInstance::NPP_DestroyStream(NPStream *stream, NPReason reason) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->destroystream != 0);

  if (stream == NULL || (stream->ndata == NULL) ||
      !IsValidStream(stream))
    return NPERR_INVALID_INSTANCE_ERROR;

  if (npp_functions_->destroystream != 0) {
    NPError result = npp_functions_->destroystream(npp_, stream, reason);
    stream->ndata = NULL;
    return result;
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

int PluginInstance::NPP_WriteReady(NPStream *stream) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->writeready != 0);
  if (npp_functions_->writeready != 0) {
    return npp_functions_->writeready(npp_, stream);
  }
  return NULL;
}

int PluginInstance::NPP_Write(NPStream *stream,
                              int offset,
                              int len,
                              void *buffer) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->write != 0);
  if (npp_functions_->write != 0) {
    return npp_functions_->write(npp_, stream, offset, len, buffer);
  }
  return NULL;
}

void PluginInstance::NPP_StreamAsFile(NPStream *stream, const char *fname) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->asfile != 0);
  if (npp_functions_->asfile != 0) {
    npp_functions_->asfile(npp_, stream, fname);
  }

  // Creating a temporary FilePath instance on the stack as the explicit
  // FilePath constructor with StringType as an argument causes a compiler
  // error when invoked via vector push back.
  FilePath file_name = FilePath::FromWStringHack(UTF8ToWide(fname));
  files_created_.push_back(file_name);
}

void PluginInstance::NPP_URLNotify(const char *url,
                                   NPReason reason,
                                   void *notifyData) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->urlnotify != 0);
  if (npp_functions_->urlnotify != 0) {
    npp_functions_->urlnotify(npp_, url, reason, notifyData);
  }
}

NPError PluginInstance::NPP_GetValue(NPPVariable variable, void *value) {
  DCHECK(npp_functions_ != 0);
  // getvalue is NULL for Shockwave
  if (npp_functions_->getvalue != 0) {
    return npp_functions_->getvalue(npp_, variable, value);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

NPError PluginInstance::NPP_SetValue(NPNVariable variable, void *value) {
  DCHECK(npp_functions_ != 0);
  if (npp_functions_->setvalue != 0) {
    return npp_functions_->setvalue(npp_, variable, value);
  }
  return NPERR_INVALID_FUNCTABLE_ERROR;
}

short PluginInstance::NPP_HandleEvent(NPEvent *event) {
  DCHECK(npp_functions_ != 0);
  DCHECK(npp_functions_->event != 0);
  if (npp_functions_->event != 0) {
    return npp_functions_->event(npp_, (void*)event);
  }
  return false;
}

bool PluginInstance::NPP_Print(NPPrint* platform_print) {
  DCHECK(npp_functions_ != 0);
  if (npp_functions_->print != 0) {
    npp_functions_->print(npp_, platform_print);
    return true;
  }
  return false;
}

void PluginInstance::SendJavaScriptStream(const std::string& url,
                                          const std::wstring& result,
                                          bool success,
                                          bool notify_needed,
                                          int notify_data) {
  if (success) {
    PluginStringStream *stream =
      new PluginStringStream(this, url, notify_needed,
                             reinterpret_cast<void*>(notify_data));
    AddStream(stream);
    stream->SendToPlugin(WideToUTF8(result), "text/html");
  } else {
    // NOTE: Sending an empty stream here will crash MacroMedia
    // Flash 9.  Just send the URL Notify.
    if (notify_needed) {
      this->NPP_URLNotify(url.c_str(), NPRES_DONE,
                          reinterpret_cast<void*>(notify_data));
    }
  }
}

void PluginInstance::DidReceiveManualResponse(const std::string& url,
                                              const std::string& mime_type,
                                              const std::string& headers,
                                              uint32 expected_length,
                                              uint32 last_modified) {
  DCHECK(load_manually_);
  std::string response_url = url;
  if (response_url.empty()) {
    response_url = instance_url_.spec();
  }

  bool cancel = false;

  plugin_data_stream_ = CreateStream(-1, url, mime_type, false, NULL);

  plugin_data_stream_->DidReceiveResponse(mime_type, headers, expected_length,
                                          last_modified, true, &cancel);
}

void PluginInstance::DidReceiveManualData(const char* buffer, int length) {
  DCHECK(load_manually_);
  if (plugin_data_stream_.get() != NULL) {
    plugin_data_stream_->DidReceiveData(buffer, length, 0);
  }
}

void PluginInstance::DidFinishManualLoading() {
  DCHECK(load_manually_);
  if (plugin_data_stream_.get() != NULL) {
    plugin_data_stream_->DidFinishLoading();
    plugin_data_stream_->Close(NPRES_DONE);
    plugin_data_stream_ = NULL;
  }
}

void PluginInstance::DidManualLoadFail() {
  DCHECK(load_manually_);
  if (plugin_data_stream_.get() != NULL) {
    plugin_data_stream_->DidFail();
    plugin_data_stream_ = NULL;
  }
}

void PluginInstance::PluginThreadAsyncCall(void (*func)(void *),
                                           void *userData) {
  message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &PluginInstance::OnPluginThreadAsyncCall, func, userData));
}

void PluginInstance::OnPluginThreadAsyncCall(void (*func)(void *),
                                             void *userData) {
#if defined(OS_WIN)
    // We are invoking an arbitrary callback provided by a third
  // party plugin. It's better to wrap this into an exception
  // block to protect us from crashes.
  __try {
    func(userData);
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // Maybe we can disable a crashing plugin.
    // But for now, just continue.
  }
#else
  NOTIMPLEMENTED();
#endif
}

PluginInstance* PluginInstance::SetInitializingInstance(
    PluginInstance* instance) {
  PluginInstance* old_instance = lazy_tls.Pointer()->Get();
  lazy_tls.Pointer()->Set(instance);
  return old_instance;
}

PluginInstance* PluginInstance::GetInitializingInstance() {
  return lazy_tls.Pointer()->Get();
}

NPError PluginInstance::GetServiceManager(void** service_manager) {
#if defined(OS_WIN)
  if (!mozilla_extenstions_) {
    mozilla_extenstions_ = new MozillaExtensionApi(this);
  }

  DCHECK(mozilla_extenstions_);
  mozilla_extenstions_->QueryInterface(nsIServiceManager::GetIID(),
                                       service_manager);
#else
  NOTIMPLEMENTED();
#endif
  return NPERR_NO_ERROR;
}

void PluginInstance::PushPopupsEnabledState(bool enabled) {
  popups_enabled_stack_.push(enabled);
}

void PluginInstance::PopPopupsEnabledState() {
  popups_enabled_stack_.pop();
}

void PluginInstance::RequestRead(NPStream* stream, NPByteRange* range_list) {
  std::string range_info = "bytes=";

  while (range_list) {
    range_info += IntToString(range_list->offset);
    range_info += "-";
    range_info += IntToString(range_list->offset + range_list->length - 1);
    range_list = range_list->next;
    if (range_list) {
      range_info += ",";
    }
  }

  if (plugin_data_stream_) {
    if (plugin_data_stream_->stream() == stream) {
      webplugin_->CancelDocumentLoad();
      plugin_data_stream_ = NULL;
    }
  }

  // The lifetime of a NPStream instance depends on the PluginStream instance
  // which owns it. When a plugin invokes NPN_RequestRead on a seekable stream,
  // we don't want to create a new stream when the corresponding response is
  // received. We send over a cookie which represents the PluginStream
  // instance which is sent back from the renderer when the response is
  // received.
  std::vector<scoped_refptr<PluginStream> >::iterator stream_index;
  for (stream_index = open_streams_.begin();
          stream_index != open_streams_.end(); ++stream_index) {
    PluginStream* plugin_stream = *stream_index;
    if (plugin_stream->stream() == stream) {
      // A stream becomes seekable the first time NPN_RequestRead
      // is called on it.
      plugin_stream->set_seekable(true);

      webplugin_->InitiateHTTPRangeRequest(
          stream->url, range_info.c_str(),
          plugin_stream,
          plugin_stream->notify_needed(),
          plugin_stream->notify_data());
      break;
    }
  }
}

}  // namespace NPAPI
