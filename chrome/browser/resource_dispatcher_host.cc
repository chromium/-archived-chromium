// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include <vector>

#include "chrome/browser/resource_dispatcher_host.h"

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/cross_site_request_manager.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_request_manager.h"
#include "chrome/browser/download/save_file_manager.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/login_prompt.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/renderer_security_policy.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/render_view_host_delegate.h"
#include "chrome/browser/resource_request_details.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/browser/tab_util.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/stl_util-inl.h"
#include "net/base/auth.h"
#include "net/base/cert_status_flags.h"
#include "net/base/load_flags.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"

// Uncomment to enable logging of request traffic.
//#define LOG_RESOURCE_DISPATCHER_REQUESTS

#ifdef LOG_RESOURCE_DISPATCHER_REQUESTS
# define RESOURCE_LOG(stuff) LOG(INFO) << stuff
#else
# define RESOURCE_LOG(stuff)
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// ----------------------------------------------------------------------------

// The interval for calls to ResourceDispatcherHost::UpdateLoadStates
static const int kUpdateLoadStatesIntervalMsec = 100;

// Maximum number of pending data messages sent to the renderer at any
// given time for a given request.
static const int kMaxPendingDataMessages = 20;

// Maximum time to wait for a gethash response from the Safe Browsing servers.
static const int kMaxGetHashMs = 1000;

// ----------------------------------------------------------------------------
// ResourceDispatcherHost::Response

struct ResourceDispatcherHost::Response : public base::RefCounted<Response> {
  ViewMsg_Resource_ResponseHead response_head;
};

// ----------------------------------------------------------------------------
// ResourceDispatcherHost::AsyncEventHandler

// Used to complete an asynchronous resource request in response to resource
// load events from the resource dispatcher host.
class ResourceDispatcherHost::AsyncEventHandler
    : public ResourceDispatcherHost::EventHandler {
 public:
  AsyncEventHandler(ResourceDispatcherHost::Receiver* receiver,
                    int render_process_host_id,
                    int routing_id,
                    HANDLE render_process,
                    const GURL& url,
                    ResourceDispatcherHost* resource_dispatcher_host)
      : receiver_(receiver),
        render_process_host_id_(render_process_host_id),
        routing_id_(routing_id),
        render_process_(render_process),
        rdh_(resource_dispatcher_host) { }

  static void GlobalCleanup() {
    delete spare_read_buffer_;
    spare_read_buffer_ = NULL;
  }

  bool OnUploadProgress(int request_id, uint64 position, uint64 size) {
    return receiver_->Send(new ViewMsg_Resource_UploadProgress(
        routing_id_, request_id, position, size));
  }

  bool OnRequestRedirected(int request_id, const GURL& new_url) {
    return receiver_->Send(new ViewMsg_Resource_ReceivedRedirect(
        routing_id_, request_id, new_url));
  }

  bool OnResponseStarted(int request_id, Response* response) {
    receiver_->Send(new ViewMsg_Resource_ReceivedResponse(
        routing_id_, request_id, response->response_head));
    return true;
  }

  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size) {
    DCHECK(min_size == -1);
    static const int kReadBufSize = 32768;
    if (spare_read_buffer_) {
      read_buffer_.reset(spare_read_buffer_);
      spare_read_buffer_ = NULL;
    } else {
      read_buffer_.reset(new base::SharedMemory);
      if (!read_buffer_->Create(std::wstring(), false, false, kReadBufSize))
        return false;
      if (!read_buffer_->Map(kReadBufSize))
        return false;
    }
    *buf = static_cast<char*>(read_buffer_->memory());
    *buf_size = kReadBufSize;
    return true;
  }

  bool OnReadCompleted(int request_id, int* bytes_read) {
    if (!*bytes_read)
      return true;
    DCHECK(read_buffer_.get());

    if (!rdh_->WillSendData(render_process_host_id_, request_id)) {
      // We should not send this data now, we have too many pending requests.
      return true;
    }

    base::SharedMemoryHandle handle;
    if (!read_buffer_->GiveToProcess(render_process_, &handle)) {
      // We wrongfully incremented the pending data count. Fake an ACK message
      // to fix this. We can't move this call above the WillSendData because
      // it's killing our read_buffer_, and we don't want that when we pause
      // the request.
      rdh_->OnDataReceivedACK(render_process_host_id_, request_id);
      return false;
    }

    receiver_->Send(new ViewMsg_Resource_DataReceived(
        routing_id_, request_id, handle, *bytes_read));

    return true;
  }

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status) {
    receiver_->Send(new ViewMsg_Resource_RequestComplete(
        routing_id_, request_id, status));

    // If we still have a read buffer, then see about caching it for later...
    if (spare_read_buffer_) {
      read_buffer_.reset();
    } else if (read_buffer_.get() && read_buffer_->memory()) {
      spare_read_buffer_ = read_buffer_.release();
    }
    return true;
  }

 private:
  // When reading, we don't know if we are going to get EOF (0 bytes read), so
  // we typically have a buffer that we allocated but did not use.  We keep
  // this buffer around for the next read as a small optimization.
  static base::SharedMemory* spare_read_buffer_;

  scoped_ptr<base::SharedMemory> read_buffer_;
  ResourceDispatcherHost::Receiver* receiver_;
  int render_process_host_id_;
  int routing_id_;
  HANDLE render_process_;
  ResourceDispatcherHost* rdh_;
};

base::SharedMemory*
    ResourceDispatcherHost::AsyncEventHandler::spare_read_buffer_;

// ----------------------------------------------------------------------------
// ResourceDispatcherHost::SyncEventHandler

// Used to complete a synchronous resource request in response to resource load
// events from the resource dispatcher host.
class ResourceDispatcherHost::SyncEventHandler
    : public ResourceDispatcherHost::EventHandler {
 public:
  SyncEventHandler(ResourceDispatcherHost::Receiver* receiver,
                   const GURL& url,
                   IPC::Message* result_message)
      : receiver_(receiver),
        result_message_(result_message) {
    result_.final_url = url;
    result_.filter_policy = FilterPolicy::DONT_FILTER;
  }

  bool OnRequestRedirected(int request_id, const GURL& new_url) {
    result_.final_url = new_url;
    return true;
  }

  bool OnResponseStarted(int request_id, Response* response) {
    // We don't care about copying the status here.
    result_.headers = response->response_head.headers;
    result_.mime_type = response->response_head.mime_type;
    result_.charset = response->response_head.charset;
    return true;
  }

  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size) {
    DCHECK(min_size == -1);
    *buf = read_buffer_;
    *buf_size = kReadBufSize;
    return true;
  }

  bool OnReadCompleted(int request_id, int* bytes_read) {
    if (!*bytes_read)
      return true;
    result_.data.append(read_buffer_, *bytes_read);
    return true;
  }

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status) {
    result_.status = status;

    ViewHostMsg_SyncLoad::WriteReplyParams(result_message_, result_);
    receiver_->Send(result_message_);
    return true;
  }

 private:
  enum { kReadBufSize = 3840 };
  char read_buffer_[kReadBufSize];

  ViewHostMsg_SyncLoad_Result result_;
  ResourceDispatcherHost::Receiver* receiver_;
  IPC::Message* result_message_;
};

// ----------------------------------------------------------------------------
// ResourceDispatcherHost::DownloadEventHandler
// Forwards data to the download thread.

