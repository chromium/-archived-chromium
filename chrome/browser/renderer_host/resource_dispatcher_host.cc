// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "chrome/browser/renderer_host/resource_dispatcher_host.h"

#include <vector>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/cross_site_request_manager.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/download/save_file_manager.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/in_process_webkit/webkit_thread.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/async_resource_handler.h"
#include "chrome/browser/renderer_host/buffered_resource_handler.h"
#include "chrome/browser/renderer_host/cross_site_resource_handler.h"
#include "chrome/browser/renderer_host/download_resource_handler.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_request_details.h"
#include "chrome/browser/renderer_host/safe_browsing_resource_handler.h"
#include "chrome/browser/renderer_host/save_file_resource_handler.h"
#include "chrome/browser/renderer_host/sync_resource_handler.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "net/base/auth.h"
#include "net/base/cert_status_flags.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/webappcachecontext.h"

// TODO(port): Move these includes to the above section when porting is done.
#if defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#elif defined(OS_WIN)
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

#if defined(OS_WIN) || defined(OS_LINUX)
#include "chrome/browser/login_prompt.h"
#endif

// Uncomment to enable logging of request traffic.
// #define LOG_RESOURCE_DISPATCHER_REQUESTS

#ifdef LOG_RESOURCE_DISPATCHER_REQUESTS
# define RESOURCE_LOG(stuff) LOG(INFO) << stuff
#else
# define RESOURCE_LOG(stuff)
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// ----------------------------------------------------------------------------

bool ResourceDispatcherHost::g_is_http_prioritization_enabled = true;

// A ShutdownTask proxies a shutdown task from the UI thread to the IO thread.
// It should be constructed on the UI thread and run in the IO thread.
class ResourceDispatcherHost::ShutdownTask : public Task {
 public:
  explicit ShutdownTask(ResourceDispatcherHost* resource_dispatcher_host)
      : rdh_(resource_dispatcher_host) { }

  void Run() {
    rdh_->OnShutdown();
  }

 private:
  ResourceDispatcherHost* rdh_;
};

namespace {

// The interval for calls to ResourceDispatcherHost::UpdateLoadStates
const int kUpdateLoadStatesIntervalMsec = 100;

// Maximum number of pending data messages sent to the renderer at any
// given time for a given request.
const int kMaxPendingDataMessages = 20;

// Maximum byte "cost" of all the outstanding requests for a renderer.
// See delcaration of |max_outstanding_requests_cost_per_process_| for details.
// This bound is 25MB, which allows for around 6000 outstanding requests.
const int kMaxOutstandingRequestsCostPerProcess = 26214400;

// Consults the RendererSecurity policy to determine whether the
// ResourceDispatcherHost should service this request.  A request might be
// disallowed if the renderer is not authorized to restrive the request URL or
// if the renderer is attempting to upload an unauthorized file.
bool ShouldServiceRequest(ChildProcessInfo::ProcessType process_type,
                          int process_id,
                          const ViewHostMsg_Resource_Request& request_data)  {
  if (process_type == ChildProcessInfo::PLUGIN_PROCESS)
    return true;

  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();

  // Check if the renderer is permitted to request the requested URL.
  if (!policy->CanRequestURL(process_id, request_data.url)) {
    LOG(INFO) << "Denied unauthorized request for " <<
        request_data.url.possibly_invalid_spec();
    return false;
  }

  // Check if the renderer is permitted to upload the requested files.
  if (request_data.upload_data) {
    const std::vector<net::UploadData::Element>& uploads =
        request_data.upload_data->elements();
    std::vector<net::UploadData::Element>::const_iterator iter;
    for (iter = uploads.begin(); iter != uploads.end(); ++iter) {
      if (iter->type() == net::UploadData::TYPE_FILE &&
          !policy->CanUploadFile(process_id, iter->file_path())) {
        NOTREACHED() << "Denied unauthorized upload of "
                     << iter->file_path().value();
        return false;
      }
    }
  }

  return true;
}

}  // namespace

ResourceDispatcherHost::ResourceDispatcherHost(MessageLoop* io_loop)
    : ui_loop_(MessageLoop::current()),
      io_loop_(io_loop),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          download_file_manager_(new DownloadFileManager(ui_loop_, this))),
      download_request_manager_(new DownloadRequestManager(io_loop, ui_loop_)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          save_file_manager_(new SaveFileManager(ui_loop_, io_loop, this))),
      safe_browsing_(new SafeBrowsingService),
      webkit_thread_(new WebKitThread),
      request_id_(-1),
      plugin_service_(PluginService::GetInstance()),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_runner_(this)),
      is_shutdown_(false),
      max_outstanding_requests_cost_per_process_(
          kMaxOutstandingRequestsCostPerProcess),
      receiver_(NULL) {
}

ResourceDispatcherHost::~ResourceDispatcherHost() {
  AsyncResourceHandler::GlobalCleanup();
  STLDeleteValues(&pending_requests_);

  // Clear blocked requests if any left.
  // Note that we have to do this in 2 passes as we cannot call
  // CancelBlockedRequestsForRoute while iterating over
  // blocked_requests_map_, as it modifies it.
  std::set<ProcessRouteIDs> ids;
  for (BlockedRequestMap::const_iterator iter = blocked_requests_map_.begin();
       iter != blocked_requests_map_.end(); ++iter) {
    std::pair<std::set<ProcessRouteIDs>::iterator, bool> result =
        ids.insert(iter->first);
    // We should not have duplicates.
    DCHECK(result.second);
  }
  for (std::set<ProcessRouteIDs>::const_iterator iter = ids.begin();
       iter != ids.end(); ++iter) {
    CancelBlockedRequestsForRoute(iter->first, iter->second);
  }
}

void ResourceDispatcherHost::Initialize() {
  DCHECK(MessageLoop::current() == ui_loop_);
  download_file_manager_->Initialize();
  safe_browsing_->Initialize(io_loop_);
}

void ResourceDispatcherHost::Shutdown() {
  DCHECK(MessageLoop::current() == ui_loop_);
  io_loop_->PostTask(FROM_HERE, new ShutdownTask(this));
}

void ResourceDispatcherHost::OnShutdown() {
  DCHECK(MessageLoop::current() == io_loop_);
  is_shutdown_ = true;
  STLDeleteValues(&pending_requests_);
  // Make sure we shutdown the timer now, otherwise by the time our destructor
  // runs if the timer is still running the Task is deleted twice (once by
  // the MessageLoop and the second time by RepeatingTimer).
  update_load_states_timer_.Stop();
}

