// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the RenderProcessHosts, and dispatches them to URLRequests. It then
// fowards the messages from the URLRequests back to the correct process for
// handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CHROME_BROWSER_RESOURCE_DISPATCHER_HOST_H__
#define CHROME_BROWSER_RESOURCE_DISPATCHER_HOST_H__

#include <map>
#include <string>

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "base/timer.h"
#include "chrome/common/filter_policy.h"
#include "chrome/common/ipc_message.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

class DownloadFileManager;
class DownloadRequestManager;
class LoginHandler;
class MessageLoop;
class PluginService;
class SafeBrowsingService;
class SaveFileManager;
class TabContents;
class URLRequestContext;
struct ViewHostMsg_Resource_Request;
struct ViewMsg_Resource_ResponseHead;

class ResourceDispatcherHost : public URLRequest::Delegate {
 public:
  // Simple wrapper that refcounts ViewMsg_Resource_ResponseHead.
  struct Response;

  // The resource dispatcher host uses this interface to push load events to the
  // renderer, allowing for differences in the types of IPC messages generated.
  // See the implementations of this interface defined below.
  class EventHandler : public base::RefCounted<EventHandler> {
   public:
    virtual ~EventHandler() {}

    // Called as upload progress is made.
    virtual bool OnUploadProgress(int request_id,
                                  uint64 position,
                                  uint64 size) {
      return true;
    }

    // The request was redirected to a new URL.
    virtual bool OnRequestRedirected(int request_id, const GURL& url) = 0;

    // Response headers and meta data are available.
    virtual bool OnResponseStarted(int request_id, Response* response) = 0;

    // Data will be read for the response.  Upon success, this method places the
    // size and address of the buffer where the data is to be written in its
    // out-params.  This call will be followed by either OnReadCompleted or
    // OnResponseCompleted, at which point the buffer may be recycled.
    virtual bool OnWillRead(int request_id,
                            char** buf,
                            int* buf_size,
                            int min_size) = 0;

    // Data (*bytes_read bytes) was written into the buffer provided by
    // OnWillRead. A return value of false cancels the request, true continues
    // reading data.
    virtual bool OnReadCompleted(int request_id, int* bytes_read) = 0;

    // The response is complete.  The final response status is given.
    // Returns false if the handler is deferring the call to a later time.
    virtual bool OnResponseCompleted(int request_id,
                                     const URLRequestStatus& status) = 0;
  };

  // Implemented by the client of ResourceDispatcherHost to receive messages in
  // response to a resource load.  The messages are intended to be forwarded to
  // the ResourceDispatcher in the renderer process via an IPC channel that the
  // client manages.
  //
  // NOTE: This class unfortunately cannot be named 'Delegate' because that
  // conflicts with the name of ResourceDispatcherHost's base class.
  //
  // If the receiver is unable to send a given message (i.e., if Send returns
  // false), then the ResourceDispatcherHost assumes the receiver has failed,
  // and the given request will be dropped. (This happens, for example, when a
  // renderer crashes and the channel dies).
  typedef IPC::Message::Sender Receiver;

  // Forward declaration of CrossSiteEventHandler, so that it can be referenced
  // in ExtraRequestInfo.
  class CrossSiteEventHandler;

  // Holds the data we would like to associate with each request
  class ExtraRequestInfo : public URLRequest::UserData {
   friend class ResourceDispatcherHost;
   public:
    ExtraRequestInfo(EventHandler* handler,
                     int request_id,
                     int render_process_host_id,
                     int render_view_id,
                     bool mixed_content,
                     ResourceType::Type resource_type,
                     uint64 upload_size)
        : event_handler(handler),
          cross_site_handler(NULL),
          login_handler(NULL),
          request_id(request_id),
          render_process_host_id(render_process_host_id),
          render_view_id(render_view_id),
          is_download(false),
          pause_count(0),
          mixed_content(mixed_content),
          resource_type(resource_type),
          filter_policy(FilterPolicy::DONT_FILTER),
          last_load_state(net::LOAD_STATE_IDLE),
          pending_data_count(0),
          upload_size(upload_size),
          last_upload_position(0),
          waiting_for_upload_progress_ack(false),
          is_paused(false),
          has_started_reading(false),
          paused_read_bytes(0) {
    }