class ResourceDispatcherHost::DownloadEventHandler
    : public ResourceDispatcherHost::EventHandler {
 public:
  DownloadEventHandler(ResourceDispatcherHost* rdh,
                       int render_process_host_id,
                       int render_view_id,
                       int request_id,
                       const std::string& url,
                       DownloadFileManager* manager,
                       URLRequest* request,
                       bool save_as)
      : download_id_(-1),
        global_id_(render_process_host_id, request_id),
        render_view_id_(render_view_id),
        read_buffer_(NULL),
        url_(UTF8ToWide(url)),
        content_length_(0),
        download_manager_(manager),
        request_(request),
        save_as_(save_as),
        buffer_(new DownloadBuffer),
        rdh_(rdh),
        is_paused_(false) {
  }

  // Not needed, as this event handler ought to be the final resource.
  bool OnRequestRedirected(int request_id, const GURL& url) {
    url_ = UTF8ToWide(url.spec());
    return true;
  }

  // Send the download creation information to the download thread.
  bool OnResponseStarted(int request_id, Response* response) {
    std::string content_disposition;
    request_->GetResponseHeaderByName("content-disposition",
                                      &content_disposition);
    set_content_disposition(content_disposition);
    set_content_length(response->response_head.content_length);

    download_id_ = download_manager_->GetNextId();
    // |download_manager_| consumes (deletes):
    DownloadCreateInfo* info = new DownloadCreateInfo;
    info->url = url_;
    info->start_time = Time::Now();
    info->received_bytes = 0;
    info->total_bytes = content_length_;
    info->state = DownloadItem::IN_PROGRESS;
    info->download_id = download_id_;
    info->render_process_id = global_id_.render_process_host_id;
    info->render_view_id = render_view_id_;
    info->request_id = global_id_.request_id;
    info->content_disposition = content_disposition_;
    info->mime_type = response->response_head.mime_type;
    info->save_as = save_as_;
    info->is_dangerous = false;
    download_manager_->file_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(download_manager_,
                          &DownloadFileManager::StartDownload,
                          info));
    return true;
  }

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size) {
    DCHECK(buf && buf_size);
    if (!read_buffer_) {
      *buf_size = min_size < 0 ? kReadBufSize : min_size;
      read_buffer_ = new char[*buf_size];
    }
    *buf = read_buffer_;
    return true;
  }

  // Pass the buffer to the download file writer.
  bool OnReadCompleted(int request_id, int* bytes_read) {
    if (!*bytes_read)
      return true;
    DCHECK(read_buffer_);
    AutoLock auto_lock(buffer_->lock);
    bool need_update = buffer_->contents.empty();
    buffer_->contents.push_back(std::make_pair(read_buffer_, *bytes_read));
    if (need_update) {
      download_manager_->file_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(download_manager_,
                            &DownloadFileManager::UpdateDownload,
                            download_id_,
                            buffer_));
    }
    read_buffer_ = NULL;

    // We schedule a pause outside of the read loop if there is too much file
    // writing work to do.
    if (buffer_->contents.size() > kLoadsToWrite)
      StartPauseTimer();

    return true;
  }

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status) {
    download_manager_->file_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(download_manager_,
                          &DownloadFileManager::DownloadFinished,
                          download_id_,
                          buffer_));
    delete [] read_buffer_;

    // 'buffer_' is deleted by the DownloadFileManager.
    buffer_ = NULL;
    return true;
  }

  // If the content-length header is not present (or contains something other
  // than numbers), the incoming content_length is -1 (unknown size).
  // Set the content length to 0 to indicate unknown size to DownloadManager.
  void set_content_length(const int64& content_length) {
    content_length_ = 0;
    if (content_length > 0)
      content_length_ = content_length;
  }

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

  void CheckWriteProgress() {
    if (!buffer_)
      return;  // The download completed while we were waiting to run.

    size_t contents_size;
    {
      AutoLock lock(buffer_->lock);
      contents_size = buffer_->contents.size();
    }

    bool should_pause = contents_size > kLoadsToWrite;

    // We'll come back later and see if it's okay to unpause the request.
    if (should_pause)
      StartPauseTimer();

    if (is_paused_ != should_pause) {
      rdh_->PauseRequest(global_id_.render_process_host_id,
                         global_id_.request_id,
                         should_pause);
      is_paused_ = should_pause;
    }
  }

 private:
  void StartPauseTimer() {
    if (!pause_timer_.IsRunning())
      pause_timer_.Start(TimeDelta::FromMilliseconds(kThrottleTimeMs), this,
                         &DownloadEventHandler::CheckWriteProgress);
  }

  int download_id_;
  ResourceDispatcherHost::GlobalRequestID global_id_;
  int render_view_id_;
  char* read_buffer_;
  std::string content_disposition_;
  std::wstring url_;
  int64 content_length_;
  DownloadFileManager* download_manager_;
  URLRequest* request_;
  bool save_as_;  // Request was initiated via "Save As" by the user.
  DownloadBuffer* buffer_;
  ResourceDispatcherHost* rdh_;
  bool is_paused_;
  base::OneShotTimer<DownloadEventHandler> pause_timer_;

  static const int kReadBufSize = 32768;  // bytes
  static const int kLoadsToWrite = 100;  // number of data buffers queued
  static const int kThrottleTimeMs = 200;  // milliseconds

  DISALLOW_EVIL_CONSTRUCTORS(DownloadEventHandler);
};

// DownloadThrottlingEventHandler----------------------------------------------

// DownloadThrottlingEventHandler is used to determine if a download should be
// allowed. When a DownloadThrottlingEventHandler is created it pauses the
// download and asks the DownloadRequestManager if the download should be
// allowed. The DownloadRequestManager notifies us asynchronously as to whether
// the download is allowed or not. If the download is allowed the request is
// resumed, a DownloadEventHandler is created and all EventHandler methods are
// delegated to it. If the download is not allowed the request is canceled.

class ResourceDispatcherHost::DownloadThrottlingEventHandler :
    public ResourceDispatcherHost::EventHandler,
    public DownloadRequestManager::Callback {
 public:
  DownloadThrottlingEventHandler(ResourceDispatcherHost* host,
                                 URLRequest* request,
                                 const std::string& url,
                                 int render_process_host_id,
                                 int render_view_id,
                                 int request_id,
                                 bool in_complete)
      : host_(host),
        request_(request),
        url_(url),
        render_process_host_id_(render_process_host_id),
        render_view_id_(render_view_id),
        request_id_(request_id),
        tmp_buffer_length_(0),
        ignore_on_read_complete_(in_complete) {
    // Pause the request.
    host_->PauseRequest(render_process_host_id_, request_id_, true);
    host_->download_request_manager()->CanDownloadOnIOThread(
        render_process_host_id_, render_view_id, this);
   }

  virtual ~DownloadThrottlingEventHandler() {}

  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) {
    if (download_handler_.get())
      return download_handler_->OnUploadProgress(request_id, position, size);
    return true;
  }

  virtual bool OnRequestRedirected(int request_id, const GURL& url) {
    if (download_handler_.get())
      return download_handler_->OnRequestRedirected(request_id, url);
    url_ = url.spec();
    return true;
  }

  virtual bool OnResponseStarted(int request_id, Response* response) {
    if (download_handler_.get())
      return download_handler_->OnResponseStarted(request_id, response);
    response_ = response;
    return true;
  }

  virtual bool OnWillRead(int request_id,
                          char** buf,
                          int* buf_size,
                          int min_size) {
    if (download_handler_.get())
      return download_handler_->OnWillRead(request_id, buf, buf_size, min_size);

    // We should only have this invoked once, as such we only deal with one
    // tmp buffer.
    DCHECK(!tmp_buffer_.get());
    if (min_size < 0)
      min_size = 1024;
    tmp_buffer_.reset(new char[min_size]);
    *buf = tmp_buffer_.get();
    *buf_size = min_size;
    return true;
  }

  virtual bool OnReadCompleted(int request_id, int* bytes_read) {
    if (ignore_on_read_complete_) {
      // See comments above definition for details on this.
      ignore_on_read_complete_ = false;
      return true;
    }
    if (!*bytes_read)
      return true;

    if (tmp_buffer_.get()) {
      DCHECK(!tmp_buffer_length_);
      tmp_buffer_length_ = *bytes_read;
      if (download_handler_.get())
        CopyTmpBufferToDownloadHandler();
      return true;
    }
    if (download_handler_.get())
      return download_handler_->OnReadCompleted(request_id, bytes_read);
    return true;
  }

  virtual bool OnResponseCompleted(int request_id,
                                   const URLRequestStatus& status) {
    if (download_handler_.get())
      return download_handler_->OnResponseCompleted(request_id, status);
    NOTREACHED();
    return true;
  }

  void CancelDownload() {
    host_->CancelRequest(render_process_host_id_, request_id_, false);
  }

  void ContinueDownload() {
    DCHECK(!download_handler_.get());
    download_handler_ =
        new DownloadEventHandler(host_,
                                 render_process_host_id_,
                                 render_view_id_,
                                 request_id_,
                                 url_,
                                 host_->download_file_manager(),
                                 request_,
                                 false);
    if (response_.get())
      download_handler_->OnResponseStarted(request_id_, response_.get());

    if (tmp_buffer_length_)
      CopyTmpBufferToDownloadHandler();

    // And let the request continue.
    host_->PauseRequest(render_process_host_id_, request_id_, false);
  }

 private:
  void CopyTmpBufferToDownloadHandler() {
    // Copy over the tmp buffer.
    char* buffer;
    int buf_size;
    if (download_handler_->OnWillRead(request_id_, &buffer, &buf_size,
                                      tmp_buffer_length_)) {
      CHECK(buf_size >= tmp_buffer_length_);
      memcpy(buffer, tmp_buffer_.get(), tmp_buffer_length_);
      download_handler_->OnReadCompleted(request_id_, &tmp_buffer_length_);
    }
    tmp_buffer_length_ = 0;
    tmp_buffer_.reset();
  }

  ResourceDispatcherHost* host_;
  URLRequest* request_;
  std::string url_;
  int render_process_host_id_;
  int render_view_id_;
  int request_id_;

  // Handles the actual download. This is only created if the download is
  // allowed to continue.
  scoped_refptr<DownloadEventHandler> download_handler_;

  // Response supplied to OnResponseStarted. Only non-null if OnResponseStarted
  // is invoked.
  scoped_refptr<Response> response_;

  // If we're created by way of BufferedEventHandler we'll get one request for
  // a buffer. This is that buffer.
  scoped_array<char> tmp_buffer_;
  int tmp_buffer_length_;

  // If true the next call to OnReadCompleted is ignored. This is used if we're
  // paused during a call to OnReadCompleted. Pausing during OnReadCompleted
  // results in two calls to OnReadCompleted for the same data. This make sure
  // we ignore one of them.
  bool ignore_on_read_complete_;

  DISALLOW_COPY_AND_ASSIGN(DownloadThrottlingEventHandler);
};


