// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/url_request_automation_job.h"

#include "base/message_loop.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/test/automation/automation_messages.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"


// This class manages the interception of network requests for automation.
// It looks at the request, and creates an intercept job if it indicates
// that it should use automation channel.
// NOTE: All methods must be called on the IO thread.
class AutomationRequestInterceptor : public URLRequest::Interceptor {
 public:
  AutomationRequestInterceptor() {
    URLRequest::RegisterRequestInterceptor(this);
  }

  virtual ~AutomationRequestInterceptor() {
    URLRequest::UnregisterRequestInterceptor(this);
  }

  // URLRequest::Interceptor
  virtual URLRequestJob* MaybeIntercept(URLRequest* request);

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationRequestInterceptor);
};

URLRequestJob* AutomationRequestInterceptor::MaybeIntercept(
    URLRequest* request) {
  ResourceDispatcherHost::ExtraRequestInfo* request_info =
      ResourceDispatcherHost::ExtraInfoForRequest(request);
  if (request_info) {
    AutomationResourceMessageFilter::AutomationDetails details;
    if (AutomationResourceMessageFilter::LookupRegisteredRenderView(
            request_info->process_id, request_info->route_id, &details)) {
      URLRequestAutomationJob* job = new URLRequestAutomationJob(request,
          details.tab_handle, details.filter);
      return job;
    }
  }

  return NULL;
}

static URLRequest::Interceptor* GetAutomationRequestInterceptor() {
  return Singleton<AutomationRequestInterceptor>::get();
}

int URLRequestAutomationJob::instance_count_ = 0;

URLRequestAutomationJob::URLRequestAutomationJob(
    URLRequest* request, int tab, AutomationResourceMessageFilter* filter)
    : URLRequestJob(request), id_(0), tab_(tab), message_filter_(filter),
      pending_buf_size_(0) {
  DLOG(INFO) << "URLRequestAutomationJob create. Count: " << ++instance_count_;
  if (message_filter_) {
    id_ = message_filter_->NewRequestId();
    DCHECK(id_);
  } else {
    NOTREACHED();
  }
}

URLRequestAutomationJob::~URLRequestAutomationJob() {
  DLOG(INFO) << "URLRequestAutomationJob delete. Count: " << --instance_count_;
  Cleanup();
}

bool URLRequestAutomationJob::InitializeInterceptor() {
  // AutomationRequestInterceptor will register itself when it
  // is first created.
  URLRequest::Interceptor* interceptor = GetAutomationRequestInterceptor();
  return (interceptor != NULL);
}

// URLRequestJob Implementation.
void URLRequestAutomationJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestAutomationJob::StartAsync));
}

void URLRequestAutomationJob::Kill() {
  message_filter_->Send(new AutomationMsg_RequestEnd(0, tab_, id_,
      URLRequestStatus(URLRequestStatus::CANCELED, 0)));
  DisconnectFromMessageFilter();
  URLRequestJob::Kill();
}

bool URLRequestAutomationJob::ReadRawData(
    net::IOBuffer* buf, int buf_size, int* bytes_read) {
  DLOG(INFO) << "URLRequestAutomationJob: " <<
      request_->url().spec() << " - read pending: " << buf_size;
  pending_buf_ = buf;
  pending_buf_size_ = buf_size;

  message_filter_->Send(new AutomationMsg_RequestRead(0, tab_, id_,
      buf_size));
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  return false;
}

bool URLRequestAutomationJob::GetMimeType(std::string* mime_type) const {
  if (!mime_type_.empty()) {
    *mime_type = mime_type_;
  } else if (headers_) {
    headers_->GetMimeType(mime_type);
  }

  return (!mime_type->empty());
}

bool URLRequestAutomationJob::GetCharset(std::string* charset) {
  if (headers_)
    return headers_->GetCharset(charset);
  return false;
}

void URLRequestAutomationJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (headers_)
    info->headers = headers_;
  if (request_->url().SchemeIsSecure()) {
    // TODO(joshia): fill up SSL related fields.
  }
}

int URLRequestAutomationJob::GetResponseCode() const {
  if (headers_)
    return headers_->response_code();

  static const int kDefaultResponseCode = 200;
  return kDefaultResponseCode;
}