bool ResourceDispatcherHost::HandleExternalProtocol(int request_id,
                                                    int process_id,
                                                    int tab_contents_id,
                                                    const GURL& url,
                                                    ResourceType::Type type,
                                                    ResourceHandler* handler) {
  if (!ResourceType::IsFrame(type) || URLRequest::IsHandledURL(url))
    return false;

  ui_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      &ExternalProtocolHandler::LaunchUrl, url, process_id, tab_contents_id));

  handler->OnResponseCompleted(request_id, URLRequestStatus(
                                               URLRequestStatus::FAILED,
                                               net::ERR_ABORTED),
                               std::string());  // No security info necessary.
  return true;
}

bool ResourceDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                               Receiver* receiver,
                                               bool* message_was_ok) {
  if (!IsResourceDispatcherHostMessage(message))
    return false;

  *message_was_ok = true;
  receiver_ = receiver;

  IPC_BEGIN_MESSAGE_MAP_EX(ResourceDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestResource, OnRequestResource)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncLoad, OnSyncLoad)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DataReceived_ACK, OnDataReceivedACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UploadProgress_ACK, OnUploadProgressACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClosePage_ACK, OnClosePageACK)
  IPC_END_MESSAGE_MAP_EX()

  receiver_ = NULL;

  return true;
}

void ResourceDispatcherHost::OnRequestResource(
    const IPC::Message& message,
    int request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  BeginRequest(request_id, request_data, NULL, message.routing_id());
}

// Begins a resource request with the given params on behalf of the specified
// child process.  Responses will be dispatched through the given receiver. The
// process ID is used to lookup TabContents from routing_id's in the case of a
// request from a renderer.  request_context is the cookie/cache context to be
// used for this request.
//
// If sync_result is non-null, then a SyncLoad reply will be generated, else
// a normal asynchronous set of response messages will be generated.
void ResourceDispatcherHost::OnSyncLoad(
    int request_id,
    const ViewHostMsg_Resource_Request& request_data,
    IPC::Message* sync_result) {
  BeginRequest(request_id, request_data, sync_result, 0);
}

void ResourceDispatcherHost::BeginRequest(
    int request_id,
    const ViewHostMsg_Resource_Request& request_data,
    IPC::Message* sync_result,  // only valid for sync
    int route_id) {
  ChildProcessInfo::ProcessType process_type = receiver_->type();
  int process_id = receiver_->GetProcessId();
  URLRequestContext* context =
      receiver_->GetRequestContext(request_id, request_data);
  if (!context)
    context = Profile::GetDefaultRequestContext();

  if (is_shutdown_ ||
      !ShouldServiceRequest(process_type, process_id, request_data)) {
    URLRequestStatus status(URLRequestStatus::FAILED, net::ERR_ABORTED);
    if (sync_result) {
      SyncLoadResult result;
      result.status = status;
      ViewHostMsg_SyncLoad::WriteReplyParams(sync_result, result);
      receiver_->Send(sync_result);
    } else {
      // Tell the renderer that this request was disallowed.
      receiver_->Send(new ViewMsg_Resource_RequestComplete(
          route_id,
          request_id,
          status,
          std::string()));  // No security info needed, connection was not
                            // established.
    }
    return;
  }

  // Note that context can still be NULL here when running unit tests.
  const Blacklist::Entry* entry = context && context->blacklist() ?
      context->blacklist()->findMatch(request_data.url) : NULL;
  if (entry && entry->IsBlocked(request_data.url)) {
    // TODO(idanan): Send a ResourceResponse to replace the blocked resource.
    return;
  }

  // Ensure the Chrome plugins are loaded, as they may intercept network
  // requests.  Does nothing if they are already loaded.
  // TODO(mpcomplete): This takes 200 ms!  Investigate parallelizing this by
  // starting the load earlier in a BG thread.
  plugin_service_->LoadChromePlugins(this);

  // Construct the event handler.
  scoped_refptr<ResourceHandler> handler;
  if (sync_result) {
    handler = new SyncResourceHandler(receiver_, request_data.url, sync_result);
  } else {
    handler = new AsyncResourceHandler(receiver_,
                                       process_id,
                                       route_id,
                                       receiver_->handle(),
                                       request_data.url,
                                       this);
  }

  if (HandleExternalProtocol(request_id, process_id, route_id,
                             request_data.url, request_data.resource_type,
                             handler)) {
    return;
  }

  // Construct the request.
  URLRequest* request = new URLRequest(request_data.url, this);
  if (entry && entry->attributes()) {
    request->SetUserData((void*)&Blacklist::kRequestDataKey,
                         new Blacklist::RequestData(entry));
  }
  request->set_method(request_data.method);
  request->set_first_party_for_cookies(request_data.first_party_for_cookies);

  if (!entry || !(entry->attributes() & Blacklist::kDontSendReferrer))
    request->set_referrer(request_data.referrer.spec());

  request->SetExtraRequestHeaders(request_data.headers);

  int load_flags = request_data.load_flags;
  // EV certificate verification could be expensive.  We don't want to spend
  // time performing EV certificate verification on all resources because
  // EV status is irrelevant to sub-frames and sub-resources.
  if (request_data.resource_type == ResourceType::MAIN_FRAME)
    load_flags |= net::LOAD_VERIFY_EV_CERT;
  request->set_load_flags(load_flags);
  request->set_context(context);
  request->set_origin_pid(request_data.origin_pid);

  if (IsHttpPrioritizationEnabled()) {
    // If the request is for the top level page or a frame/iframe, then we
    // should prioritize it higher than other resource types.  Currently, we
    // just use priorities 1 and 0.
    if (request_data.resource_type == ResourceType::MAIN_FRAME ||
        request_data.resource_type == ResourceType::SUB_FRAME) {
      request->set_priority(1);
    } else {
      request->set_priority(0);
    }
  }

  // Set upload data.
  uint64 upload_size = 0;
  if (request_data.upload_data) {
    request->set_upload(request_data.upload_data);
    upload_size = request_data.upload_data->GetContentLength();
  }

  // Install a CrossSiteResourceHandler if this request is coming from a
  // RenderViewHost with a pending cross-site request.  We only check this for
  // MAIN_FRAME requests.
  if (request_data.resource_type == ResourceType::MAIN_FRAME &&
      process_type == ChildProcessInfo::RENDER_PROCESS &&
      Singleton<CrossSiteRequestManager>::get()->
          HasPendingCrossSiteRequest(process_id, route_id)) {
    // Wrap the event handler to be sure the current page's onunload handler
    // has a chance to run before we render the new page.
    handler = new CrossSiteResourceHandler(handler,
                                           process_id,
                                           route_id,
                                           this);
  }

  if (safe_browsing_->enabled() &&
      safe_browsing_->CanCheckUrl(request_data.url)) {
    handler = new SafeBrowsingResourceHandler(handler,
                                              process_id,
                                              route_id,
                                              request_data.url,
                                              request_data.resource_type,
                                              safe_browsing_,
                                              this,
                                              receiver_);
  }

  // Insert a buffered event handler before the actual one.
  handler = new BufferedResourceHandler(handler, this, request);

  // Make extra info and read footer (contains request ID).
  ExtraRequestInfo* extra_info =
      new ExtraRequestInfo(handler,
                           process_type,
                           process_id,
                           route_id,
                           request_id,
                           request_data.frame_origin,
                           request_data.main_frame_origin,
                           request_data.resource_type,
                           upload_size);
  extra_info->allow_download =
      ResourceType::IsFrame(request_data.resource_type);
  SetExtraInfoForRequest(request, extra_info);  // request takes ownership

  BeginRequestInternal(request);
}