    // Top-level EventHandler servicing this request.
    scoped_refptr<EventHandler> event_handler;

    // CrossSiteEventHandler for this request, if it is a cross-site request.
    // (NULL otherwise.)  This handler is part of the chain of EventHandlers
    // pointed to by event_handler.
    CrossSiteEventHandler* cross_site_handler;

    LoginHandler* login_handler;

    int request_id;

    int render_process_host_id;

    int render_view_id;

    int pending_data_count;

    // Downloads allowed only as a top level request.
    bool allow_download;

    // Whether this is a download.
    bool is_download;

    // The number of clients that have called pause on this request.
    int pause_count;

    // Whether this request is served over HTTP and the main page was served
    // over HTTPS.
    bool mixed_content;

    ResourceType::Type resource_type;

    // Whether the content for this request should be filtered (on the renderer
    // side) to make it more secure: images are stamped, frame content is
    // replaced with an error message and all other resources are entirely
    // filtered out.
    FilterPolicy::Type filter_policy;

    net::LoadState last_load_state;

    uint64 upload_size;

    uint64 last_upload_position;

    base::TimeTicks last_upload_ticks;

    bool waiting_for_upload_progress_ack;

   private:
    // Request is temporarily not handling network data. Should be used only
    // by the ResourceDispatcherHost, not the event handlers.
    bool is_paused;

    // Whether this request has started reading any bytes from the response
    // yet.  Will be true after the first (unpaused) call to Read.
    bool has_started_reading;

    // How many bytes have been read while this request has been paused.
    int paused_read_bytes;
  };

  class Observer {
   public:
    virtual void OnRequestStarted(ResourceDispatcherHost* resource_dispatcher,
                                  URLRequest* request) = 0;
    virtual void OnResponseCompleted(ResourceDispatcherHost* resource_dispatcher,
                                     URLRequest* request) = 0;
    virtual void OnReceivedRedirect(ResourceDispatcherHost* resource_dispatcher,
                                    URLRequest* request,
                                    const GURL& new_url) = 0;
  };

  // Uniquely identifies a URLRequest.
  struct GlobalRequestID {
    GlobalRequestID() : render_process_host_id(-1), request_id(-1) {
    }
    GlobalRequestID(int render_process_host_id, int request_id)
        : render_process_host_id(render_process_host_id),
          request_id(request_id) {
    }

    int render_process_host_id;
    int request_id;

    bool operator<(const GlobalRequestID& other) const {
      if (render_process_host_id == other.render_process_host_id)
        return request_id < other.request_id;
      return render_process_host_id < other.render_process_host_id;
    }
  };

  explicit ResourceDispatcherHost(MessageLoop* io_loop);
  ~ResourceDispatcherHost();

  void Initialize();

  // Puts the resource dispatcher host in an inactive state (unable to begin
  // new requests).  Cancels all pending requests.
  void Shutdown();

  // Begins a resource request with the given params on behalf of the specified
  // render view.  Responses will be dispatched through the given receiver. The
  // RenderProcessHost ID is used to lookup TabContents from routing_id's.
  // request_context is the cookie/cache context to be used for this request.
  //
  // If sync_result is non-null, then a SyncLoad reply will be generated, else
  // a normal asynchronous set of response messages will be generated.
  //
  void BeginRequest(Receiver* receiver,
                    HANDLE render_process_handle,
                    int render_process_host_id,
                    int render_view_id,
                    int request_id,
                    const ViewHostMsg_Resource_Request& request,
                    URLRequestContext* request_context,
                    IPC::Message* sync_result);

  // Initiate a download from the browser process (as opposed to a resource
  // request from the renderer).
  void BeginDownload(const GURL& url,
                     const GURL& referrer,
                     int render_process_host_id,
                     int render_view_id,
                     URLRequestContext* request_context);

  // Initiate a save file from the browser process (as opposed to a resource
  // request from the renderer).
  void BeginSaveFile(const GURL& url,
                     const GURL& referrer,
                     int render_process_host_id,
                     int render_view_id,
                     URLRequestContext* request_context);

