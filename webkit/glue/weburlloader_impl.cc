// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of WebURLLoader in terms of ResourceLoaderBridge.

#include "webkit/glue/weburlloader_impl.h"

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/data_url.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_headers.h"
#include "webkit/api/public/WebHTTPHeaderVisitor.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebURLLoaderClient.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/public/WebURLResponse.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webkit_glue.h"

using base::Time;
using base::TimeDelta;
using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebHTTPHeaderVisitor;
using WebKit::WebString;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderClient;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

namespace webkit_glue {

// Utilities ------------------------------------------------------------------

namespace {

class HeaderFlattener : public WebHTTPHeaderVisitor {
 public:
  explicit HeaderFlattener(int load_flags)
      : load_flags_(load_flags),
        has_accept_header_(false) {
  }

  virtual void visitHeader(const WebString& name, const WebString& value) {
    // TODO(darin): is UTF-8 really correct here?  It is if the strings are
    // already ASCII (i.e., if they are already escaped properly).
    const std::string& name_utf8 = WebStringToStdString(name);
    const std::string& value_utf8 = WebStringToStdString(value);

    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.  We likewise prune the UA since
    // that will be added back by the network layer.
    if (LowerCaseEqualsASCII(name_utf8, "referer") ||
        LowerCaseEqualsASCII(name_utf8, "user-agent"))
      return;

    // Skip over "Cache-Control: max-age=0" header if the corresponding
    // load flag is already specified. FrameLoader sets both the flag and
    // the extra header -- the extra header is redundant since our network
    // implementation will add the necessary headers based on load flags.
    // See http://code.google.com/p/chromium/issues/detail?id=3434.
    if ((load_flags_ & net::LOAD_VALIDATE_CACHE) &&
        LowerCaseEqualsASCII(name_utf8, "cache-control") &&
        LowerCaseEqualsASCII(value_utf8, "max-age=0"))
      return;

    if (LowerCaseEqualsASCII(name_utf8, "accept"))
      has_accept_header_ = true;

    if (!buffer_.empty())
      buffer_.append("\r\n");
    buffer_.append(name_utf8 + ": " + value_utf8);
  }

  const std::string& GetBuffer() {
    // In some cases, WebKit doesn't add an Accept header, but not having the
    // header confuses some web servers.  See bug 808613.
    if (!has_accept_header_) {
      if (!buffer_.empty())
        buffer_.append("\r\n");
      buffer_.append("Accept: */*");
      has_accept_header_ = true;
    }
    return buffer_;
  }

 private:
  int load_flags_;
  std::string buffer_;
  bool has_accept_header_;
};

ResourceType::Type FromTargetType(WebURLRequest::TargetType type) {
  switch (type) {
    case WebURLRequest::TargetIsMainFrame:
      return ResourceType::MAIN_FRAME;
    case WebURLRequest::TargetIsSubFrame:
      return ResourceType::SUB_FRAME;
    case WebURLRequest::TargetIsSubResource:
      return ResourceType::SUB_RESOURCE;
    case WebURLRequest::TargetIsObject:
      return ResourceType::OBJECT;
    case WebURLRequest::TargetIsMedia:
      return ResourceType::MEDIA;
    default:
      NOTREACHED();
      return ResourceType::SUB_RESOURCE;
  }
}

// Extracts the information from a data: url.
bool GetInfoFromDataURL(const GURL& url,
                        ResourceLoaderBridge::ResponseInfo* info,
                        std::string* data, URLRequestStatus* status) {
  std::string mime_type;
  std::string charset;
  if (net::DataURL::Parse(url, &mime_type, &charset, data)) {
    *status = URLRequestStatus(URLRequestStatus::SUCCESS, 0);
    info->request_time = Time::Now();
    info->response_time = Time::Now();
    info->headers = NULL;
    info->mime_type.swap(mime_type);
    info->charset.swap(charset);
    info->security_info.clear();
    info->content_length = -1;

    return true;
  }

  *status = URLRequestStatus(URLRequestStatus::FAILED, net::ERR_INVALID_URL);
  return false;
}

void PopulateURLResponse(
    const GURL& url,
    const ResourceLoaderBridge::ResponseInfo& info,
    WebURLResponse* response) {
  response->setURL(url);
  response->setMIMEType(StdStringToWebString(info.mime_type));
  response->setTextEncodingName(StdStringToWebString(info.charset));
  response->setExpectedContentLength(info.content_length);
  response->setSecurityInfo(info.security_info);
  response->setAppCacheID(info.app_cache_id);

  const net::HttpResponseHeaders* headers = info.headers;
  if (!headers)
    return;

  response->setHTTPStatusCode(headers->response_code());
  response->setHTTPStatusText(StdStringToWebString(headers->GetStatusText()));

  // TODO(darin): We should leverage HttpResponseHeaders for this, and this
  // should be using the same code as ResourceDispatcherHost.
  // TODO(jungshik): Figure out the actual value of the referrer charset and
  // pass it to GetSuggestedFilename.
  std::string value;
  if (headers->EnumerateHeader(NULL, "content-disposition", &value)) {
    response->setSuggestedFileName(WideToUTF16Hack(
        net::GetSuggestedFilename(url, value, "", std::wstring())));
  }

  Time time_val;
  if (headers->GetLastModifiedValue(&time_val))
    response->setLastModifiedDate(time_val.ToDoubleT());

  // Build up the header map.
  void* iter = NULL;
  std::string name;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response->addHTTPHeaderField(StdStringToWebString(name),
                                 StdStringToWebString(value));
  }
}

} // namespace