// ----------------------------------------------------------------------------

// Checks that a url is safe.
class SafeBrowsingEventHandler
    : public ResourceDispatcherHost::EventHandler,
      public SafeBrowsingService::Client {
 public:
  SafeBrowsingEventHandler(ResourceDispatcherHost::EventHandler* handler,
                           int render_process_host_id,
                           int render_view_id,
                           const GURL& url,
                           ResourceType::Type resource_type,
                           SafeBrowsingService* safe_browsing,
                           ResourceDispatcherHost* resource_dispatcher_host)
      : next_handler_(handler),
        render_process_host_id_(render_process_host_id),
        render_view_id_(render_view_id),
        paused_request_id_(-1),
        safe_browsing_(safe_browsing),
        in_safe_browsing_check_(false),
        displaying_blocking_page_(false),
        rdh_(resource_dispatcher_host),
        queued_error_request_id_(-1),
        resource_type_(resource_type) {
    if (safe_browsing_->CheckUrl(url, this)) {
      safe_browsing_result_ = SafeBrowsingService::URL_SAFE;
      safe_browsing_->LogPauseDelay(TimeDelta());  // No delay.
    } else {
      AddRef();
      in_safe_browsing_check_ = true;
      // Can't pause now because it's too early, so we'll do it in OnWillRead.
    }
  }

  bool OnUploadProgress(int request_id, uint64 position, uint64 size) {
    return next_handler_->OnUploadProgress(request_id, position, size);
  }

  bool OnRequestRedirected(int request_id, const GURL& new_url) {
    if (in_safe_browsing_check_) {
      Release();
      in_safe_browsing_check_ = false;
      safe_browsing_->CancelCheck(this);
    }

    if (safe_browsing_->CheckUrl(new_url, this)) {
      safe_browsing_result_ = SafeBrowsingService::URL_SAFE;
      safe_browsing_->LogPauseDelay(TimeDelta());  // No delay.
    } else {
      AddRef();
      in_safe_browsing_check_ = true;
      // Can't pause now because it's too early, so we'll do it in OnWillRead.
    }

    return next_handler_->OnRequestRedirected(request_id, new_url);
  }

  bool OnResponseStarted(int request_id,
                         ResourceDispatcherHost::Response* response) {
    return next_handler_->OnResponseStarted(request_id, response);
  }

  void OnGetHashTimeout() {
    if (!in_safe_browsing_check_)
      return;

    safe_browsing_->CancelCheck(this);
    OnUrlCheckResult(GURL::EmptyGURL(), SafeBrowsingService::URL_SAFE);
  }

  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size) {
    if (in_safe_browsing_check_ && pause_time_.is_null()) {
      pause_time_ = Time::Now();
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          NewRunnableMethod(this, &SafeBrowsingEventHandler::OnGetHashTimeout),
          kMaxGetHashMs);
    }

    if (in_safe_browsing_check_ || displaying_blocking_page_) {
      rdh_->PauseRequest(render_process_host_id_, request_id, true);
      paused_request_id_ = request_id;
    }

    return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
  }

  bool OnReadCompleted(int request_id, int* bytes_read) {
    return next_handler_->OnReadCompleted(request_id, bytes_read);
  }

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status) {
    if ((in_safe_browsing_check_ ||
         safe_browsing_result_ != SafeBrowsingService::URL_SAFE) &&
        status.status() == URLRequestStatus::FAILED &&
        status.os_error() == net::ERR_NAME_NOT_RESOLVED) {
      // Got a DNS error while the safebrowsing check is in progress or we
      // already know that the site is unsafe.  Don't show the the dns error
      // page.
      queued_error_.reset(new URLRequestStatus(status));
      queued_error_request_id_ = request_id;
      return true;
    }

    return next_handler_->OnResponseCompleted(request_id, status);
  }

  // SafeBrowsingService::Client implementation, called on the IO thread once
  // the URL has been classified.
  void OnUrlCheckResult(const GURL& url,
                        SafeBrowsingService::UrlCheckResult result) {
    DCHECK(in_safe_browsing_check_);
    DCHECK(!displaying_blocking_page_);

    safe_browsing_result_ = result;
    in_safe_browsing_check_ = false;

    if (result == SafeBrowsingService::URL_SAFE) {
      if (paused_request_id_ != -1) {
        rdh_->PauseRequest(render_process_host_id_, paused_request_id_, false);
        paused_request_id_ = -1;
      }

      TimeDelta pause_delta;
      if (!pause_time_.is_null())
        pause_delta = Time::Now() - pause_time_;
      safe_browsing_->LogPauseDelay(pause_delta);

      if (queued_error_.get()) {
        next_handler_->OnResponseCompleted(
            queued_error_request_id_, *queued_error_.get());
        queued_error_.reset();
      }

      Release();
    } else {
      displaying_blocking_page_ = true;
      safe_browsing_->DisplayBlockingPage(
          url, resource_type_, result, this, rdh_->ui_loop(),
          render_process_host_id_, render_view_id_);
    }
  }

  // SafeBrowsingService::Client implementation, called on the IO thread when
  // the user has decided to proceed with the current request, or go back.
  void OnBlockingPageComplete(bool proceed) {
    DCHECK(displaying_blocking_page_);
    displaying_blocking_page_ = false;

    if (proceed) {
      safe_browsing_result_ = SafeBrowsingService::URL_SAFE;
      if (paused_request_id_ != -1) {
        rdh_->PauseRequest(render_process_host_id_, paused_request_id_, false);
        paused_request_id_ = -1;
      }

      if (queued_error_.get()) {
        next_handler_->OnResponseCompleted(
            queued_error_request_id_, *queued_error_.get());
        queued_error_.reset();
      }
    } else {
      rdh_->CancelRequest(render_process_host_id_, paused_request_id_, false);
    }

    Release();
  }

 private:
  scoped_refptr<ResourceDispatcherHost::EventHandler> next_handler_;
  int render_process_host_id_;
  int render_view_id_;
  int paused_request_id_;  // -1 if not paused
  bool in_safe_browsing_check_;
  bool displaying_blocking_page_;
  SafeBrowsingService::UrlCheckResult safe_browsing_result_;
  scoped_refptr<SafeBrowsingService> safe_browsing_;
  scoped_ptr<URLRequestStatus> queued_error_;
  int queued_error_request_id_;
  ResourceDispatcherHost* rdh_;
  Time pause_time_;
  ResourceType::Type resource_type_;
};

// ----------------------------------------------------------------------------
// ResourceDispatcherHost::CrossSiteEventHandler

