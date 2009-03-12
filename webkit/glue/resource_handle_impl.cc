// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file replaces WebCore/platform/network/win/ResourceHandleWin.cpp with a
// platform-neutral implementation that simply defers almost entirely to
// ResouceLoaderBridge.
//
// This uses the same ResourceHandle.h header file that the rest of WebKit
// uses, allowing us to avoid complicated changes. Our specific things are
// added on ResourceHandleInternal.  The ResourceHandle owns the
// ResourceHandleInternal and passes off almost all processing to it.
//
// The WebKit version of this code keeps the ResourceHandle AddRef'd when
// there are any callbacks. This prevents the callbacks from occuring into
// destroyed objects. However, our destructors should always stop callbacks
// from happening, making this (hopefully) unnecessary.
//
// We preserve this behavior for safety. A client could count on this behavior
// and fire off a request, release it, and wait for callbacks to get the data
// as long as it doesn't care about canceling the request. Although this is
// dumb, we support it. We use pending_ to indicate this extra AddRef, which
// is done in start() and released in OnCompletedRequest.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "CString.h"
#include "Console.h"
#include "DocLoader.h"
#include "DOMWindow.h"
#include "FormData.h"
#include "FrameLoader.h"
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
MSVC_POP_WARNING();

#undef LOG
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/time.h"
#include "base/string_util.h"
#include "base/string_tokenizer.h"
#include "webkit/glue/feed_preview.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/webframe_impl.h"
#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"

using webkit_glue::ResourceLoaderBridge;
using base::Time;
using base::TimeDelta;
using net::HttpResponseHeaders;

namespace WebCore {

static ResourceType::Type FromTargetType(ResourceRequest::TargetType type) {
  switch (type) {
    case ResourceRequest::TargetIsMainFrame:
      return ResourceType::MAIN_FRAME;
    case ResourceRequest::TargetIsSubFrame:
      return ResourceType::SUB_FRAME;
    case ResourceRequest::TargetIsSubResource:
      return ResourceType::SUB_RESOURCE;
    case ResourceRequest::TargetIsObject:
      return ResourceType::OBJECT;
    case ResourceRequest::TargetIsMedia:
      return ResourceType::MEDIA;
    default:
      NOTREACHED();
      return ResourceType::SUB_RESOURCE;
  }
}

// Extracts the information from a data: url.
static bool GetInfoFromDataUrl(const GURL& url,
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

static void ExtractInfoFromHeaders(const HttpResponseHeaders* headers,
                                   HTTPHeaderMap* header_map,
                                   int* status_code,
                                   String* status_text,
                                   long long* expected_content_length) {
  *status_code = headers->response_code();

  // Set the status text
  *status_text = webkit_glue::StdStringToString(headers->GetStatusText());

  // Set the content length.
  std::string length_val;
  if (headers->EnumerateHeader(NULL, "content-length", &length_val))
    *expected_content_length = StringToInt64(length_val);

  // Build up the header map.  Take care with duplicate headers.
  void* iter = NULL;
  std::string name, value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    String name_str = webkit_glue::StdStringToString(name);
    String value_str = webkit_glue::StdStringToString(value);

    pair<HTTPHeaderMap::iterator, bool> result =
        header_map->add(name_str, value_str);
    if (!result.second)
      result.first->second += ", " + value_str;
  }
}

static ResourceResponse MakeResourceResponse(
    const KURL& kurl,
    const ResourceLoaderBridge::ResponseInfo& info) {
  int status_code = 0;
  long long expected_content_length = info.content_length;
  String status_text;
  HTTPHeaderMap header_map;

  // It's okay if there are no headers
  if (info.headers)
    ExtractInfoFromHeaders(info.headers,
                           &header_map,
                           &status_code,
                           &status_text,
                           &expected_content_length);

  // TODO(darin): We should leverage HttpResponseHeaders for this, and this
  // should be using the same code as ResourceDispatcherHost.
  std::wstring suggested_filename;
  if (info.headers) {
    std::string disp_val;
    if (info.headers->EnumerateHeader(NULL, "content-disposition", &disp_val)) {
      suggested_filename = net::GetSuggestedFilename(
          webkit_glue::KURLToGURL(kurl), disp_val, std::wstring());
    }
  }

  ResourceResponse response(kurl,
      webkit_glue::StdStringToString(info.mime_type),
      expected_content_length,
      webkit_glue::StdStringToString(info.charset),
      webkit_glue::StdWStringToString(suggested_filename));

  if (info.headers) {
    Time time_val;
    if (info.headers->GetLastModifiedValue(&time_val))
      response.setLastModifiedDate(time_val.ToTimeT());

    // Compute expiration date
    TimeDelta freshness_lifetime =
        info.headers->GetFreshnessLifetime(info.response_time);
    if (freshness_lifetime != TimeDelta()) {
      Time now = Time::Now();
      TimeDelta current_age =
          info.headers->GetCurrentAge(info.request_time, info.response_time,
                                      now);
      time_val = now + freshness_lifetime - current_age;

      response.setExpirationDate(time_val.ToTimeT());
    } else {
      // WebKit uses 0 as a special expiration date that means never expire.
      // 1 is a small enough value to let it always expire.
      response.setExpirationDate(1);
    }
  }

  response.setHTTPStatusCode(status_code);
  response.setHTTPStatusText(status_text);
  response.setSecurityInfo(webkit_glue::StdStringToCString(info.security_info));

  // WebKit doesn't provide a way for us to set expected content length after
  // calling the constructor, so we parse the headers first and then swap in
  // our HTTP header map. Ideally we would like a setter for expected content
  // length (perhaps by abstracting ResourceResponse interface into
  // ResourceResponseBase) but that would require forking.
  const_cast<HTTPHeaderMap*>(&response.httpHeaderFields())->swap(header_map);

  return response;
}

class ResourceHandleInternal : public ResourceLoaderBridge::Peer {
 public:
  ResourceHandleInternal(ResourceHandle* job, const ResourceRequest& r,
                         ResourceHandleClient* c);
  ~ResourceHandleInternal();

