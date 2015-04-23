// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_plugin/test_chrome_plugin.h"

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/chrome_plugin_api.h"
#include "googleurl/src/gurl.h"

static CPID g_cpid;
static CPBrowserFuncs g_cpbrowser_funcs;
static CPRequestFuncs g_cprequest_funcs;
static CPResponseFuncs g_cpresponse_funcs;
static TestFuncParams::BrowserFuncs g_cptest_funcs;

// Create a global AtExitManager so that our code can use code from base that
// uses Singletons, for example.  We don't care about static constructors here.
static base::AtExitManager global_at_exit_manager;

const TestResponsePayload* FindPayload(const char* url) {
  for (size_t i = 0; i < arraysize(kChromeTestPluginPayloads); ++i) {
    if (strcmp(kChromeTestPluginPayloads[i].url, url) == 0)
      return &kChromeTestPluginPayloads[i];
  }
  return NULL;
}

std::string GetPayloadHeaders(const TestResponsePayload* payload) {
  return StringPrintf(
    "HTTP/1.1 200 OK%c"
    "Content-type: %s%c"
    "%c", 0, payload->mime_type, 0, 0);
}

void STDCALL InvokeLaterCallback(void* data) {
  Task* task = static_cast<Task*>(data);
  task->Run();
  delete task;
}

// ResponseStream: Manages the streaming of the payload data.

class ResponseStream : public base::RefCounted<ResponseStream> {
public:
  ResponseStream(const TestResponsePayload* payload, CPRequest* request);
  ~ResponseStream() {
    request_->pdata = NULL;
  }

  void Init();
  int GetResponseInfo(CPResponseInfoType type, void* buf, uint32 buf_size);
  int ReadData(void* buf, uint32 buf_size);

private:
  // Called asynchronously via InvokeLater.
  void ResponseStarted();
  int ReadCompleted(void* buf, uint32 buf_size);

  enum ReadyStates {
    READY_INVALID = 0,
    READY_WAITING = 1,
    READY_GOT_HEADERS = 2,
    READY_GOT_DATA = 3,
  };
  const TestResponsePayload* payload_;
  uint32 offset_;
  int ready_state_;
  CPRequest* request_;
};

ResponseStream::ResponseStream(const TestResponsePayload* payload,
                               CPRequest* request)
    : payload_(payload), offset_(0), ready_state_(READY_INVALID),
    request_(request) {
}

void ResponseStream::Init() {
  if (payload_->async) {
    // simulate an asynchronous start complete
    ready_state_ = READY_WAITING;
    g_cptest_funcs.invoke_later(
        InvokeLaterCallback,
        // downcast to Task before void, since we upcast from void to Task.
        static_cast<Task*>(
            NewRunnableMethod(this, &ResponseStream::ResponseStarted)),
        500);
  } else {
    ready_state_ = READY_GOT_DATA;
  }
}

int ResponseStream::GetResponseInfo(CPResponseInfoType type, void* buf,
                                    uint32 buf_size) {
  if (ready_state_ < READY_GOT_HEADERS)
    return CPERR_FAILURE;

  switch (type) {
  case CPRESPONSEINFO_HTTP_STATUS:
    if (buf)
      memcpy(buf, &payload_->status, buf_size);
    break;
  case CPRESPONSEINFO_HTTP_RAW_HEADERS: {
    std::string headers = GetPayloadHeaders(payload_);
    if (buf_size < headers.size()+1)
      return static_cast<int>(headers.size()+1);
    if (buf)
      memcpy(buf, headers.c_str(), headers.size()+1);
    break;
    }
  default:
    return CPERR_INVALID_VERSION;
  }

  return CPERR_SUCCESS;
}

int ResponseStream::ReadData(void* buf, uint32 buf_size) {
  if (ready_state_ < READY_GOT_DATA) {
    // simulate an asynchronous read complete
    g_cptest_funcs.invoke_later(
        InvokeLaterCallback,
        // downcast to Task before void, since we upcast from void to Task.
        static_cast<Task*>(
            NewRunnableMethod(this, &ResponseStream::ReadCompleted,
                              buf, buf_size)),
        500);
    return CPERR_IO_PENDING;
  }

  // synchronously complete the read
  return ReadCompleted(buf, buf_size);
}

void ResponseStream::ResponseStarted() {
  ready_state_ = READY_GOT_HEADERS;
  g_cpresponse_funcs.start_completed(request_, CPERR_SUCCESS);
}