// Task to notify the WebContents that a cross-site response has begun, so that
// WebContents can tell the old page to run its onunload handler.
class ResourceDispatcherHost::CrossSiteNotifyTabTask : public Task {
 public:
  CrossSiteNotifyTabTask(int render_process_host_id,
                         int render_view_id,
                         int request_id)
    : render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id),
      request_id_(request_id) {}

  void Run() {
    RenderViewHost* view =
        RenderViewHost::FromID(render_process_host_id_, render_view_id_);
    if (view) {
      view->OnCrossSiteResponse(render_process_host_id_, request_id_);
    } else {
      // The view couldn't be found.
      // TODO(creis): Should notify the IO thread to proceed anyway, using
      // ResourceDispatcherHost::OnClosePageACK.
    }
  }

 private:
  int render_process_host_id_;
  int render_view_id_;
  int request_id_;
};

// Ensures that cross-site responses are delayed until the onunload handler of
// the previous page is allowed to run.  This handler wraps an
// AsyncEventHandler, and it sits inside SafeBrowsing and Buffered event
// handlers.  This is important, so that it can intercept OnResponseStarted
// after we determine that a response is safe and not a download.
class ResourceDispatcherHost::CrossSiteEventHandler
    : public ResourceDispatcherHost::EventHandler {
 public:
  CrossSiteEventHandler(ResourceDispatcherHost::EventHandler* handler,
                        int render_process_host_id,
                        int render_view_id,
                        ResourceDispatcherHost* resource_dispatcher_host)
      : next_handler_(handler),
        render_process_host_id_(render_process_host_id),
        render_view_id_(render_view_id),
        has_started_response_(false),
        in_cross_site_transition_(false),
        request_id_(-1),
        completed_during_transition_(false),
        completed_status_(),
        response_(NULL),
        rdh_(resource_dispatcher_host) {}

  bool OnRequestRedirected(int request_id, const GURL& new_url) {
    // We should not have started the transition before being redirected.
    DCHECK(!in_cross_site_transition_);
    return next_handler_->OnRequestRedirected(request_id, new_url);
  }

  bool OnResponseStarted(int request_id,
                         ResourceDispatcherHost::Response* response) {
    // At this point, we know that the response is safe to send back to the
    // renderer: it is not a download, and it has passed the SSL and safe
    // browsing checks.
    // We should not have already started the transition before now.
    DCHECK(!in_cross_site_transition_);
    has_started_response_ = true;

    // Look up the request and associated info.
    GlobalRequestID global_id(render_process_host_id_, request_id);
    URLRequest* request = rdh_->GetURLRequest(global_id);
    if (!request) {
      DLOG(WARNING) << "Request wasn't found";
      return false;
    }
    ExtraRequestInfo* info = ExtraInfoForRequest(request);

    // If this is a download, just pass the response through without doing a
    // cross-site check.  The renderer will see it is a download and abort the
    // request.
    if (info->is_download) {
      return next_handler_->OnResponseStarted(request_id, response);
    }

    // Tell the renderer to run the onunload event handler, and wait for the
    // reply.
    StartCrossSiteTransition(request_id, response, global_id);
    return true;
  }

  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size) {
    return next_handler_->OnWillRead(request_id, buf, buf_size, min_size);
  }

  bool OnReadCompleted(int request_id, int* bytes_read) {
    if (!in_cross_site_transition_) {
      return next_handler_->OnReadCompleted(request_id, bytes_read);
    }
    return true;
  }

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status) {
    if (!in_cross_site_transition_) {
      if (has_started_response_) {
        // We've already completed the transition, so just pass it through.
        return next_handler_->OnResponseCompleted(request_id, status);
      } else {
        // Some types of failures will call OnResponseCompleted without calling
        // CrossSiteEventHandler::OnResponseStarted.  We should wait now for
        // the cross-site transition.  Also continue with the logic below to
        // remember that we completed during the cross-site transition.
        GlobalRequestID global_id(render_process_host_id_, request_id);
        StartCrossSiteTransition(request_id, NULL, global_id);
      }
    }

    // We have to buffer the call until after the transition completes.
    completed_during_transition_ = true;
    completed_status_ = status;

    // Return false to tell RDH not to notify the world or clean up the
    // pending request.  We will do so in ResumeResponse.
    return false;
  }

  // We can now send the response to the new renderer, which will cause
  // WebContents to swap in the new renderer and destroy the old one.
  void ResumeResponse() {
    DCHECK(request_id_ != -1);
    DCHECK(in_cross_site_transition_);
    in_cross_site_transition_ = false;

    // Find the request for this response.
    GlobalRequestID global_id(render_process_host_id_, request_id_);
    URLRequest* request = rdh_->GetURLRequest(global_id);
    if (!request) {
      DLOG(WARNING) << "Resuming a request that wasn't found";
      return;
    }
    ExtraRequestInfo* info = ExtraInfoForRequest(request);

    if (has_started_response_) {
      // Send OnResponseStarted to the new renderer.
      DCHECK(response_);
      next_handler_->OnResponseStarted(request_id_, response_);

      // Unpause the request to resume reading.  Any further reads will be
      // directed toward the new renderer.
      rdh_->PauseRequest(render_process_host_id_, request_id_, false);
    }

    // Remove ourselves from the ExtraRequestInfo.
    info->cross_site_handler = NULL;

    // If the response completed during the transition, notify the next
    // event handler.
    if (completed_during_transition_) {
      next_handler_->OnResponseCompleted(request_id_, completed_status_);

      // Since we didn't notify the world or clean up the pending request in
      // RDH::OnResponseCompleted during the transition, we should do it now.
      rdh_->NotifyResponseCompleted(request, render_process_host_id_);
      rdh_->RemovePendingRequest(render_process_host_id_, request_id_);
    }
  }

 private:
  // Prepare to render the cross-site response in a new RenderViewHost, by
  // telling the old RenderViewHost to run its onunload handler.
  void StartCrossSiteTransition(int request_id,
                                ResourceDispatcherHost::Response* response,
                                GlobalRequestID global_id) {
    in_cross_site_transition_ = true;
    request_id_ = request_id;
    response_ = response;

    // Store this handler on the ExtraRequestInfo, so that RDH can call our
    // ResumeResponse method when the close ACK is received.
    URLRequest* request = rdh_->GetURLRequest(global_id);
    if (!request) {
      DLOG(WARNING) << "Cross site response for a request that wasn't found";
      return;
    }
    ExtraRequestInfo* info = ExtraInfoForRequest(request);
    info->cross_site_handler = this;

    if (has_started_response_) {
      // Pause the request until the old renderer is finished and the new
      // renderer is ready.
      rdh_->PauseRequest(render_process_host_id_, request_id, true);
    }
    // If our OnResponseStarted wasn't called, then we're being called by
    // OnResponseCompleted after a failure.  We don't need to pause, because
    // there will be no reads.

    // Tell the tab responsible for this request that a cross-site response is
    // starting, so that it can tell its old renderer to run its onunload
    // handler now.  We will wait to hear the corresponding ClosePage_ACK.
    ResourceDispatcherHost::CrossSiteNotifyTabTask* task =
        new CrossSiteNotifyTabTask(render_process_host_id_,
                                   render_view_id_,
                                   request_id);
    rdh_->ui_loop()->PostTask(FROM_HERE, task);
  }

  scoped_refptr<ResourceDispatcherHost::EventHandler> next_handler_;
  int render_process_host_id_;
  int render_view_id_;
  bool has_started_response_;
  bool in_cross_site_transition_;
  int request_id_;
  bool completed_during_transition_;
  URLRequestStatus completed_status_;
  ResourceDispatcherHost::Response* response_;
  ResourceDispatcherHost* rdh_;
};

// ----------------------------------------------------------------------------
// ResourceDispatcherHost::BufferedEventHandler