// WebURLLoaderImpl::Context --------------------------------------------------

// This inner class exists since the WebURLLoader may be deleted while inside a
// call to WebURLLoaderClient.  The bridge requires its Peer to stay alive
// until it receives OnCompletedRequest.
class WebURLLoaderImpl::Context : public base::RefCounted<Context>,
                                  public ResourceLoaderBridge::Peer {
 public:
  explicit Context(WebURLLoaderImpl* loader);

  WebURLLoaderClient* client() const { return client_; }
  void set_client(WebURLLoaderClient* client) { client_ = client; }

  void Cancel();
  void SetDefersLoading(bool value);
  void Start(
      const WebURLRequest& request,
      ResourceLoaderBridge::SyncLoadResponse* sync_load_response);

  // ResourceLoaderBridge::Peer methods:
  virtual void OnUploadProgress(uint64 position, uint64 size);
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const ResourceLoaderBridge::ResponseInfo& info, bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(
      const URLRequestStatus& status, const std::string& security_info);
  virtual std::string GetURLForDebugging();

 private:
  friend class base::RefCounted<Context>;
  ~Context() {}

  void HandleDataURL();

  WebURLLoaderImpl* loader_;
  GURL url_;
  WebURLLoaderClient* client_;
  scoped_ptr<ResourceLoaderBridge> bridge_;
  scoped_ptr<MultipartResponseDelegate> multipart_delegate_;
  int64 expected_content_length_;
};

WebURLLoaderImpl::Context::Context(WebURLLoaderImpl* loader)
    : loader_(loader),
      client_(NULL) {
}

void WebURLLoaderImpl::Context::Cancel() {
  // The bridge will still send OnCompletedRequest, which will Release() us, so
  // we don't do that here.
  if (bridge_.get())
    bridge_->Cancel();

  // Ensure that we do not notify the multipart delegate anymore as it has
  // its own pointer to the client.
  multipart_delegate_.reset();

  // Do not make any further calls to the client.
  client_ = NULL;
  loader_ = NULL;
}

void WebURLLoaderImpl::Context::SetDefersLoading(bool value) {
  if (bridge_.get())
    bridge_->SetDefersLoading(value);
}