int ResponseStream::ReadCompleted(void* buf, uint32 buf_size) {
  uint32 size = static_cast<uint32>(strlen(payload_->body));
  uint32 avail = size - offset_;
  uint32 count = buf_size;
  if (count > avail)
    count = avail;

  if (count) {
    memcpy(buf, payload_->body + offset_, count);
  }

  offset_ += count;

  if (ready_state_ < READY_GOT_DATA) {
    ready_state_ = READY_GOT_DATA;
    g_cpresponse_funcs.read_completed(request_, static_cast<int>(count));
  }

  return count;
}

// CPP Funcs

CPError STDCALL CPP_Shutdown() {
  return CPERR_SUCCESS;
}

CPBool STDCALL CPP_ShouldInterceptRequest(CPRequest* request) {
  DCHECK(base::strncasecmp(request->url, kChromeTestPluginProtocol,
                           arraysize(kChromeTestPluginProtocol) - 1) == 0);
  return FindPayload(request->url) != NULL;
}

CPError STDCALL CPR_StartRequest(CPRequest* request) {
  const TestResponsePayload* payload = FindPayload(request->url);
  DCHECK(payload);
  ResponseStream* stream = new ResponseStream(payload, request);
  stream->AddRef();  // Released in CPR_EndRequest
  stream->Init();
  request->pdata = stream;
  return payload->async ? CPERR_IO_PENDING : CPERR_SUCCESS;
}

void STDCALL CPR_EndRequest(CPRequest* request, CPError reason) {
  ResponseStream* stream = static_cast<ResponseStream*>(request->pdata);
  request->pdata = NULL;
  stream->Release();  // balances AddRef in CPR_StartRequest
}

void STDCALL CPR_SetExtraRequestHeaders(CPRequest* request,
                                        const char* headers) {
  // doesn't affect us
}

void STDCALL CPR_SetRequestLoadFlags(CPRequest* request, uint32 flags) {
  // doesn't affect us
}

void STDCALL CPR_AppendDataToUpload(CPRequest* request, const char* bytes,
                                    int bytes_len) {
  // doesn't affect us
}

CPError STDCALL CPR_AppendFileToUpload(CPRequest* request, const char* filepath,
                                       uint64 offset, uint64 length) {
  // doesn't affect us
  return CPERR_FAILURE;
}

int STDCALL CPR_GetResponseInfo(CPRequest* request, CPResponseInfoType type,
                        void* buf, uint32 buf_size) {
  ResponseStream* stream = static_cast<ResponseStream*>(request->pdata);
  return stream->GetResponseInfo(type, buf, buf_size);
}

int STDCALL CPR_Read(CPRequest* request, void* buf, uint32 buf_size) {
  ResponseStream* stream = static_cast<ResponseStream*>(request->pdata);
  return stream->ReadData(buf, buf_size);
}

// RequestResponse: manages the retrieval of response data from the host

class RequestResponse {
public:
  RequestResponse(const std::string& raw_headers)
      : raw_headers_(raw_headers), offset_(0) {}
  void StartReading(CPRequest* request);
  void ReadCompleted(CPRequest* request, int bytes_read);

private:
  std::string raw_headers_;
  std::string body_;
  int offset_;
};

void RequestResponse::StartReading(CPRequest* request) {
  int rv = 0;
  const uint32 kReadSize = 4096;
  do {
    body_.resize(offset_ + kReadSize);
    rv = g_cprequest_funcs.read(request, &body_[offset_], kReadSize);
    if (rv > 0)
      offset_ += rv;
  } while (rv > 0);

  if (rv != CPERR_IO_PENDING) {
    // Either an error occurred, or we are done.
    ReadCompleted(request, rv);
  }
}

void RequestResponse::ReadCompleted(CPRequest* request, int bytes_read) {
  if (bytes_read > 0) {
    offset_ += bytes_read;
    StartReading(request);
    return;
  }

  body_.resize(offset_);
  bool success = (bytes_read == 0);
  g_cptest_funcs.test_complete(request, success, raw_headers_, body_);
  g_cprequest_funcs.end_request(request, CPERR_CANCELLED);
  delete this;
}

void STDCALL CPRR_ReceivedRedirect(CPRequest* request, const char* new_url) {
}