// Used to buffer a request until enough data has been received.
class ResourceDispatcherHost::BufferedEventHandler
    : public ResourceDispatcherHost::EventHandler {
 public:
  BufferedEventHandler(ResourceDispatcherHost::EventHandler* handler,
                       ResourceDispatcherHost* host, URLRequest* request)
    : real_handler_(handler),
      host_(host),
      request_(request),
      bytes_read_(0),
      sniff_content_(false),
      should_buffer_(false),
      buffering_(false),
      finished_(false) {}

  bool OnUploadProgress(int request_id, uint64 position, uint64 size) {
    return real_handler_->OnUploadProgress(request_id, position, size);
  }

  bool OnRequestRedirected(int request_id, const GURL& new_url) {
    return real_handler_->OnRequestRedirected(request_id, new_url);
  }

  bool OnResponseStarted(int request_id, Response* response) {
    response_ = response;
    if (!DelayResponse())
      return CompleteResponseStarted(request_id, false);
    return true;
  }

  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status) {
    return real_handler_->OnResponseCompleted(request_id, status);
  }

 private:
  // Returns true if we should delay OnResponseStarted forwarding.
  bool DelayResponse();

  // Returns true if there will be a need to parse the DocType of the document
  // to determine the right way to handle it.
  bool ShouldBuffer(const GURL& url, const std::string& mime_type);

  // Returns true if there is enough information to process the DocType.
  bool DidBufferEnough(int bytes_read) {
    const int kRequiredLength = 256;

    return bytes_read >= kRequiredLength;
  }

  // Returns true if we have to keep buffering data.
  bool KeepBuffering(int bytes_read);

  // Sends a pending OnResponseStarted notification. |in_complete| is true if
  // this is invoked from |OnResponseCompleted|.
  bool CompleteResponseStarted(int request_id, bool in_complete);

  scoped_refptr<ResourceDispatcherHost::EventHandler> real_handler_;
  scoped_refptr<Response> response_;
  ResourceDispatcherHost* host_;
  URLRequest* request_;
  char* read_buffer_;
  int read_buffer_size_;
  int bytes_read_;
  bool sniff_content_;
  bool should_buffer_;
  bool buffering_;
  bool finished_;

  DISALLOW_EVIL_CONSTRUCTORS(BufferedEventHandler);
};

// We'll let the original event handler provide a buffer, and reuse it for
// subsequent reads until we're done buffering.
bool ResourceDispatcherHost::BufferedEventHandler::OnWillRead(
    int request_id, char** buf, int* buf_size, int min_size) {
  if (buffering_) {
    *buf = read_buffer_ + bytes_read_;
    *buf_size = read_buffer_size_ - bytes_read_;
    DCHECK(*buf_size > 0);
    return true;
  }

  if (finished_)
    return false;

  bool ret = real_handler_->OnWillRead(request_id, buf, buf_size, min_size);
  read_buffer_ = *buf;
  read_buffer_size_ = *buf_size;
  bytes_read_ = 0;
  return ret;
}

bool ResourceDispatcherHost::BufferedEventHandler::OnReadCompleted(
    int request_id, int* bytes_read) {
  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request_);

  if (sniff_content_ || should_buffer_) {
    if (KeepBuffering(*bytes_read))
      return true;

    LOG(INFO) << "Finished buffering " << request_->url().spec();
    sniff_content_ = should_buffer_ = false;
    *bytes_read = bytes_read_;

    // Done buffering, send the pending ResponseStarted event.
    if (!CompleteResponseStarted(request_id, true))
      return false;
  }

  return real_handler_->OnReadCompleted(request_id, bytes_read);
}

bool ResourceDispatcherHost::BufferedEventHandler::DelayResponse() {
  std::string mime_type;
  request_->GetMimeType(&mime_type);

  std::string content_type_options;
  request_->GetResponseHeaderByName("x-content-type-options",
                                    &content_type_options);
  if (content_type_options != "nosniff" &&
      net::ShouldSniffMimeType(request_->url(), mime_type)) {
    // We're going to look at the data before deciding what the content type
    // is.  That means we need to delay sending the ResponseStarted message
    // over the IPC channel.
    sniff_content_ = true;
    LOG(INFO) << "To buffer: " << request_->url().spec();
    return true;
  }

  if (ShouldBuffer(request_->url(), mime_type)) {
    // This is a temporary fix for the fact that webkit expects to have
    // enough data to decode the doctype in order to select the rendering
    // mode.
    should_buffer_ = true;
    LOG(INFO) << "To buffer: " << request_->url().spec();
    return true;
  }
  return false;
}

bool ResourceDispatcherHost::BufferedEventHandler::ShouldBuffer(
    const GURL& url, const std::string& mime_type) {
  // We are willing to buffer for HTTP and HTTPS.
  bool sniffable_scheme = url.is_empty() ||
                          url.SchemeIs("http") ||
                          url.SchemeIs("https");
  if (!sniffable_scheme)
    return false;

  // Today, the only reason to buffer the request is to fix the doctype decoding
  // performed by webkit: if there is not enough data it will go to quirks mode.
  // We only expect the doctype check to apply to html documents.
  return mime_type == "text/html";
}

bool ResourceDispatcherHost::BufferedEventHandler::KeepBuffering(
    int bytes_read) {
  DCHECK(read_buffer_);
  bytes_read_ += bytes_read;
  finished_ = (bytes_read == 0);

  if (sniff_content_) {
    std::string type_hint, new_type;
    request_->GetMimeType(&type_hint);

    if (!net::SniffMimeType(read_buffer_, bytes_read_, request_->url(),
                            type_hint, &new_type)) {
      // SniffMimeType() returns false if there is not enough data to determine
      // the mime type. However, even if it returns false, it returns a new type
      // that is probably better than the current one.
      DCHECK(bytes_read_ < 512 /*kMaxBytesToSniff*/);
      if (!finished_) {
        buffering_ = true;
        return true;
      }
    }
    sniff_content_ = false;
    response_->response_head.mime_type.assign(new_type);

    // We just sniffed the mime type, maybe there is a doctype to process.
    if (ShouldBuffer(request_->url(), new_type))
      should_buffer_ = true;
  }

  if (!finished_ && should_buffer_) {
    if (!DidBufferEnough(bytes_read_)) {
      buffering_ = true;
      return true;
    }
  }
  buffering_ = false;
  return false;
}

bool ResourceDispatcherHost::BufferedEventHandler::CompleteResponseStarted(
    int request_id,
    bool in_complete) {
  // Check to see if we should forward the data from this request to the
  // download thread.
  // TODO(paulg): Only download if the context from the renderer allows it.
  std::string content_disposition;
  request_->GetResponseHeaderByName("content-disposition",
                                    &content_disposition);

  ResourceDispatcherHost::ExtraRequestInfo* info =
      ResourceDispatcherHost::ExtraInfoForRequest(request_);

  if (info->allow_download &&
      host_->ShouldDownload(response_->response_head.mime_type,
                            content_disposition)) {
    if (response_->response_head.headers &&  // Can be NULL if FTP.
        response_->response_head.headers->response_code() / 100 != 2) {
      // The response code indicates that this is an error page, but we don't
      // know how to display the content.  We follow Firefox here and show our
      // own error page instead of triggering a download.
      // TODO(abarth): We should abstract the response_code test, but this kind
      //               of check is scattered throughout our codebase.
      request_->CancelWithError(net::ERR_FILE_NOT_FOUND);
      return false;
    }

    info->is_download = true;

    scoped_refptr<DownloadThrottlingEventHandler> download_handler =
        new DownloadThrottlingEventHandler(host_,
                                           request_,
                                           request_->url().spec(),
                                           info->render_process_host_id,
                                           info->render_view_id,
                                           request_id,
                                           in_complete);
    if (bytes_read_) {
      // a Read has already occurred and we need to copy the data into the
      // EventHandler.
      char *buf = NULL;
      int buf_len = 0;
      download_handler->OnWillRead(request_id, &buf, &buf_len, bytes_read_);
      CHECK((buf_len >= bytes_read_) && (bytes_read_ >= 0));
      memcpy(buf, read_buffer_, bytes_read_);
    }
    // Update the renderer with the response headers which will cause it to
    // cancel the request.
    // TODO(paulg): Send the renderer a response that indicates that the request
    //              will be handled by an external source (the browser).
    real_handler_->OnResponseStarted(info->request_id, response_);
    real_handler_ = download_handler;
  }
  return real_handler_->OnResponseStarted(request_id, response_);
}