void WebURLLoaderImpl::Context::Start(
    const WebURLRequest& request,
    ResourceLoaderBridge::SyncLoadResponse* sync_load_response) {
  DCHECK(!bridge_.get());

  url_ = request.url();
  if (url_.SchemeIs("data")) {
    if (sync_load_response) {
      // This is a sync load. Do the work now.
      sync_load_response->url = url_;
      std::string data;
      GetInfoFromDataURL(sync_load_response->url, sync_load_response,
                         &sync_load_response->data,
                         &sync_load_response->status);
    } else {
      AddRef();  // Balanced in OnCompletedRequest
      MessageLoop::current()->PostTask(FROM_HERE,
          NewRunnableMethod(this, &Context::HandleDataURL));
    }
    return;
  }

  GURL referrer_url(WebStringToStdString(
      request.httpHeaderField(WebString::fromUTF8("Referer"))));
  const std::string& method = WebStringToStdString(request.httpMethod());

  int load_flags = net::LOAD_NORMAL;
  switch (request.cachePolicy()) {
    case WebURLRequest::ReloadIgnoringCacheData:
      // Required by LayoutTests/http/tests/misc/refresh-headers.php
      load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case WebURLRequest::ReturnCacheDataElseLoad:
      load_flags |= net::LOAD_PREFERRING_CACHE;
      break;
    case WebURLRequest::ReturnCacheDataDontLoad:
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case WebURLRequest::UseProtocolCachePolicy:
      break;
  }

  if (request.reportUploadProgress())
    load_flags |= net::LOAD_ENABLE_UPLOAD_PROGRESS;

  // TODO(jcampan): in the non out-of-process plugin case the request does not
  // have a requestor_pid. Find a better place to set this.
  int requestor_pid = request.requestorProcessID();
  if (requestor_pid == 0)
    requestor_pid = base::GetCurrentProcId();

  HeaderFlattener flattener(load_flags);
  request.visitHTTPHeaderFields(&flattener);

  // TODO(abarth): These are wrong!  I need to figure out how to get the right
  //               strings here.  See: http://crbug.com/8706
  std::string frame_origin = request.firstPartyForCookies().spec();
  std::string main_frame_origin = request.firstPartyForCookies().spec();

  // TODO(brettw) this should take parameter encoding into account when
  // creating the GURLs.
  bridge_.reset(ResourceLoaderBridge::Create(
      method,
      url_,
      request.firstPartyForCookies(),
      referrer_url,
      frame_origin,
      main_frame_origin,
      flattener.GetBuffer(),
      load_flags,
      requestor_pid,
      FromTargetType(request.targetType()),
      request.appCacheContextID(),
      request.requestorID()));

  if (!request.httpBody().isNull()) {
    // GET and HEAD requests shouldn't have http bodies.
    DCHECK(method != "GET" && method != "HEAD");
    const WebHTTPBody& httpBody = request.httpBody();
    size_t i = 0;
    WebHTTPBody::Element element;
    while (httpBody.elementAt(i++, element)) {
      switch (element.type) {
        case WebHTTPBody::Element::TypeData:
          if (!element.data.isEmpty()) {
            // WebKit sometimes gives up empty data to append. These aren't
            // necessary so we just optimize those out here.
            bridge_->AppendDataToUpload(
                element.data.data(), static_cast<int>(element.data.size()));
          }
          break;
        case WebHTTPBody::Element::TypeFile:
          bridge_->AppendFileToUpload(
              FilePath(WebStringToFilePathString(element.filePath)));
          break;
        default:
          NOTREACHED();
      }
    }
    bridge_->SetUploadIdentifier(request.httpBody().identifier());
  }

  if (sync_load_response) {
    bridge_->SyncLoad(sync_load_response);
    return;
  }

  if (bridge_->Start(this)) {
    AddRef();  // Balanced in OnCompletedRequest
  } else {
    bridge_.reset();
  }
}

void WebURLLoaderImpl::Context::OnUploadProgress(uint64 position, uint64 size) {
  if (client_)
    client_->didSendData(loader_, position, size);
}

void WebURLLoaderImpl::Context::OnReceivedRedirect(const GURL& new_url) {
  if (!client_)
    return;

  // TODO(darin): We lack sufficient information to construct the
  // actual redirect response, so we just simulate it here.
  WebURLResponse response(url_);

  // TODO(darin): We lack sufficient information to construct the
  // actual request that resulted from the redirect, so we just
  // report a GET navigation to the new location.
  WebURLRequest new_request(new_url);

  url_ = new_url;
  client_->willSendRequest(loader_, new_request, response);

  // TODO(darin): since new_request is sent as a mutable reference, it is
  // possible that willSendRequest may have modified it.
  //
  // andresca on #webkit confirms that that is intentional, so we'll need
  // to rework the ResourceLoaderBridge to give us control over what URL
  // is really loaded (and with what headers) when a redirect is encountered.
  // TODO(darin): we fail this assertion in some layout tests!
  //DCHECK(GURL(new_request.url()) == new_url);
}