void ResourceDispatcherHost::OnDataReceivedACK(int request_id) {
  DataReceivedACK(receiver_->GetProcessId(), request_id);
}

void ResourceDispatcherHost::DataReceivedACK(int process_id, int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(process_id, request_id));
  if (i == pending_requests_.end())
    return;

  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);

  // Decrement the number of pending data messages.
  info->pending_data_count--;

  // If the pending data count was higher than the max, resume the request.
  if (info->pending_data_count == kMaxPendingDataMessages) {
    // Decrement the pending data count one more time because we also
    // incremented it before pausing the request.
    info->pending_data_count--;

    // Resume the request.
    PauseRequest(process_id, request_id, false);
  }
}

void ResourceDispatcherHost::OnUploadProgressACK(int request_id) {
  int process_id = receiver_->GetProcessId();
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(process_id, request_id));
  if (i == pending_requests_.end())
    return;

  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);
  info->waiting_for_upload_progress_ack = false;
}

void ResourceDispatcherHost::OnCancelRequest(int request_id) {
  CancelRequest(receiver_->GetProcessId(), request_id, true, true);
}

void ResourceDispatcherHost::OnClosePageACK(int new_render_process_host_id,
                                           int new_request_id) {
  GlobalRequestID global_id(new_render_process_host_id, new_request_id);
  PendingRequestList::iterator i = pending_requests_.find(global_id);
  if (i == pending_requests_.end()) {
    // If there are no matching pending requests, then this is not a
    // cross-site navigation and we are just closing the tab/browser.
    ui_loop_->PostTask(FROM_HERE, NewRunnableFunction(
        &RenderViewHost::ClosePageIgnoringUnloadEvents,
        new_render_process_host_id,
        new_request_id));
    return;
  }

  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);
  if (info->cross_site_handler) {
    info->cross_site_handler->ResumeResponse();
  }
}

// We are explicitly forcing the download of 'url'.
void ResourceDispatcherHost::BeginDownload(const GURL& url,
                                           const GURL& referrer,
                                           int process_id,
                                           int route_id,
                                           URLRequestContext* request_context) {
  if (is_shutdown_)
    return;

  // Check if the renderer is permitted to request the requested URL.
  if (!ChildProcessSecurityPolicy::GetInstance()->
          CanRequestURL(process_id, url)) {
    LOG(INFO) << "Denied unauthorized download request for " <<
        url.possibly_invalid_spec();
    return;
  }

  // Ensure the Chrome plugins are loaded, as they may intercept network
  // requests.  Does nothing if they are already loaded.
  plugin_service_->LoadChromePlugins(this);
  URLRequest* request = new URLRequest(url, this);

  request_id_--;

  scoped_refptr<ResourceHandler> handler =
      new DownloadResourceHandler(this,
                                  process_id,
                                  route_id,
                                  request_id_,
                                  url,
                                  download_file_manager_.get(),
                                  request,
                                  true);


  if (safe_browsing_->enabled() && safe_browsing_->CanCheckUrl(url)) {
    handler = new SafeBrowsingResourceHandler(handler,
                                              process_id,
                                              route_id,
                                              url,
                                              ResourceType::MAIN_FRAME,
                                              safe_browsing_,
                                              this,
                                              receiver_);
  }

  bool known_proto = URLRequest::IsHandledURL(url);
  if (!known_proto) {
    CHECK(false);
  }

  request->set_method("GET");
  request->set_referrer(referrer.spec());
  request->set_context(request_context);
  request->set_load_flags(request->load_flags() |
      net::LOAD_IS_DOWNLOAD);

  ExtraRequestInfo* extra_info =
      new ExtraRequestInfo(handler,
                           ChildProcessInfo::RENDER_PROCESS,
                           process_id,
                           route_id,
                           request_id_,
                           "null",  // frame_origin
                           "null",  // main_frame_origin
                           ResourceType::SUB_RESOURCE,
                           0 /* upload_size */);
  extra_info->allow_download = true;
  extra_info->is_download = true;
  SetExtraInfoForRequest(request, extra_info);  // request takes ownership

  BeginRequestInternal(request);
}

// This function is only used for saving feature.
void ResourceDispatcherHost::BeginSaveFile(const GURL& url,
                                           const GURL& referrer,
                                           int process_id,
                                           int route_id,
                                           URLRequestContext* request_context) {
  if (is_shutdown_)
    return;

  // Ensure the Chrome plugins are loaded, as they may intercept network
  // requests.  Does nothing if they are already loaded.
  plugin_service_->LoadChromePlugins(this);

  scoped_refptr<ResourceHandler> handler =
      new SaveFileResourceHandler(process_id,
                                  route_id,
                                  url,
                                  save_file_manager_.get());
  request_id_--;

  bool known_proto = URLRequest::IsHandledURL(url);
  if (!known_proto) {
    // Since any URLs which have non-standard scheme have been filtered
    // by save manager(see GURL::SchemeIsStandard). This situation
    // should not happen.
    NOTREACHED();
    return;
  }

  URLRequest* request = new URLRequest(url, this);
  request->set_method("GET");
  request->set_referrer(referrer.spec());
  // So far, for saving page, we need fetch content from cache, in the
  // future, maybe we can use a configuration to configure this behavior.
  request->set_load_flags(net::LOAD_ONLY_FROM_CACHE);
  request->set_context(request_context);

  ExtraRequestInfo* extra_info =
      new ExtraRequestInfo(handler,
                           ChildProcessInfo::RENDER_PROCESS,
                           process_id,
                           route_id,
                           request_id_,
                           "null",  // frame_origin
                           "null",  // main_frame_origin
                           ResourceType::SUB_RESOURCE,
                           0 /* upload_size */);
  // Just saving some resources we need, disallow downloading.
  extra_info->allow_download = false;
  extra_info->is_download = false;
  SetExtraInfoForRequest(request, extra_info);  // request takes ownership

  BeginRequestInternal(request);
}