namespace {

// Consults the RendererSecurity policy to determine whether the
// ResourceDispatcherHost should service this request.  A request might be
// disallowed if the renderer is not authorized to restrive the request URL or
// if the renderer is attempting to upload an unauthorized file.
bool ShouldServiceRequest(int render_process_host_id,
                          const ViewHostMsg_Resource_Request& request_data)  {
  // TODO(mpcomplete): remove this when http://b/viewIssue?id=1080959 is fixed.
  if (render_process_host_id == -1)
    return true;

  RendererSecurityPolicy* policy = RendererSecurityPolicy::GetInstance();

  // Check if the renderer is permitted to request the requested URL.
  if (!policy->CanRequestURL(render_process_host_id, request_data.url)) {
    LOG(INFO) << "Denied unauthorized request for " <<
        request_data.url.possibly_invalid_spec();
    return false;
  }

  // Check if the renderer is permitted to upload the requested files.
  const std::vector<net::UploadData::Element>& uploads =
      request_data.upload_content;
  for (std::vector<net::UploadData::Element>::const_iterator iter(uploads.begin());
      iter != uploads.end(); ++iter) {
    if (iter->type() == net::UploadData::TYPE_FILE &&
        !policy->CanUploadFile(render_process_host_id, iter->file_path())) {
      NOTREACHED() << "Denied unauthorized upload of " << iter->file_path();
      return false;
    }
  }

  return true;
}

}  // namespace

// ----------------------------------------------------------------------------
// ResourceDispatcherHost::SaveFileEventHandler
// Forwards data to the save thread.
class ResourceDispatcherHost::SaveFileEventHandler
    : public ResourceDispatcherHost::EventHandler {
 public:
  SaveFileEventHandler(int render_process_host_id,
                       int render_view_id,
                       const std::string& url,
                       SaveFileManager* manager)
      : save_id_(-1),
        render_process_id_(render_process_host_id),
        render_view_id_(render_view_id),
        read_buffer_(NULL),
        url_(UTF8ToWide(url)),
        content_length_(0),
        save_manager_(manager) {
  }

  // Save the redirected URL to final_url_, we need to use the original
  // URL to match original request.
  bool OnRequestRedirected(int request_id, const GURL& url) {
    final_url_ = UTF8ToWide(url.spec());
    return true;
  }

  // Send the download creation information to the download thread.
  bool OnResponseStarted(int request_id, Response* response) {
    save_id_ = save_manager_->GetNextId();
    // |save_manager_| consumes (deletes):
    SaveFileCreateInfo* info = new SaveFileCreateInfo;
    info->url = url_;
    info->final_url = final_url_;
    info->total_bytes = content_length_;
    info->save_id = save_id_;
    info->render_process_id = render_process_id_;
    info->render_view_id = render_view_id_;
    info->request_id = request_id;
    info->content_disposition = content_disposition_;
    info->save_source = SaveFileCreateInfo::SAVE_FILE_FROM_NET;
    save_manager_->GetSaveLoop()->PostTask(FROM_HERE,
        NewRunnableMethod(save_manager_,
                          &SaveFileManager::StartSave,
                          info));
    return true;
  }

  // Create a new buffer, which will be handed to the download thread for file
  // writing and deletion.
  bool OnWillRead(int request_id, char** buf, int* buf_size, int min_size) {
    DCHECK(buf && buf_size);
    if (!read_buffer_) {
      *buf_size = min_size < 0 ? kReadBufSize : min_size;
      read_buffer_ = new char[*buf_size];
    }
    *buf = read_buffer_;
    return true;
  }

  // Pass the buffer to the download file writer.
  bool OnReadCompleted(int request_id, int* bytes_read) {
    DCHECK(read_buffer_);
    save_manager_->GetSaveLoop()->PostTask(FROM_HERE,
        NewRunnableMethod(save_manager_,
                          &SaveFileManager::UpdateSaveProgress,
                          save_id_,
                          read_buffer_,
                          *bytes_read));
    read_buffer_ = NULL;
    return true;
  }

  bool OnResponseCompleted(int request_id, const URLRequestStatus& status) {
    save_manager_->GetSaveLoop()->PostTask(FROM_HERE,
        NewRunnableMethod(save_manager_,
                          &SaveFileManager::SaveFinished,
                          save_id_,
                          url_,
                          render_process_id_,
                          status.is_success() && !status.is_io_pending()));
    delete [] read_buffer_;
    return true;
  }

  // If the content-length header is not present (or contains something other
  // than numbers), StringToInt64 returns 0, which indicates 'unknown size' and
  // is handled correctly by the SaveManager.
  void set_content_length(const std::string& content_length) {
    content_length_ = StringToInt64(content_length);
  }

  void set_content_disposition(const std::string& content_disposition) {
    content_disposition_ = content_disposition;
  }

 private:
  int save_id_;
  int render_process_id_;
  int render_view_id_;
  char* read_buffer_;
  std::string content_disposition_;
  std::wstring url_;
  std::wstring final_url_;
  int64 content_length_;
  SaveFileManager* save_manager_;

  static const int kReadBufSize = 32768;  // bytes

  DISALLOW_EVIL_CONSTRUCTORS(SaveFileEventHandler);
};

// ----------------------------------------------------------------------------
// ResourceDispatcherHost

ResourceDispatcherHost::ResourceDispatcherHost(MessageLoop* io_loop)
    : ui_loop_(MessageLoop::current()),
      io_loop_(io_loop),
      download_file_manager_(new DownloadFileManager(ui_loop_, this)),
      download_request_manager_(new DownloadRequestManager(io_loop, ui_loop_)),
      save_file_manager_(new SaveFileManager(ui_loop_, io_loop, this)),
      safe_browsing_(new SafeBrowsingService),
      request_id_(-1),
      plugin_service_(PluginService::GetInstance()),
      method_runner_(this),
      is_shutdown_(false) {
}

ResourceDispatcherHost::~ResourceDispatcherHost() {
  AsyncEventHandler::GlobalCleanup();
  STLDeleteValues(&pending_requests_);
}

void ResourceDispatcherHost::Initialize() {
  DCHECK(MessageLoop::current() == ui_loop_);
  download_file_manager_->Initialize();
  safe_browsing_->Initialize(io_loop_);
}

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
                                                    int render_process_host_id,
                                                    int tab_contents_id,
                                                    const GURL& url,
                                                    ResourceType::Type type,
                                                    EventHandler* handler) {
  if (!ResourceType::IsFrame(type) || URLRequest::IsHandledURL(url))
    return false;

  ui_loop_->PostTask(FROM_HERE, NewRunnableFunction(
      &ExternalProtocolHandler::LaunchUrl, url, render_process_host_id,
      tab_contents_id));

  handler->OnResponseCompleted(request_id, URLRequestStatus(
                                               URLRequestStatus::FAILED,
                                               net::ERR_ABORTED));
  return true;
}