void WebURLLoaderImpl::Context::OnReceivedResponse(
    const ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  if (!client_)
    return;

  WebURLResponse response;
  response.initialize();
  PopulateURLResponse(url_, info, &response);
  response.setIsContentFiltered(content_filtered);

  expected_content_length_ = response.expectedContentLength();

  client_->didReceiveResponse(loader_, response);

  // we may have been cancelled after didReceiveResponse, which would leave us
  // without a client and therefore without much need to do multipart handling.
  if (!client_)
    return;

  DCHECK(!multipart_delegate_.get());
  if (info.headers && info.mime_type == "multipart/x-mixed-replace") {
    std::string content_type;
    info.headers->EnumerateHeader(NULL, "content-type", &content_type);

    std::string boundary = net::GetHeaderParamValue(content_type, "boundary");
    TrimString(boundary, " \"", &boundary);

    // If there's no boundary, just handle the request normally.  In the gecko
    // code, nsMultiMixedConv::OnStartRequest throws an exception.
    if (!boundary.empty()) {
      multipart_delegate_.reset(
          new MultipartResponseDelegate(client_, loader_, response, boundary));
    }
  }
}

void WebURLLoaderImpl::Context::OnReceivedData(const char* data, int len) {
  if (!client_)
    return;

  if (multipart_delegate_.get()) {
    // The multipart delegate will make the appropriate calls to
    // client_->didReceiveData and client_->didReceiveResponse.
    multipart_delegate_->OnReceivedData(data, len);
  } else {
    client_->didReceiveData(loader_, data, len, expected_content_length_);
  }
}

void WebURLLoaderImpl::Context::OnCompletedRequest(
    const URLRequestStatus& status,
    const std::string& security_info) {
  if (multipart_delegate_.get()) {
    multipart_delegate_->OnCompletedRequest();
    multipart_delegate_.reset(NULL);
  }

  // Prevent any further IPC to the browser now that we're complete.
  bridge_.reset();

  if (client_) {
    if (status.status() != URLRequestStatus::SUCCESS) {
      int error_code;
      if (status.status() == URLRequestStatus::HANDLED_EXTERNALLY) {
        // By marking this request as aborted we insure that we don't navigate
        // to an error page.
        error_code = net::ERR_ABORTED;
      } else {
        error_code = status.os_error();
      }
      WebURLError error;
      error.domain = WebString::fromUTF8(net::kErrorDomain);
      error.reason = error_code;
      error.unreachableURL = url_;
      client_->didFail(loader_, error);
    } else {
      client_->didFinishLoading(loader_);
    }
  }

  // We are done with the bridge now, and so we need to release the reference
  // to ourselves that we took on behalf of the bridge.  This may cause our
  // destruction.
  Release();
}

std::string WebURLLoaderImpl::Context::GetURLForDebugging() {
  return url_.spec();
}

void WebURLLoaderImpl::Context::HandleDataURL() {
  ResourceLoaderBridge::ResponseInfo info;
  URLRequestStatus status;
  std::string data;

  if (GetInfoFromDataURL(url_, &info, &data, &status)) {
    OnReceivedResponse(info, false);
    if (!data.empty())
      OnReceivedData(data.data(), data.size());
  }

  OnCompletedRequest(status, info.security_info);
}

// WebURLLoaderImpl -----------------------------------------------------------

WebURLLoaderImpl::WebURLLoaderImpl()
    : ALLOW_THIS_IN_INITIALIZER_LIST(context_(new Context(this))) {
}

WebURLLoaderImpl::~WebURLLoaderImpl() {
  cancel();
}

void WebURLLoaderImpl::loadSynchronously(const WebURLRequest& request,
                                         WebURLResponse& response,
                                         WebURLError& error,
                                         WebData& data) {
  ResourceLoaderBridge::SyncLoadResponse sync_load_response;
  context_->Start(request, &sync_load_response);

  const GURL& final_url = sync_load_response.url;

  // TODO(tc): For file loads, we may want to include a more descriptive
  // status code or status text.
  const URLRequestStatus::Status& status = sync_load_response.status.status();
  if (status != URLRequestStatus::SUCCESS &&
      status != URLRequestStatus::HANDLED_EXTERNALLY) {
    response.setURL(final_url);
    error.domain = WebString::fromUTF8(net::kErrorDomain);
    error.reason = sync_load_response.status.os_error();
    error.unreachableURL = final_url;
    return;
  }

  PopulateURLResponse(final_url, sync_load_response, &response);

  data.assign(sync_load_response.data.data(),
              sync_load_response.data.size());
}

void WebURLLoaderImpl::loadAsynchronously(const WebURLRequest& request,
                                          WebURLLoaderClient* client) {
  DCHECK(!context_->client());

  context_->set_client(client);
  context_->Start(request, NULL);
}

void WebURLLoaderImpl::cancel() {
  context_->Cancel();
}

void WebURLLoaderImpl::setDefersLoading(bool value) {
  context_->SetDefersLoading(value);
}

}  // namespace webkit_glue