void ResourceDispatcherHost::CancelRequest(int process_id,
                                           int request_id,
                                           bool from_renderer) {
  CancelRequest(process_id, request_id, from_renderer, true);
}

void ResourceDispatcherHost::CancelRequest(int process_id,
                                           int request_id,
                                           bool from_renderer,
                                           bool allow_delete) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(process_id, request_id));
  if (i == pending_requests_.end()) {
    // We probably want to remove this warning eventually, but I wanted to be
    // able to notice when this happens during initial development since it
    // should be rare and may indicate a bug.
    DLOG(WARNING) << "Canceling a request that wasn't found";
    return;
  }

  // WebKit will send us a cancel for downloads since it no longer handles them.
  // In this case, ignore the cancel since we handle downloads in the browser.
  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);
  if (!from_renderer || !info->is_download) {
    if (info->login_handler) {
      info->login_handler->OnRequestCancelled();
      info->login_handler = NULL;
    }
    if (!i->second->is_pending() && allow_delete) {
      // No io is pending, canceling the request won't notify us of anything,
      // so we explicitly remove it.
      // TODO(sky): removing the request in this manner means we're not
      // notifying anyone. We need make sure the event handlers and others are
      // notified so that everything is cleaned up properly.
      RemovePendingRequest(info->process_id, info->request_id);
    } else {
      i->second->Cancel();
    }
  }

  // Do not remove from the pending requests, as the request will still
  // call AllDataReceived, and may even have more data before it does
  // that.
}

bool ResourceDispatcherHost::WillSendData(int process_id,
                                          int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(process_id, request_id));
  if (i == pending_requests_.end()) {
    NOTREACHED() << L"WillSendData for invalid request";
    return false;
  }

  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);

  info->pending_data_count++;
  if (info->pending_data_count > kMaxPendingDataMessages) {
    // We reached the max number of data messages that can be sent to
    // the renderer for a given request. Pause the request and wait for
    // the renderer to start processing them before resuming it.
    PauseRequest(process_id, request_id, true);
    return false;
  }

  return true;
}

void ResourceDispatcherHost::PauseRequest(int process_id,
                                          int request_id,
                                          bool pause) {
  GlobalRequestID global_id(process_id, request_id);
  PendingRequestList::iterator i = pending_requests_.find(global_id);
  if (i == pending_requests_.end()) {
    DLOG(WARNING) << "Pausing a request that wasn't found";
    return;
  }

  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);

  int pause_count = info->pause_count + (pause ? 1 : -1);
  if (pause_count < 0) {
    NOTREACHED();  // Unbalanced call to pause.
    return;
  }
  info->pause_count = pause_count;

  RESOURCE_LOG("To pause (" << pause << "): " << i->second->url().spec());

  // If we're resuming, kick the request to start reading again. Run the read
  // asynchronously to avoid recursion problems.
  if (info->pause_count == 0) {
    MessageLoop::current()->PostTask(FROM_HERE,
        method_runner_.NewRunnableMethod(
            &ResourceDispatcherHost::ResumeRequest, global_id));
  }
}

int ResourceDispatcherHost::GetOutstandingRequestsMemoryCost(
    int process_id) const {
  OutstandingRequestsMemoryCostMap::const_iterator entry =
      outstanding_requests_memory_cost_map_.find(process_id);
  return (entry == outstanding_requests_memory_cost_map_.end()) ?
      0 : entry->second;
}

// The object died, so cancel and detach all requests associated with it except
// for downloads, which belong to the browser process even if initiated via a
// renderer.
void ResourceDispatcherHost::CancelRequestsForProcess(int process_id) {
  CancelRequestsForRoute(process_id, -1 /* cancel all */);
}

void ResourceDispatcherHost::CancelRequestsForRoute(
    int process_id,
    int route_id) {
  // Since pending_requests_ is a map, we first build up a list of all of the
  // matching requests to be cancelled, and then we cancel them.  Since there
  // may be more than one request to cancel, we cannot simply hold onto the map
  // iterators found in the first loop.

  // Find the global ID of all matching elements.
  std::vector<GlobalRequestID> matching_requests;
  for (PendingRequestList::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->first.process_id == process_id) {
      ExtraRequestInfo* info = ExtraInfoForRequest(i->second);
      if (!info->is_download && (route_id == -1 ||
                                 route_id == info->route_id)) {
        matching_requests.push_back(
            GlobalRequestID(process_id, i->first.request_id));
      }
    }
  }

  // Remove matches.
  for (size_t i = 0; i < matching_requests.size(); ++i) {
    PendingRequestList::iterator iter =
        pending_requests_.find(matching_requests[i]);
    // Although every matching request was in pending_requests_ when we built
    // matching_requests, it is normal for a matching request to be not found
    // in pending_requests_ after we have removed some matching requests from
    // pending_requests_.  For example, deleting a URLRequest that has
    // exclusive (write) access to an HTTP cache entry may unblock another
    // URLRequest that needs exclusive access to the same cache entry, and
    // that URLRequest may complete and remove itself from pending_requests_.
    // So we need to check that iter is not equal to pending_requests_.end().
    if (iter != pending_requests_.end())
      RemovePendingRequest(iter);
  }

  // Now deal with blocked requests if any.
  if (route_id != -1) {
    if (blocked_requests_map_.find(std::pair<int, int>(process_id, route_id)) !=
        blocked_requests_map_.end()) {
      CancelBlockedRequestsForRoute(process_id, route_id);
    }
  } else {
    // We have to do all render views for the process |process_id|.
    // Note that we have to do this in 2 passes as we cannot call
    // CancelBlockedRequestsForRoute while iterating over
    // blocked_requests_map_, as it modifies it.
    std::set<int> route_ids;
    for (BlockedRequestMap::const_iterator iter = blocked_requests_map_.begin();
         iter != blocked_requests_map_.end(); ++iter) {
      if (iter->first.first == process_id)
        route_ids.insert(iter->first.second);
    }
    for (std::set<int>::const_iterator iter = route_ids.begin();
        iter != route_ids.end(); ++iter) {
      CancelBlockedRequestsForRoute(process_id, *iter);
    }
  }
}

// Cancels the request and removes it from the list.
void ResourceDispatcherHost::RemovePendingRequest(int process_id,
                                                  int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(process_id, request_id));
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Trying to remove a request that's not here";
    return;
  }
  RemovePendingRequest(i);
}