void ResourceDispatcherHost::BeginRequest(
    Receiver* receiver,
    HANDLE render_process_handle,
    int render_process_host_id,
    int render_view_id,
    int request_id,
    const ViewHostMsg_Resource_Request& request_data,
    URLRequestContext* request_context,
    IPC::Message* sync_result) {
  if (is_shutdown_ ||
      !ShouldServiceRequest(render_process_host_id, request_data)) {
    // Tell the renderer that this request was disallowed.
    receiver->Send(new ViewMsg_Resource_RequestComplete(
        render_view_id,
        request_id,
        URLRequestStatus(URLRequestStatus::FAILED, net::ERR_ABORTED)));
    return;
  }

  // Ensure the Chrome plugins are loaded, as they may intercept network
  // requests.  Does nothing if they are already loaded.
  // TODO(mpcomplete): This takes 200 ms!  Investigate parallelizing this by
  // starting the load earlier in a BG thread.
  plugin_service_->LoadChromePlugins(this);

  // Construct the event handler.
  scoped_refptr<EventHandler> handler;
  if (sync_result) {
    handler = new SyncEventHandler(receiver, request_data.url, sync_result);
  } else {
    handler = new AsyncEventHandler(receiver,
                                    render_process_host_id,
                                    render_view_id,
                                    render_process_handle,
                                    request_data.url,
                                    this);
  }

  if (HandleExternalProtocol(request_id, render_process_host_id, render_view_id,
                             request_data.url, request_data.resource_type,
                             handler)) {
    return;
  }

  // Construct the request.
  URLRequest* request = new URLRequest(request_data.url, this);
  request->set_method(request_data.method);
  request->set_policy_url(request_data.policy_url);
  request->set_referrer(request_data.referrer.spec());
  request->SetExtraRequestHeaders(request_data.headers);
  request->set_load_flags(request_data.load_flags);
  request->set_context(request_context);
  request->set_origin_pid(request_data.origin_pid);

  // Set upload data.
  uint64 upload_size = 0;
  if (!request_data.upload_content.empty()) {
    scoped_refptr<net::UploadData> upload = new net::UploadData();
    upload->set_elements(request_data.upload_content);  // Deep copy.
    request->set_upload(upload);
    upload_size = upload->GetContentLength();
  }

  // Install a CrossSiteEventHandler if this request is coming from a
  // RenderViewHost with a pending cross-site request.  We only check this for
  // MAIN_FRAME requests.
  // TODO(mpcomplete): remove "render_process_host_id != -1"
  //                   when http://b/viewIssue?id=1080959 is fixed.
  if (request_data.resource_type == ResourceType::MAIN_FRAME &&
      render_process_host_id != -1 &&
      Singleton<CrossSiteRequestManager>::get()->
          HasPendingCrossSiteRequest(render_process_host_id, render_view_id)) {
    // Wrap the event handler to be sure the current page's onunload handler
    // has a chance to run before we render the new page.
    handler = new CrossSiteEventHandler(handler,
                                        render_process_host_id,
                                        render_view_id,
                                        this);
  }

  if (safe_browsing_->enabled() &&
      safe_browsing_->CanCheckUrl(request_data.url)) {
    handler = new SafeBrowsingEventHandler(handler,
                                           render_process_host_id,
                                           render_view_id,
                                           request_data.url,
                                           request_data.resource_type,
                                           safe_browsing_,
                                           this);
  }

  // Insert a buffered event handler before the actual one.
  handler = new BufferedEventHandler(handler, this, request);

  // Make extra info and read footer (contains request ID).
  ExtraRequestInfo* extra_info =
      new ExtraRequestInfo(handler,
                           request_id,
                           render_process_host_id,
                           render_view_id,
                           request_data.mixed_content,
                           request_data.resource_type,
                           upload_size);
  extra_info->allow_download =
      ResourceType::IsFrame(request_data.resource_type);
  request->set_user_data(extra_info);  // takes pointer ownership

  BeginRequestInternal(request, request_data.mixed_content);
}

// We are explicitly forcing the download of 'url'.
void ResourceDispatcherHost::BeginDownload(const GURL& url,
                                           const GURL& referrer,
                                           int render_process_host_id,
                                           int render_view_id,
                                           URLRequestContext* request_context) {
  if (is_shutdown_)
    return;

  // Check if the renderer is permitted to request the requested URL.
  //
  // TODO(mpcomplete): remove "render_process_host_id != -1"
  //                   when http://b/viewIssue?id=1080959 is fixed.
  if (render_process_host_id != -1 &&
      !RendererSecurityPolicy::GetInstance()->
          CanRequestURL(render_process_host_id, url)) {
    LOG(INFO) << "Denied unauthorized download request for " <<
        url.possibly_invalid_spec();
    return;
  }

  // Ensure the Chrome plugins are loaded, as they may intercept network
  // requests.  Does nothing if they are already loaded.
  plugin_service_->LoadChromePlugins(this);
  URLRequest* request = new URLRequest(url, this);

  request_id_--;

  scoped_refptr<EventHandler> handler =
      new DownloadEventHandler(this,
                               render_process_host_id,
                               render_view_id,
                               request_id_,
                               url.spec(),
                               download_file_manager_.get(),
                               request,
                               true);


  if (safe_browsing_->enabled() && safe_browsing_->CanCheckUrl(url)) {
    handler = new SafeBrowsingEventHandler(handler,
                                           render_process_host_id,
                                           render_view_id,
                                           url,
                                           ResourceType::MAIN_FRAME,
                                           safe_browsing_,
                                           this);
  }

  bool known_proto = URLRequest::IsHandledURL(url);
  if (!known_proto) {
    CHECK(false);
  }

  request->set_method("GET");
  request->set_referrer(referrer.spec());
  request->set_context(request_context);

  ExtraRequestInfo* extra_info =
      new ExtraRequestInfo(handler,
                           request_id_,
                           render_process_host_id,
                           render_view_id,
                           false,  // Downloads are not considered mixed-content
                           ResourceType::SUB_RESOURCE,
                           0 /* upload_size */ );
  extra_info->allow_download = true;
  extra_info->is_download = true;
  request->set_user_data(extra_info);  // Takes pointer ownership.

  BeginRequestInternal(request, false);
}

// This function is only used for saving feature.
void ResourceDispatcherHost::BeginSaveFile(const GURL& url,
                                           const GURL& referrer,
                                           int render_process_host_id,
                                           int render_view_id,
                                           URLRequestContext* request_context) {
  if (is_shutdown_)
    return;

  // Ensure the Chrome plugins are loaded, as they may intercept network
  // requests.  Does nothing if they are already loaded.
  plugin_service_->LoadChromePlugins(this);

  scoped_refptr<EventHandler> handler =
      new SaveFileEventHandler(render_process_host_id,
                               render_view_id,
                               url.spec(),
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
                           request_id_,
                           render_process_host_id,
                           render_view_id,
                           false,
                           ResourceType::SUB_RESOURCE,
                           0 /* upload_size */);
  // Just saving some resources we need, disallow downloading.
  extra_info->allow_download = false;
  extra_info->is_download = false;
  request->set_user_data(extra_info);  // Takes pointer ownership.

  BeginRequestInternal(request, false);
}

void ResourceDispatcherHost::CancelRequest(int render_process_host_id,
                                           int request_id,
                                           bool from_renderer) {
  CancelRequest(render_process_host_id, request_id, from_renderer, true);
}

void ResourceDispatcherHost::CancelRequest(int render_process_host_id,
                                           int request_id,
                                           bool from_renderer,
                                           bool allow_delete) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(render_process_host_id, request_id));
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
      // TODO: removing the request in this manner means we're not notifying
      // anyone. We need make sure the event handlers and others are notified
      // so that everything is cleaned up properly.
      RemovePendingRequest(info->render_process_host_id, info->request_id);
    } else {
      i->second->Cancel();
    }
  }

  // Do not remove from the pending requests, as the request will still
  // call AllDataReceived, and may even have more data before it does
  // that.
}

void ResourceDispatcherHost::OnDataReceivedACK(int render_process_host_id,
                                               int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(render_process_host_id, request_id));
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
    PauseRequest(render_process_host_id, request_id, false);
  }
}

void ResourceDispatcherHost::OnUploadProgressACK(int render_process_host_id,
                                                 int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(render_process_host_id, request_id));
  if (i == pending_requests_.end())
    return;

  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);
  info->waiting_for_upload_progress_ack = false;
}

bool ResourceDispatcherHost::WillSendData(int render_process_host_id,
                                          int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(render_process_host_id, request_id));
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
    PauseRequest(render_process_host_id, request_id, true);
    return false;
  }

  return true;
}