  // Cancels the given request if it still exists. We ignore cancels from the
  // renderer in the event of a download.
  void CancelRequest(int render_process_host_id,
                     int request_id,
                     bool from_renderer);

  // Decrements the pending_data_count for the request and resumes
  // the request if it was paused due to too many pending data
  // messages sent.
  void OnDataReceivedACK(int render_process_host_id, int request_id);

  // Resets the waiting_for_upload_progress_ack flag.
  void OnUploadProgressACK(int render_process_host_id, int request_id);

  // Returns true if it's ok to send the data. If there are already too many
  // data messages pending, it pauses the request and returns false. In this
  // case the caller should not send the data.
  bool WillSendData(int render_process_host_id, int request_id);

  // Pauses or resumes network activity for a particular request.
  void PauseRequest(int render_process_host_id, int request_id, bool pause);

  // Returns the number of pending requests. This is designed for the unittests
  int pending_requests() const {
    return static_cast<int>(pending_requests_.size());
  }

  DownloadFileManager* download_file_manager() const {
    return download_file_manager_;
  }

  DownloadRequestManager* download_request_manager() const {
    return download_request_manager_.get();
  }

  SaveFileManager* save_file_manager() const {
    return save_file_manager_;
  }

  SafeBrowsingService* safe_browsing_service() const {
    return safe_browsing_;
  }

  MessageLoop* ui_loop() const { return ui_loop_; }

  // Called when the onunload handler for a cross-site request has finished.
  void OnClosePageACK(int render_process_host_id, int request_id);

  // Force cancels any pending requests for the given process.
  void CancelRequestsForProcess(int render_process_host_id);

  // Force cancels any pending requests for the given render view.  This method
  // acts like CancelRequestsForProcess when render_view_id is -1.
  void CancelRequestsForRenderView(int render_process_host_id,
                                   int render_view_id);