void ResourceDispatcherHost::RemovePendingRequest(
    const PendingRequestList::iterator& iter) {
  ExtraRequestInfo* info = ExtraInfoForRequest(iter->second);

  // Remove the memory credit that we added when pushing the request onto
  // the pending list.
  IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost,
                                         info->process_id);

  // Notify the login handler that this request object is going away.
  if (info && info->login_handler)
    info->login_handler->OnRequestCancelled();

  delete iter->second;
  pending_requests_.erase(iter);

  // If we have no more pending requests, then stop the load state monitor
  if (pending_requests_.empty())
    update_load_states_timer_.Stop();
}

// URLRequest::Delegate -------------------------------------------------------

void ResourceDispatcherHost::OnReceivedRedirect(URLRequest* request,
                                                const GURL& new_url) {
  RESOURCE_LOG("OnReceivedRedirect: " << request->url().spec());
  ExtraRequestInfo* info = ExtraInfoForRequest(request);

  DCHECK(request->status().is_success());

  if (info->process_type != ChildProcessInfo::PLUGIN_PROCESS &&
      !ChildProcessSecurityPolicy::GetInstance()->
          CanRequestURL(info->process_id, new_url)) {
    LOG(INFO) << "Denied unauthorized request for " <<
        new_url.possibly_invalid_spec();

    // Tell the renderer that this request was disallowed.
    CancelRequest(info->process_id, info->request_id, false);
    return;
  }

  NofityReceivedRedirect(request, info->process_id, new_url);

  if (HandleExternalProtocol(info->request_id, info->process_id,
                             info->route_id, new_url,
                             info->resource_type, info->resource_handler)) {
    // The request is complete so we can remove it.
    RemovePendingRequest(info->process_id, info->request_id);
    return;
  }

  if (!info->resource_handler->OnRequestRedirected(info->request_id, new_url))
    CancelRequest(info->process_id, info->request_id, false);
}

void ResourceDispatcherHost::OnAuthRequired(
    URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  // Create a login dialog on the UI thread to get authentication data,
  // or pull from cache and continue on the IO thread.
  // TODO(mpcomplete): We should block the parent tab while waiting for
  // authentication.
  // That would also solve the problem of the URLRequest being cancelled
  // before we receive authentication.
  ExtraRequestInfo* info = ExtraInfoForRequest(request);
  DCHECK(!info->login_handler) <<
      "OnAuthRequired called with login_handler pending";
  info->login_handler = CreateLoginPrompt(auth_info, request, ui_loop_);
}

void ResourceDispatcherHost::OnCertificateRequested(
      URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) {
  DCHECK(request);

  bool select_first_cert = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAutoSSLClientAuth);
  net::X509Certificate* cert =
      select_first_cert && !cert_request_info->client_certs.empty() ?
      cert_request_info->client_certs[0] : NULL;
  request->ContinueWithCertificate(cert);
}

void ResourceDispatcherHost::OnSSLCertificateError(
    URLRequest* request,
    int cert_error,
    net::X509Certificate* cert) {
  DCHECK(request);
  SSLManager::OnSSLCertificateError(this, request, cert_error, cert, ui_loop_);
}

void ResourceDispatcherHost::OnResponseStarted(URLRequest* request) {
  RESOURCE_LOG("OnResponseStarted: " << request->url().spec());
  ExtraRequestInfo* info = ExtraInfoForRequest(request);
  if (PauseRequestIfNeeded(info)) {
    RESOURCE_LOG("OnResponseStarted pausing: " << request->url().spec());
    return;
  }

  if (request->status().is_success()) {
    // We want to send a final upload progress message prior to sending
    // the response complete message even if we're waiting for an ack to
    // to a previous upload progress message.
    info->waiting_for_upload_progress_ack = false;
    MaybeUpdateUploadProgress(info, request);

    if (!CompleteResponseStarted(request)) {
      CancelRequest(info->process_id, info->request_id, false);
    } else {
      // Start reading.
      int bytes_read = 0;
      if (Read(request, &bytes_read)) {
        OnReadCompleted(request, bytes_read);
      } else if (!request->status().is_io_pending()) {
        DCHECK(!info->is_paused);
        // If the error is not an IO pending, then we're done reading.
        OnResponseCompleted(request);
      }
    }
  } else {
    OnResponseCompleted(request);
  }
}

bool ResourceDispatcherHost::CompleteResponseStarted(URLRequest* request) {
  ExtraRequestInfo* info = ExtraInfoForRequest(request);

  scoped_refptr<ResourceResponse> response(new ResourceResponse);

  response->response_head.status = request->status();
  response->response_head.request_time = request->request_time();
  response->response_head.response_time = request->response_time();
  response->response_head.headers = request->response_headers();
  request->GetCharset(&response->response_head.charset);
  response->response_head.filter_policy = info->filter_policy;
  response->response_head.content_length = request->GetExpectedContentSize();
  response->response_head.app_cache_id = WebAppCacheContext::kNoAppCacheId;
  request->GetMimeType(&response->response_head.mime_type);

  if (request->ssl_info().cert) {
    int cert_id =
        CertStore::GetSharedInstance()->StoreCert(
            request->ssl_info().cert,
            info->process_id);
    response->response_head.security_info =
        SSLManager::SerializeSecurityInfo(cert_id,
                                          request->ssl_info().cert_status,
                                          request->ssl_info().security_bits);
  } else {
    // We should not have any SSL state.
    DCHECK(!request->ssl_info().cert_status &&
           (request->ssl_info().security_bits == -1 ||
           request->ssl_info().security_bits == 0));
  }

  NotifyResponseStarted(request, info->process_id);
  return info->resource_handler->OnResponseStarted(info->request_id,
                                                   response.get());
}

int ResourceDispatcherHost::IncrementOutstandingRequestsMemoryCost(
    int cost, int process_id) {
  // Retrieve the previous value (defaulting to 0 if not found).
  OutstandingRequestsMemoryCostMap::iterator prev_entry =
      outstanding_requests_memory_cost_map_.find(process_id);
  int new_cost = 0;
  if (prev_entry != outstanding_requests_memory_cost_map_.end())
    new_cost = prev_entry->second;

  // Insert/update the total; delete entries when their value reaches 0.
  new_cost += cost;
  CHECK(new_cost >= 0);
  if (new_cost == 0)
    outstanding_requests_memory_cost_map_.erase(prev_entry);
  else
    outstanding_requests_memory_cost_map_[process_id] = new_cost;

  return new_cost;
}