  // If the response parameter is null, then an asynchronous load is started.
  bool Start(ResourceLoaderBridge::SyncLoadResponse* response);

  // Used to cancel an asynchronous load.
  void Cancel();

  // Used to suspend/resume an asynchronous load.
  void SetDefersLoading(bool value);

  // ResourceLoaderBridge::Peer implementation
  virtual void OnUploadProgress(uint64 position, uint64 size);
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info);
  virtual std::string GetURLForDebugging();

  // Handles a data: url internally instead of calling the bridge.
  void HandleDataUrl();

  // This is the bridge implemented by the embedder.
  // The bridge is kept alive as long as the request is valid and we
  // are ready for callbacks.
  scoped_ptr<ResourceLoaderBridge> bridge_;

  // The resource loader that owns us
  ResourceHandle* job_;

  // This is the object that receives various status messages (such as when the
  // loader has received data).  See definition for the exact messages that are
  // sent to it.
  ResourceHandleClient* client_;

  ResourceRequest request_;

  // Runnable Method Factory used to invoke later HandleDataUrl().
  ScopedRunnableMethodFactory<ResourceHandleInternal> data_url_factory_;

  int load_flags_;

 private:
  // Set to true when we're waiting for data from the bridge, also indicating
  // we have addrefed our job.
  bool pending_;

  // Expected content length of the response
  long long expected_content_length_;

  // NULL unless we are handling a multipart/x-mixed-replace request
  scoped_ptr<MultipartResponseDelegate> multipart_delegate_;

  // NULL unless we are handling a feed:// request.
  scoped_ptr<FeedClientProxy> feed_client_proxy_;
};

ResourceHandleInternal::ResourceHandleInternal(ResourceHandle* job,
                                               const ResourceRequest& r,
                                               ResourceHandleClient* c)
    : job_(job),
      client_(c),
      request_(r),
MSVC_SUPPRESS_WARNING(4355)  // can use this
      data_url_factory_(this),
      load_flags_(net::LOAD_NORMAL),
      pending_(false),
      expected_content_length_(-1),
      multipart_delegate_(NULL) {
}

ResourceHandleInternal::~ResourceHandleInternal() {
  DCHECK(!pending_);
}