void ResourceDispatcherHost::PauseRequest(int render_process_host_id,
                                          int request_id,
                                          bool pause) {
  GlobalRequestID global_id(render_process_host_id, request_id);
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

void ResourceDispatcherHost::OnClosePageACK(int render_process_host_id,
                                            int request_id) {
  GlobalRequestID global_id(render_process_host_id, request_id);
  PendingRequestList::iterator i = pending_requests_.find(global_id);
  if (i == pending_requests_.end()) {
    // If there are no matching pending requests, then this is not a
    // cross-site navigation and we are just closing the tab/browser.
    ui_loop_->PostTask(FROM_HERE, NewRunnableFunction(
        &RenderViewHost::ClosePageIgnoringUnloadEvents,
        render_process_host_id,
        request_id));
    return;
  }

  ExtraRequestInfo* info = ExtraInfoForRequest(i->second);
  if (info->cross_site_handler) {
    info->cross_site_handler->ResumeResponse();
  }
}

// The object died, so cancel and detach all requests associated with it except
// for downloads, which belong to the browser process even if initiated via a
// renderer.
void ResourceDispatcherHost::CancelRequestsForProcess(
    int render_process_host_id) {
  CancelRequestsForRenderView(render_process_host_id, -1 /* cancel all */);
}

void ResourceDispatcherHost::CancelRequestsForRenderView(
    int render_process_host_id,
    int render_view_id) {
  // Since pending_requests_ is a map, we first build up a list of all of the
  // matching requests to be cancelled, and then we cancel them.  Since there
  // may be more than one request to cancel, we cannot simply hold onto the map
  // iterators found in the first loop.

  // Find the global ID of all matching elements.
  std::vector<GlobalRequestID> matching_requests;
  for (PendingRequestList::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->first.render_process_host_id == render_process_host_id) {
      ExtraRequestInfo* info = ExtraInfoForRequest(i->second);
      if (!info->is_download && (render_view_id == -1 ||
                                 render_view_id == info->render_view_id)) {
        matching_requests.push_back(i->first);
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
}

// Cancels the request and removes it from the list.
void ResourceDispatcherHost::RemovePendingRequest(int render_process_host_id,
                                                  int request_id) {
  PendingRequestList::iterator i = pending_requests_.find(
      GlobalRequestID(render_process_host_id, request_id));
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Trying to remove a request that's not here";
    return;
  }
  RemovePendingRequest(i);
}

void ResourceDispatcherHost::RemovePendingRequest(
    const PendingRequestList::iterator& iter) {
  // Notify the login handler that this request object is going away.
  ExtraRequestInfo* info = ExtraInfoForRequest(iter->second);
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

  // TODO(mpcomplete): remove this when http://b/viewIssue?id=1080959 is fixed.
  if (info->render_process_host_id != -1 &&
      !RendererSecurityPolicy::GetInstance()->
          CanRequestURL(info->render_process_host_id, new_url)) {
    LOG(INFO) << "Denied unauthorized request for " <<
        new_url.possibly_invalid_spec();

    // Tell the renderer that this request was disallowed.
    CancelRequest(info->render_process_host_id, info->request_id, false);
    return;
  }

  NofityReceivedRedirect(request, info->render_process_host_id, new_url);

  if (HandleExternalProtocol(info->request_id, info->render_process_host_id,
                             info->render_view_id, new_url,
                             info->resource_type, info->event_handler)) {
    // The request is complete so we can remove it.
    RemovePendingRequest(info->render_process_host_id, info->request_id);
    return;
  }

  if (!info->event_handler->OnRequestRedirected(info->request_id, new_url))
    CancelRequest(info->render_process_host_id, info->request_id, false);
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
      CancelRequest(info->render_process_host_id, info->request_id, false);
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

  scoped_refptr<Response> response(new Response);

  response->response_head.status = request->status();
  response->response_head.request_time = request->request_time();
  response->response_head.response_time = request->response_time();
  response->response_head.headers = request->response_headers();
  request->GetCharset(&response->response_head.charset);
  response->response_head.filter_policy = info->filter_policy;
  response->response_head.content_length = request->GetExpectedContentSize();
  request->GetMimeType(&response->response_head.mime_type);

  if (request->ssl_info().cert) {
    int cert_id =
        CertStore::GetSharedInstance()->StoreCert(
            request->ssl_info().cert,
            info->render_process_host_id);
    int cert_status = request->ssl_info().cert_status;
    // EV certificate verification could be expensive.  We don't want to spend
    // time performing EV certificate verification on all resources because
    // EV status is irrelevant to sub-frames and sub-resources.  So we call
    // IsEV here rather than in the network layer because the network layer
    // doesn't know the resource type.
    if (info->resource_type == ResourceType::MAIN_FRAME &&
        request->ssl_info().cert->IsEV(cert_status))
      cert_status |= net::CERT_STATUS_IS_EV;

    response->response_head.security_info =
        SSLManager::SerializeSecurityInfo(cert_id,
                                          cert_status,
                                          request->ssl_info().security_bits);
  } else {
    // We should not have any SSL state.
    DCHECK(!request->ssl_info().cert_status &&
           (request->ssl_info().security_bits == -1 ||
           request->ssl_info().security_bits == 0));
  }

  NotifyResponseStarted(request, info->render_process_host_id);
  return info->event_handler->OnResponseStarted(info->request_id,
                                                response.get());
}

void ResourceDispatcherHost::BeginRequestInternal(URLRequest* request,
                                                  bool mixed_content) {
  ExtraRequestInfo* info = ExtraInfoForRequest(request);
  GlobalRequestID global_id(info->render_process_host_id, info->request_id);
  pending_requests_[global_id] = request;
  if (mixed_content) {
    // We don't start the request in that case.  The SSLManager will potentially
    // change the request (potentially to indicate its content should be
    // filtered) and start it itself.
    SSLManager::OnMixedContentRequest(this, request, ui_loop_);
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

  char* buf;
  int buf_size;
  if (!info->event_handler->OnWillRead(info->request_id, &buf, &buf_size, -1))
    return false;

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
        GlobalRequestID id(info->render_process_host_id, info->request_id);
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

  if (!info->event_handler->OnReadCompleted(info->request_id, bytes_read)) {
    // Pass in false as the last arg to indicate we don't want |request|
    // deleted. We do this as callers of us assume |request| is valid after we
    // return.
    CancelRequest(info->render_process_host_id, info->request_id, false, false);
    return false;
  }

  return *bytes_read != 0;
}

void ResourceDispatcherHost::OnResponseCompleted(URLRequest* request) {
  RESOURCE_LOG("OnResponseCompleted: " << request->url().spec());
  ExtraRequestInfo* info = ExtraInfoForRequest(request);

  if (info->event_handler->OnResponseCompleted(info->request_id,
                                               request->status())) {
    NotifyResponseCompleted(request, info->render_process_host_id);

    // The request is complete so we can remove it.
    RemovePendingRequest(info->render_process_host_id, info->request_id);
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
    if (!tab_util::GetTabContentsID(request,
                                    &render_process_host_id_,
                                    &tab_contents_id_))
      NOTREACHED();
  }

  void Run() {
    // Find the tab associated with this request.
    TabContents* tab_contents =
        tab_util::GetTabContentsByID(render_process_host_id_, tab_contents_id_);

    if (tab_contents) {
      // Issue the notification.
      NotificationService::current()->
          Notify(type_,
                 Source<NavigationController>(tab_contents->controller()),
                 Details<ResourceRequestDetails>(details_.get()));
    }
  }

 private:
  // These IDs let us find the correct tab on the UI thread.
  int render_process_host_id_;
  int tab_contents_id_;

  // The type and details of the notification.
  NotificationType type_;
  scoped_ptr<ResourceRequestDetails> details_;
};

static int GetCertID(URLRequest* request, int render_process_host_id) {
  if (request->ssl_info().cert) {
    return CertStore::GetSharedInstance()->StoreCert(request->ssl_info().cert,
                                                     render_process_host_id);
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
                                                   int render_process_host_id) {
  // Notify the observers on the IO thread.
  FOR_EACH_OBSERVER(Observer, observer_list_, OnRequestStarted(this, request));

  // Notify the observers on the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      new NotificationTask(NOTIFY_RESOURCE_RESPONSE_STARTED, request,
                           new ResourceRequestDetails(request,
                               GetCertID(request, render_process_host_id))));
}

void ResourceDispatcherHost::NotifyResponseCompleted(
    URLRequest* request,
    int render_process_host_id) {
  // Notify the observers on the IO thread.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnResponseCompleted(this, request));

  // Notify the observers on the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      new NotificationTask(NOTIFY_RESOURCE_RESPONSE_COMPLETED, request,
                           new ResourceRequestDetails(request,
                               GetCertID(request, render_process_host_id))));
}

void ResourceDispatcherHost::NofityReceivedRedirect(URLRequest* request,
                                                    int render_process_host_id,
                                                    const GURL& new_url) {
  // Notify the observers on the IO thread.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnReceivedRedirect(this, request, new_url));

  int cert_id = GetCertID(request, render_process_host_id);

  // Notify the observers on the UI thread.
  ui_loop_->PostTask(FROM_HERE,
      new NotificationTask(NOTIFY_RESOURCE_RECEIVED_REDIRECT, request,
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

      std::pair<int, int> key(info->render_process_host_id,
                              info->render_view_id);
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
    info->event_handler->OnUploadProgress(info->request_id, position, size);
    info->waiting_for_upload_progress_ack = true;
    info->last_upload_ticks = TimeTicks::Now();
    info->last_upload_position = position;
  }
}