// static
int ResourceDispatcherHost::CalculateApproximateMemoryCost(
    URLRequest* request) {
  // The following fields should be a minor size contribution (experimentally
  // on the order of 100). However since they are variable length, it could
  // in theory be a sizeable contribution.
  int strings_cost = request->extra_request_headers().size() +
                     request->original_url().spec().size() +
                     request->referrer().size() +
                     request->method().size();

  int upload_cost = 0;

  // TODO(eroman): don't enable the upload throttling until we have data
  // showing what a reasonable limit is (limiting to 25MB of uploads may
  // be too restrictive).
#if 0
  // Sum all the (non-file) upload data attached to the request, if any.
  if (request->has_upload()) {
    const std::vector<net::UploadData::Element>& uploads =
        request->get_upload()->elements();
    std::vector<net::UploadData::Element>::const_iterator iter;
    for (iter = uploads.begin(); iter != uploads.end(); ++iter) {
      if (iter->type() == net::UploadData::TYPE_BYTES) {
        int64 element_size = iter->GetContentLength();
        // This cast should not result in truncation.
        upload_cost += static_cast<int>(element_size);
      }
    }
  }
#endif

  // Note that this expression will typically be dominated by:
  // |kAvgBytesPerOutstandingRequest|.
  return kAvgBytesPerOutstandingRequest + strings_cost + upload_cost;
}

void ResourceDispatcherHost::BeginRequestInternal(URLRequest* request) {
  DCHECK(!request->is_pending());
  ExtraRequestInfo* info = ExtraInfoForRequest(request);

  // Add the memory estimate that starting this request will consume.
  info->memory_cost = CalculateApproximateMemoryCost(request);
  int memory_cost = IncrementOutstandingRequestsMemoryCost(
      info->memory_cost,
      info->process_id);

  // If enqueing/starting this request will exceed our per-process memory
  // bound, abort it right away.
  if (memory_cost > max_outstanding_requests_cost_per_process_) {
    // We call "SimulateError()" as a way of setting the URLRequest's
    // status -- it has no effect beyond this, since the request hasn't started.
    request->SimulateError(net::ERR_INSUFFICIENT_RESOURCES);

    // TODO(eroman): this is kinda funky -- we insert the unstarted request into
    // |pending_requests_| simply to please OnResponseCompleted().
    GlobalRequestID global_id(info->process_id, info->request_id);
    pending_requests_[global_id] = request;
    OnResponseCompleted(request);
    return;
  }

  std::pair<int, int> pair_id(info->process_id, info->route_id);
  BlockedRequestMap::const_iterator iter = blocked_requests_map_.find(pair_id);
  if (iter != blocked_requests_map_.end()) {
    // The request should be blocked.
    iter->second->push_back(request);
    return;
  }

  GlobalRequestID global_id(info->process_id, info->request_id);
  pending_requests_[global_id] = request;
  if (!SSLManager::ShouldStartRequest(this, request, ui_loop_)) {
    // The SSLManager has told us that we shouldn't start the request yet.  The
    // SSLManager will potentially change the request (potentially to indicate
    // its content should be filtered) and start it itself.
    return;
  }
  request->Start();

  // Make sure we have the load state monitor running
  if (!update_load_states_timer_.IsRunning()) {
    update_load_states_timer_.Start(
        TimeDelta::FromMilliseconds(kUpdateLoadStatesIntervalMsec),
        this, &ResourceDispatcherHost::UpdateLoadStates);
  }
}

// This test mirrors the decision that WebKit makes in
// WebFrameLoaderClient::dispatchDecidePolicyForMIMEType.
// static.
bool ResourceDispatcherHost::ShouldDownload(
    const std::string& mime_type, const std::string& content_disposition) {
  std::string type = StringToLowerASCII(mime_type);
  std::string disposition = StringToLowerASCII(content_disposition);

  // First, examine content-disposition.
  if (!disposition.empty()) {
    bool should_download = true;

    // Some broken sites just send ...
    //    Content-Disposition: ; filename="file"
    // ... screen those out here.
    if (disposition[0] == ';')
      should_download = false;

    if (disposition.compare(0, 6, "inline") == 0)
      should_download = false;

    // Some broken sites just send ...
    //    Content-Disposition: filename="file"
    // ... without a disposition token... Screen those out.
    if (disposition.compare(0, 8, "filename") == 0)
      should_download = false;

    // Also in use is Content-Disposition: name="file"
    if (disposition.compare(0, 4, "name") == 0)
      should_download = false;

    // We have a content-disposition of "attachment" or unknown.
    // RFC 2183, section 2.8 says that an unknown disposition
    // value should be treated as "attachment".
    if (should_download)
      return true;
  }

  // MIME type checking.
  if (net::IsSupportedMimeType(type))
    return false;

  // Finally, check the plugin service.
  bool allow_wildcard = false;
  return !plugin_service_->HavePluginFor(type, allow_wildcard);
}

bool ResourceDispatcherHost::PauseRequestIfNeeded(ExtraRequestInfo* info) {
  if (info->pause_count > 0)
    info->is_paused = true;

  return info->is_paused;
}

void ResourceDispatcherHost::ResumeRequest(const GlobalRequestID& request_id) {
  PendingRequestList::iterator i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())  // The request may have been destroyed
    return;

  URLRequest* request = i->second;
  ExtraRequestInfo* info = ExtraInfoForRequest(request);
  if (!info->is_paused)
    return;

  RESOURCE_LOG("Resuming: " << i->second->url().spec());

  info->is_paused = false;

  if (info->has_started_reading)
    OnReadCompleted(i->second, info->paused_read_bytes);
  else
    OnResponseStarted(i->second);
}

bool ResourceDispatcherHost::Read(URLRequest* request, int* bytes_read) {
  ExtraRequestInfo* info = ExtraInfoForRequest(request);
  DCHECK(!info->is_paused);

  net::IOBuffer* buf;
  int buf_size;
  if (!info->resource_handler->OnWillRead(info->request_id,
                                          &buf, &buf_size, -1)) {
    return false;
  }

  DCHECK(buf);
  DCHECK(buf_size > 0);

  info->has_started_reading = true;
  return request->Read(buf, buf_size, bytes_read);
}