void ResourceHandleInternal::HandleDataUrl() {
  ResourceLoaderBridge::ResponseInfo info;
  URLRequestStatus status;
  std::string data;

  if (GetInfoFromDataUrl(webkit_glue::KURLToGURL(request_.url()), &info, &data,
                         &status)) {
    OnReceivedResponse(info, false);

    if (data.size())
      OnReceivedData(data.c_str(), data.size());
  }

  OnCompletedRequest(status, info.security_info);

  // We are done using the object. ResourceHandle and ResourceHandleInternal
  // might be destroyed now.
  job_->deref();
}

bool ResourceHandleInternal::Start(
    ResourceLoaderBridge::SyncLoadResponse* sync_load_response) {
  DCHECK(!bridge_.get());

  // The WebFrame is the Frame's FrameWinClient
  WebFrameImpl* webframe =
      request_.frame() ? WebFrameImpl::FromFrame(request_.frame()) : NULL;

  CString method = request_.httpMethod().latin1();
  GURL referrer(webkit_glue::StringToStdString(request_.httpReferrer()));

  // Compute the URL of the load.
  GURL url = webkit_glue::KURLToGURL(request_.url());
  if (url.SchemeIs("feed:")) {
    // Feed URLs are special, they actually mean "http".
    url_canon::Replacements<char> replacements;
    replacements.SetScheme("http", url_parse::Component(0, 4));
    url = url.ReplaceComponents(replacements);

    // Replace our client with a client that understands previewing feeds
    // and forwards the feeds along to the original client.
    feed_client_proxy_.reset(new FeedClientProxy(client_));
    client_ = feed_client_proxy_.get();
  }

  // Inherit the policy URL from the request's frame. However, if the request
  // is for a main frame, the current document's policyBaseURL is the old
  // document, so we leave policyURL empty to indicate that the request is a
  // first-party request.
  GURL policy_url;
  if (request_.targetType() != ResourceRequest::TargetIsMainFrame &&
      request_.frame() && request_.frame()->document()) {
    policy_url = GURL(webkit_glue::StringToStdString(
        request_.frame()->document()->policyBaseURL()));
  }

  switch (request_.cachePolicy()) {
    case ReloadIgnoringCacheData:
      // Required by LayoutTests/http/tests/misc/refresh-headers.php
      load_flags_ |= net::LOAD_VALIDATE_CACHE;
      break;
    case ReturnCacheDataElseLoad:
      load_flags_ |= net::LOAD_PREFERRING_CACHE;
      break;
    case ReturnCacheDataDontLoad:
      load_flags_ |= net::LOAD_ONLY_FROM_CACHE;
      break;
    case UseProtocolCachePolicy:
      break;
  }

  if (request_.reportUploadProgress())
    load_flags_ |= net::LOAD_ENABLE_UPLOAD_PROGRESS;

  // Translate the table of request headers to a formatted string blob
  String headerBuf;
  const HTTPHeaderMap& headerMap = request_.httpHeaderFields();

  // In some cases, WebCore doesn't add an Accept header, but not having the
  // header confuses some web servers.  See bug 808613.
  // Note: headerMap uses case-insenstive keys, so this will find Accept as
  //       as well.
  if (!headerMap.contains("accept"))
    request_.addHTTPHeaderField("Accept", "*/*");

  const String crlf("\r\n");
  const String sep(": ");
  for (HTTPHeaderMap::const_iterator it = headerMap.begin();
       it != headerMap.end(); ++it) {
    // Skip over referrer headers found in the header map because we already
    // pulled it out as a separate parameter.  We likewise prune the UA since
    // that will be added back by the network layer.
    if (equalIgnoringCase((*it).first, "referer") ||
        equalIgnoringCase((*it).first, "user-agent"))
      continue;

    // Skip over "Cache-Control: max-age=0" header if the corresponding
    // load flag is already specified. FrameLoader sets both the flag and
    // the extra header -- the extra header is redundant since our network
    // implementation will add the necessary headers based on load flags.
    // See http://code.google.com/p/chromium/issues/detail?id=3434.
    if ((load_flags_ & net::LOAD_VALIDATE_CACHE) &&
        equalIgnoringCase((*it).first, "cache-control") &&
        (*it).second == "max-age=0")
      continue;

    // WinInet dies if blank headers are set.  TODO(darin): Is this still an
    // issue now that we are using WinHTTP?
    if ((*it).first.isEmpty()) {
      webframe->frame()->domWindow()->console()->addMessage(
        JSMessageSource,
        ErrorMessageLevel,
        "Refused to set blank header",
        1,
        String());
      continue;
    }
    if (!headerBuf.isEmpty())
      headerBuf.append(crlf);
    headerBuf.append((*it).first + sep + (*it).second);
  }

  // TODO(jcampan): in the non out-of-process plugin case the request does not
  // have a origin_pid. Find a better place to set this.
  int origin_pid = request_.originPid();
  if (origin_pid == 0)
    origin_pid = base::GetCurrentProcId();

  bool mixed_content =
      webkit_glue::KURLToGURL(request_.mainDocumentURL()).SchemeIsSecure() &&
      !url.SchemeIsSecure();

  if (url.SchemeIs("data")) {
    if (sync_load_response) {
      // This is a sync load. Do the work now.
      sync_load_response->url = url;
      std::string data;
      GetInfoFromDataUrl(sync_load_response->url, sync_load_response,
                         &sync_load_response->data,
                         &sync_load_response->status);
    } else {
      pending_ = true;
      job_->ref();  // to be released when we get a OnCompletedRequest.
      job_->ref();  // to be released when HandleDataUrl is completed.
      MessageLoop::current()->PostTask(FROM_HERE,
          data_url_factory_.NewRunnableMethod(
              &ResourceHandleInternal::HandleDataUrl));
    }
    return true;
  }

  // TODO(darin): is latin1 really correct here?  It is if the strings are
  // already ASCII (i.e., if they are already escaped properly).
  // TODO(brettw) this should take parameter encoding into account when
  // creating the GURLs.
  bridge_.reset(ResourceLoaderBridge::Create(
      webframe,
      webkit_glue::CStringToStdString(method),
      url,
      policy_url,
      referrer,
      webkit_glue::CStringToStdString(headerBuf.latin1()),
      load_flags_,
      origin_pid,
      FromTargetType(request_.targetType()),
      mixed_content));
  if (!bridge_.get())
    return false;

  if (request_.httpBody()) {
    // GET and HEAD requests shouldn't have http bodies.
    DCHECK(method != "GET" && method != "HEAD");
    const Vector<FormDataElement>& elements = request_.httpBody()->elements();
    size_t n = elements.size();
    for (size_t i = 0; i < n; ++i) {
      const FormDataElement& e = elements[static_cast<unsigned>(i)];
      if (e.m_type == FormDataElement::data) {
        if (e.m_data.size() > 0) {
          // WebKit sometimes gives up empty data to append. These aren't
          // necessary so we just optimize those out here.
          bridge_->AppendDataToUpload(e.m_data.data(),
                                      static_cast<int>(e.m_data.size()));
        }
      } else {
        bridge_->AppendFileToUpload(
            webkit_glue::StringToStdWString(e.m_filename));
      }
    }
  }

  if (sync_load_response) {
    bridge_->SyncLoad(sync_load_response);
    return true;
  }

  bool rv = bridge_->Start(this);
  if (rv) {
    pending_ = true;
    job_->ref();  // to be released when we get a OnCompletedRequest.
  } else {
    bridge_.reset();
  }

  return rv;
}