void STDCALL CPRR_StartCompleted(CPRequest* request, CPError result) {
  DCHECK(!request->pdata);

  std::string raw_headers;
  int size = g_cprequest_funcs.get_response_info(
      request, CPRESPONSEINFO_HTTP_RAW_HEADERS, NULL, 0);
  int rv = size < 0 ? size : g_cprequest_funcs.get_response_info(
      request, CPRESPONSEINFO_HTTP_RAW_HEADERS,
      WriteInto(&raw_headers, size+1), size);
  if (rv != CPERR_SUCCESS) {
    g_cptest_funcs.test_complete(request, false, std::string(), std::string());
    g_cprequest_funcs.end_request(request, CPERR_CANCELLED);
    return;
  }

  RequestResponse* response = new RequestResponse(raw_headers);
  request->pdata = response;
  response->StartReading(request);
}

void STDCALL CPRR_ReadCompleted(CPRequest* request, int bytes_read) {
  RequestResponse* response =
      reinterpret_cast<RequestResponse*>(request->pdata);
  response->ReadCompleted(request, bytes_read);
}

int STDCALL CPT_MakeRequest(const char* method, const GURL& url) {
  CPRequest* request = NULL;
  if (g_cpbrowser_funcs.create_request(g_cpid, NULL, method, url.spec().c_str(),
                                       &request) != CPERR_SUCCESS ||
      !request) {
    return CPERR_FAILURE;
  }

  g_cprequest_funcs.set_request_load_flags(request,
                                           CPREQUESTLOAD_DISABLE_INTERCEPT);

  if (strcmp(method, "POST") == 0) {
    g_cprequest_funcs.set_extra_request_headers(
        request, "Content-Type: text/plain");
    g_cprequest_funcs.append_data_to_upload(
        request, kChromeTestPluginPostData,
        arraysize(kChromeTestPluginPostData) - 1);
  }

  int rv = g_cprequest_funcs.start_request(request);
  if (rv == CPERR_SUCCESS) {
    CPRR_StartCompleted(request, CPERR_SUCCESS);
  } else if (rv != CPERR_IO_PENDING) {
    g_cprequest_funcs.end_request(request, CPERR_CANCELLED);
    return CPERR_FAILURE;
  }

  return CPERR_SUCCESS;
}

// DLL entry points

CPError STDCALL CP_Initialize(CPID id, const CPBrowserFuncs* bfuncs,
                              CPPluginFuncs* pfuncs) {
  if (bfuncs == NULL || pfuncs == NULL)
    return CPERR_FAILURE;

  if (CP_GET_MAJOR_VERSION(bfuncs->version) > CP_MAJOR_VERSION)
    return CPERR_INVALID_VERSION;

  if (bfuncs->size < sizeof(CPBrowserFuncs) ||
      pfuncs->size < sizeof(CPPluginFuncs))
    return CPERR_INVALID_VERSION;

  pfuncs->version = CP_VERSION;
  pfuncs->shutdown = CPP_Shutdown;
  pfuncs->should_intercept_request = CPP_ShouldInterceptRequest;

  static CPRequestFuncs request_funcs;
  request_funcs.start_request = CPR_StartRequest;
  request_funcs.end_request = CPR_EndRequest;
  request_funcs.set_extra_request_headers = CPR_SetExtraRequestHeaders;
  request_funcs.set_request_load_flags = CPR_SetRequestLoadFlags;
  request_funcs.append_data_to_upload = CPR_AppendDataToUpload;
  request_funcs.get_response_info = CPR_GetResponseInfo;
  request_funcs.read = CPR_Read;
  request_funcs.append_file_to_upload = CPR_AppendFileToUpload;
  pfuncs->request_funcs = &request_funcs;

  static CPResponseFuncs response_funcs;
  response_funcs.received_redirect = CPRR_ReceivedRedirect;
  response_funcs.start_completed = CPRR_StartCompleted;
  response_funcs.read_completed = CPRR_ReadCompleted;
  pfuncs->response_funcs = &response_funcs;

  g_cpid = id;
  g_cpbrowser_funcs = *bfuncs;
  g_cprequest_funcs = *bfuncs->request_funcs;
  g_cpresponse_funcs = *bfuncs->response_funcs;
  g_cpbrowser_funcs = *bfuncs;

  const char* protocols[] = {kChromeTestPluginProtocol};
  g_cpbrowser_funcs.enable_request_intercept(g_cpid, protocols, 1);
  return CPERR_SUCCESS;
}

int STDCALL CP_Test(void* vparam) {
  TestFuncParams* param = reinterpret_cast<TestFuncParams*>(vparam);
  param->pfuncs.test_make_request = CPT_MakeRequest;

  g_cptest_funcs = param->bfuncs;
  return CPERR_SUCCESS;
}