  // URLRequest::Delegate
  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url);
  virtual void OnAuthRequired(URLRequest* request,
                              net::AuthChallengeInfo* auth_info);
  virtual void OnSSLCertificateError(URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert);
  virtual void OnResponseStarted(URLRequest* request);
  virtual void OnReadCompleted(URLRequest* request, int bytes_read);
  void OnResponseCompleted(URLRequest* request);

  // Helper function to get our extra data out of a request. The given request
  // must have been one we created so that it has the proper extra data pointer.
  static ExtraRequestInfo* ExtraInfoForRequest(URLRequest* request) {
    ExtraRequestInfo* r = static_cast<ExtraRequestInfo*>(request->user_data());
    DLOG_IF(WARNING, !r) << "Request doesn't seem to have our data";
    return r;
  }

  static const ExtraRequestInfo* ExtraInfoForRequest(const URLRequest* request) {
    const ExtraRequestInfo* r =
        static_cast<const ExtraRequestInfo*>(request->user_data());
    DLOG_IF(WARNING, !r) << "Request doesn't seem to have our data";
    return r;
  }

  // Add an observer.  The observer will be called on the IO thread.  To
  // observe resource events on the UI thread, subscribe to the
  // NOTIFY_RESOURCE_* notifications of the notification service.
  void AddObserver(Observer* obs);

  // Remove an observer.
  void RemoveObserver(Observer* obs);

  // Retrieves a URLRequest.  Must be called from the IO thread.
  URLRequest* GetURLRequest(GlobalRequestID request_id) const;

 private:
  class AsyncEventHandler;
  class BufferedEventHandler;
  class CrossSiteNotifyTabTask;
  class DownloadEventHandler;
  class DownloadThrottlingEventHandler;
  class SaveFileEventHandler;
  class ShutdownTask;
  class SyncEventHandler;

  friend class ShutdownTask;

  // A shutdown helper that runs on the IO thread.
  void OnShutdown();

  // Returns true if the request is paused.
  bool PauseRequestIfNeeded(ExtraRequestInfo* info);

  // Resumes the given request by calling OnResponseStarted or OnReadCompleted.
  void ResumeRequest(const GlobalRequestID& request_id);

  // Reads data from the response using our internal buffer as async IO.
  // Returns true if data is available immediately, false otherwise.  If the
  // return value is false, we will receive a OnReadComplete() callback later.
  bool Read(URLRequest* request, int* bytes_read);

  // Internal function to finish an async IO which has completed.  Returns
  // true if there is more data to read (e.g. we haven't read EOF yet and
  // no errors have occurred).
  bool CompleteRead(URLRequest *, int* bytes_read);

  // Internal function to finish handling the ResponseStarted message.  Returns
  // true on success.
  bool CompleteResponseStarted(URLRequest* request);

  // Cancels the given request if it still exists. We ignore cancels from the
  // renderer in the event of a download. If |allow_delete| is true and no IO
  // is pending, the request is removed and deleted.
  void CancelRequest(int render_process_host_id,
                     int request_id,
                     bool from_renderer,
                     bool allow_delete);

  // Helper function for regular and download requests.
  void BeginRequestInternal(URLRequest* request, bool mixed_content);

  // The list of all requests that we have pending. This list is not really
  // optimized, and assumes that we have relatively few requests pending at once
  // since some operations require brute-force searching of the list.
  //
  // It may be enhanced in the future to provide some kind of prioritization
  // mechanism. We should also consider a hashtable or binary tree if it turns
  // out we have a lot of things here.
  typedef std::map<GlobalRequestID,URLRequest*> PendingRequestList;

  // A test to determining whether a given request should be forwarded to the
  // download thread.
  bool ShouldDownload(const std::string& mime_type,
                      const std::string& content_disposition);

  void RemovePendingRequest(int render_process_host_id, int request_id);
  // Deletes the pending request identified by the iterator passed in.
  // This function will invalidate the iterator passed in. Callers should
  // not rely on this iterator being valid on return.
  void RemovePendingRequest(const PendingRequestList::iterator& iter);

  // Notify our observers that we started receiving a response for a request.
  void NotifyResponseStarted(URLRequest* request, int render_process_host_id);

  // Notify our observers that a request has been cancelled.
  void NotifyResponseCompleted(URLRequest* request, int render_process_host_id);

  // Notify our observers that a request has been redirected.
  void NofityReceivedRedirect(URLRequest* request,
                              int render_process_host_id,
                              const GURL& new_url);

  // Tries to handle the url with an external protocol. If the request is
  // handled, the function returns true. False otherwise.
  bool HandleExternalProtocol(int request_id,
                              int render_process_host_id,
                              int tab_contents_id,
                              const GURL& url,
                              ResourceType::Type resource_type,
                              EventHandler* handler);

  void UpdateLoadStates();

  void MaybeUpdateUploadProgress(ExtraRequestInfo *info, URLRequest *request);

  PendingRequestList pending_requests_;

  // We cache the UI message loop so we can create new UI-related objects on it.
  MessageLoop* ui_loop_;

  // We cache the IO loop to ensure that GetURLRequest is only called from the
  // IO thread.
  MessageLoop* io_loop_;

  // A timer that periodically calls UpdateLoadStates while pending_requests_
  // is not empty.
  base::RepeatingTimer<ResourceDispatcherHost> update_load_states_timer_;

  // We own the download file writing thread and manager
  scoped_refptr<DownloadFileManager> download_file_manager_;

  // Determines whether a download is allowed.
  scoped_refptr<DownloadRequestManager> download_request_manager_;

  // We own the save file manager.
  scoped_refptr<SaveFileManager> save_file_manager_;

  scoped_refptr<SafeBrowsingService> safe_browsing_;

  // Request ID for non-renderer initiated requests. request_ids generated by
  // the renderer process are counted up from 0, while browser created requests
  // start at -2 and go down from there. (We need to start at -2 because -1 is
  // used as a special value all over the resource_dispatcher_host for
  // uninitialized variables.) This way, we no longer have the unlikely (but
  // observed in the real world!) event where we have two requests with the same
  // request_id_.
  int request_id_;

  // List of objects observing resource dispatching.
  ObserverList<Observer> observer_list_;

  PluginService* plugin_service_;

  // For running tasks.
  ScopedRunnableMethodFactory<ResourceDispatcherHost> method_runner_;

  // True if the resource dispatcher host has been shut down.
  bool is_shutdown_;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceDispatcherHost);
};

#endif  // CHROME_BROWSER_RESOURCE_DISPATCHER_HOST_H__