void ResourceHandleInternal::Cancel() {
  // The bridge will still send OnCompletedRequest, which will deref() us,
  // so we don't do that here.
  if (bridge_.get())
    bridge_->Cancel();

  // Ensure that we do not notify the multipart delegate anymore as it has
  // its own pointer to the client.
  multipart_delegate_.reset();

  // Do not make any further calls to the client.
  client_ = NULL;
}

void ResourceHandleInternal::SetDefersLoading(bool value) {
  if (bridge_.get())
    bridge_->SetDefersLoading(value);
}

// ResourceLoaderBridge::Peer impl --------------------------------------------

void ResourceHandleInternal::OnUploadProgress(uint64 position, uint64 size) {
  if (client_)
    client_->didSendData(job_, position, size);
}

void ResourceHandleInternal::OnReceivedRedirect(const GURL& new_url) {
  DCHECK(pending_);

  KURL url = webkit_glue::GURLToKURL(new_url);

  // TODO(darin): need a way to properly initialize a ResourceResponse
  ResourceResponse response(request_.url(), String(), -1, String(), String());

  ResourceRequest new_request(url);

  // TODO(darin): we need to setup new_request to reflect the fact that we
  // for example drop the httpBody when following a POST request that is
  // redirected to a GET request.

  if (client_)
    client_->willSendRequest(job_, new_request, response);

  //
  // TODO(darin): since new_request is sent as a mutable reference, it is
  // possible that willSendRequest may expect to be able to modify it.
  //
  // andresca on #webkit confirms that that is intentional, so we'll need
  // to rework the ResourceLoaderBridge to give us control over what URL
  // is really loaded (and with what headers) when a redirect is encountered.
  //

  request_ = new_request;
}

