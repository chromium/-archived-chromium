// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/filter_policy.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/resource_type.h"

class MessageLoop;
class SSLCertErrorHandler;
class TabContents;
class URLRequest;

// An SSLErrorHandler carries information from the IO thread to the UI thread
// and is dispatched to the appropriate SSLManager when it arrives on the
// UI thread.  Subclasses should override the OnDispatched/OnDispatchFailed
// methods to implement the actions that should be taken on the UI thread.
// These methods can call the different convenience methods ContinueRequest/
// CancelRequest/StartRequest to perform any required action on the URLRequest
// the ErrorHandler was created with.
//
// IMPORTANT NOTE:
//
//   If you are not doing anything in OnDispatched/OnDispatchFailed, make sure
//   you call TakeNoAction().  This is necessary for ensuring the instance is
//   not leaked.
//
class SSLErrorHandler : public base::RefCountedThreadSafe<SSLErrorHandler> {
 public:
  virtual ~SSLErrorHandler() { }

  virtual SSLCertErrorHandler* AsSSLCertErrorHandler() { return NULL; }

  // Find the appropriate SSLManager for the URLRequest and begin handling
  // this error.
  //
  // Call on UI thread.
  void Dispatch();

  // Available on either thread.
  const GURL& request_url() const { return request_url_; }

  // Available on either thread.
  ResourceType::Type resource_type() const { return resource_type_; }

  // Available on either thread.
  const std::string& frame_origin() const { return frame_origin_; }

  // Available on either thread.
  const std::string& main_frame_origin() const { return main_frame_origin_; }

  // Returns the TabContents this object is associated with.  Should be
  // called from the UI thread.
  TabContents* GetTabContents();

  // Cancels the associated URLRequest.
  // This method can be called from OnDispatchFailed and OnDispatched.
  void CancelRequest();

  // Continue the URLRequest ignoring any previous errors.  Note that some
  // errors cannot be ignored, in which case this will result in the request
  // being canceled.
  // This method can be called from OnDispatchFailed and OnDispatched.
  void ContinueRequest();

  // Cancels the associated URLRequest and mark it as denied.  The renderer
  // processes such request in a special manner, optionally replacing them
  // with alternate content (typically frames content is replaced with a
  // warning message).
  // This method can be called from OnDispatchFailed and OnDispatched.
  void DenyRequest();

  // Starts the associated URLRequest.  |filter_policy| specifies whether the
  // ResourceDispatcher should attempt to filter the loaded content in order
  // to make it secure (ex: images are made slightly transparent and are
  // stamped).
  // Should only be called when the URLRequest has not already been started.
  // This method can be called from OnDispatchFailed and OnDispatched.
  void StartRequest(FilterPolicy::Type filter_policy);

  // Does nothing on the URLRequest but ensures the current instance ref
  // count is decremented appropriately.  Subclasses that do not want to
  // take any specific actions in their OnDispatched/OnDispatchFailed should
  // call this.
  void TakeNoAction();

 protected:
  // Construct on the IO thread.
  SSLErrorHandler(ResourceDispatcherHost* resource_dispatcher_host,
                  URLRequest* request,
                  ResourceType::Type resource_type,
                  const std::string& frame_origin,
                  const std::string& main_frame_origin,
                  MessageLoop* ui_loop);

  // The following 2 methods are the methods subclasses should implement.
  virtual void OnDispatchFailed() { TakeNoAction(); }

  // Can use the manager_ member.
  virtual void OnDispatched() { TakeNoAction(); }

  // We cache the message loops to be able to proxy events across the thread
  // boundaries.
  MessageLoop* ui_loop_;
  MessageLoop* io_loop_;

  // Should only be accessed on the UI thread.
  SSLManager* manager_;  // Our manager.

  // The id of the URLRequest associated with this object.
  // Should only be accessed from the IO thread.
  ResourceDispatcherHost::GlobalRequestID request_id_;

  // The ResourceDispatcherHost we are associated with.
  ResourceDispatcherHost* resource_dispatcher_host_;

 private:
  // Completes the CancelRequest operation on the IO thread.
  // Call on the IO thread.
  void CompleteCancelRequest(int error);

  // Completes the ContinueRequest operation on the IO thread.
  //
  // Call on the IO thread.
  void CompleteContinueRequest();

  // Completes the StartRequest operation on the IO thread.
  // Call on the IO thread.
  void CompleteStartRequest(FilterPolicy::Type filter_policy);

  // Derefs this instance.
  // Call on the IO thread.
  void CompleteTakeNoAction();

  // We use these members to find the correct SSLManager when we arrive on
  // the UI thread.
  int render_process_host_id_;
  int tab_contents_id_;

  // The URL that we requested.
  // This read-only member can be accessed on any thread.
  const GURL request_url_;

  // What kind of resource is associated with the requested that generated
  // that error.
  // This read-only member can be accessed on any thread.
  const ResourceType::Type resource_type_;

  // The origin of the frame associated with this request.
  // This read-only member can be accessed on any thread.
  const std::string frame_origin_;

  // The origin of the main frame associated with this request.
  // This read-only member can be accessed on any thread.
  const std::string main_frame_origin_;

  // A flag to make sure we notify the URLRequest exactly once.
  // Should only be accessed on the IO thread
  bool request_has_been_notified_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