void ResourceDispatcherHost::OnReadCompleted(URLRequest* request,
                                             int bytes_read) {
  DCHECK(request);
  RESOURCE_LOG("OnReadCompleted: " << request->url().spec());
  ExtraRequestInfo* info = ExtraInfoForRequest(request);
  if (PauseRequestIfNeeded(info)) {
    info->paused_read_bytes = bytes_read;
    RESOURCE_LOG("OnReadCompleted pausing: " << request->url().spec());
    return;
  }

  if (request->status().is_success() && CompleteRead(request, &bytes_read)) {
    // The request can be paused if we realize that the renderer is not
    // servicing messages fast enough.
    if (info->pause_count == 0 &&
        Read(request, &bytes_read) &&
        request->status().is_success()) {
      if (bytes_read == 0) {
        CompleteRead(request, &bytes_read);
      } else {
        // Force the next CompleteRead / Read pair to run as a separate task.
        // This avoids a fast, large network request from monopolizing the IO
        // thread and starving other IO operations from running.
        info->paused_read_bytes = bytes_read;
        info->is_paused = true;
        GlobalRequestID id(info->process_id, info->request_id);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_runner_.NewRunnableMethod(
                &ResourceDispatcherHost::ResumeRequest, id));
        return;
      }
    }
  }

  if (PauseRequestIfNeeded(info)) {
    info->paused_read_bytes = bytes_read;
    RESOURCE_LOG("OnReadCompleted (CompleteRead) pausing: " <<
                 request->url().spec());
    return;
  }

  // If the status is not IO pending then we've either finished (success) or we
  // had an error.  Either way, we're done!
  if (!request->status().is_io_pending())
    OnResponseCompleted(request);
}

bool ResourceDispatcherHost::CompleteRead(URLRequest* request,
                                          int* bytes_read) {
  if (!request->status().is_success()) {
    NOTREACHED();
    return false;
  }

  ExtraRequestInfo* info = ExtraInfoForRequest(request);

  if (!info->resource_handler->OnReadCompleted(info->request_id, bytes_read)) {
    // Pass in false as the last arg to indicate we don't want |request|
    // deleted. We do this as callers of us assume |request| is valid after we
    // return.
    CancelRequest(info->process_id, info->request_id, false, false);
    return false;
  }

  return *bytes_read != 0;
}

void ResourceDispatcherHost::OnResponseCompleted(URLRequest* request) {
  RESOURCE_LOG("OnResponseCompleted: " << request->url().spec());
  ExtraRequestInfo* info = ExtraInfoForRequest(request);

  std::string security_info;
  const net::SSLInfo& ssl_info = request->ssl_info();
  if (ssl_info.cert != NULL) {
    int cert_id = CertStore::GetSharedInstance()->
        StoreCert(ssl_info.cert, info->process_id);
    security_info = SSLManager::SerializeSecurityInfo(cert_id,
                                                      ssl_info.cert_status,
                                                      ssl_info.security_bits);
  }

  if (info->resource_handler->OnResponseCompleted(info->request_id,
                                                  request->status(),
                                                  security_info)) {
    NotifyResponseCompleted(request, info->process_id);

    // The request is complete so we can remove it.
    RemovePendingRequest(info->process_id, info->request_id);
  }
  // If the handler's OnResponseCompleted returns false, we are deferring the
  // call until later.  We will notify the world and clean up when we resume.
}

void ResourceDispatcherHost::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void ResourceDispatcherHost::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

URLRequest* ResourceDispatcherHost::GetURLRequest(
    GlobalRequestID request_id) const {
  // This should be running in the IO loop. io_loop_ can be NULL during the
  // unit_tests.
  DCHECK(MessageLoop::current() == io_loop_ && io_loop_);

  PendingRequestList::const_iterator i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())
    return NULL;

  return i->second;
}

// A NotificationTask proxies a resource dispatcher notification from the IO
// thread to the UI thread.  It should be constructed on the IO thread and run
// in the UI thread.  Takes ownership of |details|.
class NotificationTask : public Task {
 public:
  NotificationTask(NotificationType type,
                   URLRequest* request,
                   ResourceRequestDetails* details)
  : type_(type),
    details_(details) {
    if (!tab_util::GetTabContentsID(request, &process_id_, &tab_contents_id_))
      NOTREACHED();
  }

  void Run() {
    // Find the tab associated with this request.
    TabContents* tab_contents =
        tab_util::GetTabContentsByID(process_id_, tab_contents_id_);

    if (tab_contents) {
      // Issue the notification.
      NotificationService::current()->Notify(
          type_,
          Source<NavigationController>(&tab_contents->controller()),
          Details<ResourceRequestDetails>(details_.get()));
    }
  }

 private:
  // These IDs let us find the correct tab on the UI thread.
  int process_id_;
  int tab_contents_id_;

  // The type and details of the notification.
  NotificationType type_;
  scoped_ptr<ResourceRequestDetails> details_;
};

static int GetCertID(URLRequest* request, int process_id) {
  if (request->ssl_info().cert) {
    return CertStore::GetSharedInstance()->StoreCert(request->ssl_info().cert,
                                                     process_id);
  }
  // If there is no SSL info attached to this request, we must either be a non
  // secure request, or the request has been canceled or failed (before the SSL
  // info was populated), or the response is an error (we have seen 403, 404,
  // and 501) made up by the proxy.
  DCHECK(!request->url().SchemeIsSecure() ||
         (request->status().status() == URLRequestStatus::CANCELED) ||
         (request->status().status() == URLRequestStatus::FAILED) ||
         ((request->response_headers()->response_code() >= 400) &&
         (request->response_headers()->response_code() <= 599)));
  return 0;
}

void ResourceDispatcherHost::NotifyResponseStarted(URLRequest* request,
                                                   int process_id) {
  // Notify the observers on the IO thread.
  FOR_EACH_OBSERVER(Observer, observer_list_, OnRequestStarted(this, request));

  // Notify the observers on the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      new NotificationTask(NotificationType::RESOURCE_RESPONSE_STARTED, request,
                           new ResourceRequestDetails(request,
                               GetCertID(request, process_id))));
}

void ResourceDispatcherHost::NotifyResponseCompleted(
    URLRequest* request,
    int process_id) {
  // Notify the observers on the IO thread.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnResponseCompleted(this, request));

  // Notify the observers on the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      new NotificationTask(NotificationType::RESOURCE_RESPONSE_COMPLETED,
                           request,
                           new ResourceRequestDetails(request,
                               GetCertID(request, process_id))));
}

void ResourceDispatcherHost::NofityReceivedRedirect(URLRequest* request,
                                                    int process_id,
                                                    const GURL& new_url) {
  // Notify the observers on the IO thread.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnReceivedRedirect(this, request, new_url));

  int cert_id = GetCertID(request, process_id);

  // Notify the observers on the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      new NotificationTask(NotificationType::RESOURCE_RECEIVED_REDIRECT,
                           request,
                           new ResourceRedirectDetails(request,
                                                       cert_id,
                                                       new_url)));
}