void ResourceHandleInternal::OnReceivedResponse(
    const ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  DCHECK(pending_);

  // TODO(darin): need a way to properly initialize a ResourceResponse
  ResourceResponse response = MakeResourceResponse(request_.url(), info);
  response.setIsContentFiltered(content_filtered);

  expected_content_length_ = response.expectedContentLength();

  if (client_)
    client_->didReceiveResponse(job_, response);

  // we may have been cancelled after didReceiveResponse, which would leave us
  // without a client and therefore without much need to do multipart handling.

  DCHECK(!multipart_delegate_.get());
  if (client_ && info.headers && response.isMultipart()) {
    std::string content_type;
    info.headers->EnumerateHeader(NULL, "content-type", &content_type);

    std::string boundary = net::GetHeaderParamValue(content_type, "boundary");
    TrimString(boundary, " \"", &boundary);
    // If there's no boundary, just handle the request normally.  In the gecko
    // code, nsMultiMixedConv::OnStartRequest throws an exception.
    if (!boundary.empty()) {
      multipart_delegate_.reset(new MultipartResponseDelegate(client_, job_,
          response, boundary));
    }
  }

  // TODO(darin): generate willCacheResponse callback.  debug mac webkit to
  // determine when it should be called.
}

void ResourceHandleInternal::OnReceivedData(const char* data, int data_len) {
  DCHECK(pending_);

  if (client_) {
    // TODO(darin): figure out what to pass for lengthReceived.  from reading
    // the loader code, it looks like this is supposed to be the content-length
    // value, but it seems really wacky to include that here!  we have to debug
    // webkit on mac to figure out what this should be.

    // TODO(jackson): didReceiveData expects an int, but an expected content
    // length is an int64, so we do our best to fit it inside an int. The only
    // code that cares currently about this value is the Inspector, so beware
    // that the Inspector's network panel might under-represent the size of
    // some resources if they're larger than a gigabyte.
    int lengthReceived = static_cast<int>(expected_content_length_);
    if (lengthReceived != expected_content_length_)  // overflow occurred
      lengthReceived = -1;

    if (!multipart_delegate_.get()) {
      client_->didReceiveData(job_, data, data_len, lengthReceived);
    } else {
      // AddData will make the appropriate calls to client_->didReceiveData
      // and client_->didReceiveResponse
      multipart_delegate_->OnReceivedData(data, data_len);
    }
  }
}

void ResourceHandleInternal::OnCompletedRequest(
    const URLRequestStatus& status,
    const std::string& security_info) {
  if (multipart_delegate_.get()) {
    multipart_delegate_->OnCompletedRequest();
    multipart_delegate_.reset(NULL);
  }

  pending_ = false;

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
        // TODO(tc): fill in these fields properly
        ResourceError error(net::kErrorDomain,
                            error_code,
                            request_.url().string(),
                            String() /*localized description*/);
        client_->didFail(job_, error);
    } else {
      client_->didFinishLoading(job_);
    }
  }

  job_->deref();  // may destroy our owner and hence |this|
}

std::string ResourceHandleInternal::GetURLForDebugging() {
  return webkit_glue::CStringToStdString(request_.url().string().latin1());
}

// ResourceHandle -------------------------------------------------------------