bool URLRequestAutomationJob::IsRedirectResponse(
    GURL* location, int* http_status_code) {
  if (!request_->response_headers())
    return false;

  std::string value;
  if (!request_->response_headers()->IsRedirect(&value))
    return false;

  *location = request_->url().Resolve(value);
  *http_status_code = request_->response_headers()->response_code();
  return true;
}

int URLRequestAutomationJob::MayFilterMessage(const IPC::Message& message) {
  switch (message.type()) {
    case AutomationMsg_RequestStarted::ID:
    case AutomationMsg_RequestData::ID:
    case AutomationMsg_RequestEnd::ID: {
      void* iter = NULL;
      int tab = 0;
      int id = 0;
      if (message.ReadInt(&iter, &tab) && message.ReadInt(&iter, &id)) {
        DCHECK(id);
        return id;
      }
      break;
    }
  }

  return 0;
}

void URLRequestAutomationJob::OnMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(URLRequestAutomationJob, message)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStarted, OnRequestStarted)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestData, OnDataAvailable)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnRequestEnd)
  IPC_END_MESSAGE_MAP()
}

void URLRequestAutomationJob::OnRequestStarted(
    int tab, int id, const IPC::AutomationURLResponse& response) {
  DLOG(INFO) << "URLRequestAutomationJob: " <<
      request_->url().spec() << " - response started.";
  set_expected_content_size(response.content_length);
  mime_type_ = response.mime_type;

  if (!response.headers.empty())
    headers_ = new net::HttpResponseHeaders(response.headers);

  NotifyHeadersComplete();
}

void URLRequestAutomationJob::OnDataAvailable(
    int tab, int id, const std::string& bytes) {
  DLOG(INFO) << "URLRequestAutomationJob: " <<
      request_->url().spec() << " - data available, Size: " << bytes.size();
  DCHECK(!bytes.empty());

  // The request completed, and we have all the data.
  // Clear any IO pending status.
  SetStatus(URLRequestStatus());

  if (pending_buf_ && pending_buf_->data()) {
    DCHECK_GE(pending_buf_size_, bytes.size());
    const int bytes_to_copy = std::min(bytes.size(), pending_buf_size_);
    memcpy(pending_buf_->data(), &bytes[0], bytes_to_copy);

    pending_buf_ = NULL;
    pending_buf_size_ = 0;

    NotifyReadComplete(bytes_to_copy);
  }
}

void URLRequestAutomationJob::OnRequestEnd(
    int tab, int id, const URLRequestStatus& status) {
  DLOG(INFO) << "URLRequestAutomationJob: " <<
      request_->url().spec() << " - request end. Status: " << status.status();

  DisconnectFromMessageFilter();
  NotifyDone(status);

  // Reset any pending reads.
  if (pending_buf_) {
    pending_buf_ = NULL;
    pending_buf_size_ = 0;
    NotifyReadComplete(0);
  }
}

void URLRequestAutomationJob::Cleanup() {
  headers_ = NULL;
  mime_type_.erase();

  id_ = 0;
  tab_ = 0;

  DCHECK(message_filter_ == NULL);
  DisconnectFromMessageFilter();

  pending_buf_ = NULL;
  pending_buf_size_ = 0;
}

void URLRequestAutomationJob::StartAsync() {
  DLOG(INFO) << "URLRequestAutomationJob: start request: " <<
      request_->url().spec();

  // If the job is cancelled before we got a chance to start it
  // we have nothing much to do here.
  if (is_done())
    return;

  if (!request_) {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED,
        net::ERR_FAILED));
    return;
  }

  // Register this request with automation message filter.
  message_filter_->RegisterRequest(this);

  // Ask automation to start this request.
  IPC::AutomationURLRequest automation_request = {
    request_->url().spec(),
    request_->method(),
    request_->referrer(),
    request_->extra_request_headers()
  };

  DCHECK(message_filter_);
  message_filter_->Send(new AutomationMsg_RequestStart(0, tab_, id_,
      automation_request));
}

void URLRequestAutomationJob::DisconnectFromMessageFilter() {
  if (message_filter_) {
    message_filter_->UnRegisterRequest(this);
    message_filter_ = NULL;
  }
}