namespace {

// This function attempts to return the "more interesting" load state of |a|
// and |b|.  We don't have temporal information about these load states
// (meaning we don't know when we transitioned into these states), so we just
// rank them according to how "interesting" the states are.
//
// We take advantage of the fact that the load states are an enumeration listed
// in the order in which they occur during the lifetime of a request, so we can
// regard states with larger numeric values as being further along toward
// completion.  We regard those states as more interesting to report since they
// represent progress.
//
// For example, by this measure "tranferring data" is a more interesting state
// than "resolving host" because when we are transferring data we are actually
// doing something that corresponds to changes that the user might observe,
// whereas waiting for a host name to resolve implies being stuck.
//
net::LoadState MoreInterestingLoadState(net::LoadState a, net::LoadState b) {
  return (a < b) ? b : a;
}

// Carries information about a load state change.
struct LoadInfo {
  GURL url;
  net::LoadState load_state;
};

// Map from ProcessID+ViewID pair to LoadState
typedef std::map<std::pair<int, int>, LoadInfo> LoadInfoMap;

// Used to marshall calls to LoadStateChanged from the IO to UI threads.  We do
// them all as a single task to avoid spamming the UI thread.
class LoadInfoUpdateTask : public Task {
 public:
  virtual void Run() {
    LoadInfoMap::const_iterator i;
    for (i = info_map.begin(); i != info_map.end(); ++i) {
      RenderViewHost* view =
          RenderViewHost::FromID(i->first.first, i->first.second);
      if (view)  // The view could be gone at this point.
        view->LoadStateChanged(i->second.url, i->second.load_state);
    }
  }
  LoadInfoMap info_map;
};

}  // namespace

void ResourceDispatcherHost::UpdateLoadStates() {
  // Populate this map with load state changes, and then send them on to the UI
  // thread where they can be passed along to the respective RVHs.
  LoadInfoMap info_map;

  PendingRequestList::const_iterator i;
  for (i = pending_requests_.begin(); i != pending_requests_.end(); ++i) {
    URLRequest* request = i->second;
    net::LoadState load_state = request->GetLoadState();
    ExtraRequestInfo* info = ExtraInfoForRequest(request);

    // We also poll for upload progress on this timer and send upload
    // progress ipc messages to the plugin process.
    MaybeUpdateUploadProgress(info, request);

    if (info->last_load_state != load_state) {
      info->last_load_state = load_state;

      std::pair<int, int> key(info->process_id, info->route_id);
      net::LoadState to_insert;
      LoadInfoMap::iterator existing = info_map.find(key);
      if (existing == info_map.end()) {
        to_insert = load_state;
      } else {
        to_insert =
            MoreInterestingLoadState(existing->second.load_state, load_state);
        if (to_insert == existing->second.load_state)
          continue;
      }
      LoadInfo& load_info = info_map[key];
      load_info.url = request->url();
      load_info.load_state = to_insert;
    }
  }

  if (info_map.empty())
    return;

  LoadInfoUpdateTask* task = new LoadInfoUpdateTask;
  task->info_map.swap(info_map);
  ui_loop_->PostTask(FROM_HERE, task);
}

void ResourceDispatcherHost::MaybeUpdateUploadProgress(ExtraRequestInfo *info,
                                                       URLRequest *request) {
  if (!info->upload_size || info->waiting_for_upload_progress_ack ||
      !(request->load_flags() & net::LOAD_ENABLE_UPLOAD_PROGRESS))
    return;

  uint64 size = info->upload_size;
  uint64 position = request->GetUploadProgress();
  if (position == info->last_upload_position)
    return;  // no progress made since last time

  const uint64 kHalfPercentIncrements = 200;
  const TimeDelta kOneSecond = TimeDelta::FromMilliseconds(1000);

  uint64 amt_since_last = position - info->last_upload_position;
  TimeDelta time_since_last = TimeTicks::Now() - info->last_upload_ticks;

  bool is_finished = (size == position);
  bool enough_new_progress = (amt_since_last > (size / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    info->resource_handler->OnUploadProgress(info->request_id, position, size);
    info->waiting_for_upload_progress_ack = true;
    info->last_upload_ticks = TimeTicks::Now();
    info->last_upload_position = position;
  }
}

void ResourceDispatcherHost::BlockRequestsForRoute(
    int process_id,
    int route_id) {
  std::pair<int, int> key(process_id, route_id);
  DCHECK(blocked_requests_map_.find(key) == blocked_requests_map_.end()) <<
      "BlockRequestsForRoute called  multiple time for the same RVH";
  blocked_requests_map_[key] = new BlockedRequestsList();
}

void ResourceDispatcherHost::ResumeBlockedRequestsForRoute(
    int process_id,
    int route_id) {
  ProcessBlockedRequestsForRoute(process_id, route_id, false);
}

void ResourceDispatcherHost::CancelBlockedRequestsForRoute(
    int process_id,
    int route_id) {
  ProcessBlockedRequestsForRoute(process_id, route_id, true);
}

void ResourceDispatcherHost::ProcessBlockedRequestsForRoute(
    int process_id,
    int route_id,
    bool cancel_requests) {
  BlockedRequestMap::iterator iter =
      blocked_requests_map_.find(std::pair<int, int>(process_id, route_id));
  if (iter == blocked_requests_map_.end()) {
    // It's possible to reach here if the renderer crashed while an interstitial
    // page was showing.
    return;
  }

  BlockedRequestsList* requests = iter->second;

  // Removing the vector from the map unblocks any subsequent requests.
  blocked_requests_map_.erase(iter);

  for (BlockedRequestsList::iterator req_iter = requests->begin();
       req_iter != requests->end(); ++req_iter) {
    // Remove the memory credit that we added when pushing the request onto
    // the blocked list.
    URLRequest* request = *req_iter;
    ExtraRequestInfo* info = ExtraInfoForRequest(request);
    IncrementOutstandingRequestsMemoryCost(-1 * info->memory_cost,
                                           info->process_id);
    if (cancel_requests)
      delete request;
    else
      BeginRequestInternal(request);
  }

  delete requests;
}

bool ResourceDispatcherHost::IsResourceDispatcherHostMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case ViewHostMsg_RequestResource::ID:
    case ViewHostMsg_CancelRequest::ID:
    case ViewHostMsg_ClosePage_ACK::ID:
    case ViewHostMsg_DataReceived_ACK::ID:
    case ViewHostMsg_DownloadProgress_ACK::ID:
    case ViewHostMsg_UploadProgress_ACK::ID:
    case ViewHostMsg_SyncLoad::ID:
      return true;

    default:
      break;
  }

  return false;
}