ResourceHandle::ResourceHandle(const ResourceRequest& request,
                               ResourceHandleClient* client,
                               bool defersLoading,
                               bool shouldContentSniff,
                               bool mightDownloadFromHandle)
MSVC_SUPPRESS_WARNING(4355)  // it's okay to pass |this| here!
      : d(new ResourceHandleInternal(this, request, client)) {
  // TODO(darin): figure out what to do with the two bool params
}

PassRefPtr<ResourceHandle> ResourceHandle::create(const ResourceRequest& request,
                                                  ResourceHandleClient* client,
                                                  Frame* deprecated,
                                                  bool defersLoading,
                                                  bool shouldContentSniff,
                                                  bool mightDownloadFromHandle) {
  RefPtr<ResourceHandle> newHandle =
      adoptRef(new ResourceHandle(request, client, defersLoading,
                         shouldContentSniff, mightDownloadFromHandle));

  if (newHandle->start(NULL))
    return newHandle.release();

  return NULL;
}

const ResourceRequest& ResourceHandle::request() const {
  return d->request_;
}

ResourceHandleClient* ResourceHandle::client() const {
  return d->client_;
}

void ResourceHandle::setClient(ResourceHandleClient* client) {
  d->client_ = client;
}

void ResourceHandle::setDefersLoading(bool value) {
  d->SetDefersLoading(value);
}

bool ResourceHandle::start(Frame* deprecated) {
  return d->Start(NULL);
}

void ResourceHandle::clearAuthentication() {
  // TODO(darin): do something here.  it looks like the ResourceLoader calls
  // this method when it is canceled.  i have no idea why it does this.
}

void ResourceHandle::cancel() {
  d->Cancel();
}

ResourceHandle::~ResourceHandle() {
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData() {
  return NULL;
}

/*static*/ bool ResourceHandle::loadsBlocked() {
  return false;  // this seems to be related to sync XMLHttpRequest...
}

/*static*/ bool ResourceHandle::supportsBufferedData() {
  return false;  // the loader will buffer manually if it needs to
}

/*static*/ void ResourceHandle::loadResourceSynchronously(
    const ResourceRequest& request, ResourceError& error,
    ResourceResponse& response, Vector<char>& data, Frame*) {

  RefPtr<ResourceHandle> handle =
      adoptRef(new ResourceHandle(request, NULL, false, false, false));

  ResourceLoaderBridge::SyncLoadResponse sync_load_response;
  if (!handle->d->Start(&sync_load_response)) {
    response =
        ResourceResponse(request.url(), String(), 0, String(), String());
    // TODO(darin): what should the error code really be?
    error = ResourceError(net::kErrorDomain,
                          net::ERR_FAILED,
                          request.url().string(),
                          String() /* localized description */);
    return;
  }

  KURL kurl = webkit_glue::GURLToKURL(sync_load_response.url);

  // TODO(tc): For file loads, we may want to include a more descriptive
  // status code or status text.
  const URLRequestStatus::Status& status = sync_load_response.status.status();
  if (status != URLRequestStatus::SUCCESS &&
      status != URLRequestStatus::HANDLED_EXTERNALLY) {
    response = ResourceResponse(kurl, String(), 0, String(), String());
    error = ResourceError(net::kErrorDomain,
                          sync_load_response.status.os_error(),
                          kurl.string(),
                          String() /* localized description */);
    return;
  }

  response = MakeResourceResponse(kurl, sync_load_response);

  data.clear();
  data.append(sync_load_response.data.data(),
              sync_load_response.data.size());
}

// static
bool ResourceHandle::willLoadFromCache(ResourceRequest& request) {
  //
  // This method is used to determine if a POST request can be repeated from
  // cache, but you cannot really know until you actually try to read from the
  // cache.  Even if we checked now, something else could come along and wipe
  // out the cache entry by the time we fetch it.
  //
  // So, we always say yes here, which allows us to generate an ERR_CACHE_MISS
  // if the request cannot be serviced from cache.  We force the 'DontLoad'
  // cache policy at this point to ensure that we never hit the network for
  // this request.
  //
  DCHECK(request.httpMethod() == "POST");
  request.setCachePolicy(ReturnCacheDataDontLoad);
  return true;
}

}  // namespace WebCore
