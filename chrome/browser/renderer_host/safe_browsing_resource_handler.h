// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_HANDLER_H_
#define CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_HANDLER_H_

#include <string>

#include "base/time.h"
#include "chrome/browser/renderer_host/resource_handler.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"

class ResourceDispatcherHost;

// Checks that a url is safe.
class SafeBrowsingResourceHandler : public ResourceHandler,
                                    public SafeBrowsingService::Client {
 public:
  SafeBrowsingResourceHandler(ResourceHandler* handler,
                              int render_process_host_id,
                              int render_view_id,
                              const GURL& url,
                              ResourceType::Type resource_type,
                              SafeBrowsingService* safe_browsing,
                              ResourceDispatcherHost* resource_dispatcher_host);

  // ResourceHandler implementation:
  bool OnUploadProgress(int request_id, uint64 position, uint64 size);
  bool OnRequestRedirected(int request_id, const GURL& new_url);
  bool OnResponseStarted(int request_id, ResourceResponse* response);
  void OnGetHashTimeout();
  bool OnWillRead(int request_id, net::IOBuffer** buf, int* buf_size,
                  int min_size);
  bool OnReadCompleted(int request_id, int* bytes_read);
  bool OnResponseCompleted(int request_id,
                           const URLRequestStatus& status,
                           const std::string& security_info);

  // SafeBrowsingService::Client implementation, called on the IO thread once
  // the URL has been classified.
  void OnUrlCheckResult(const GURL& url,
                        SafeBrowsingService::UrlCheckResult result);

  // SafeBrowsingService::Client implementation, called on the IO thread when
  // the user has decided to proceed with the current request, or go back.
  void OnBlockingPageComplete(bool proceed);

 private:
  scoped_refptr<ResourceHandler> next_handler_;
  int render_process_host_id_;
  int render_view_id_;
  int paused_request_id_;  // -1 if not paused
  bool in_safe_browsing_check_;
  bool displaying_blocking_page_;
  SafeBrowsingService::UrlCheckResult safe_browsing_result_;
  scoped_refptr<SafeBrowsingService> safe_browsing_;
  scoped_ptr<URLRequestStatus> queued_error_;
  std::string queued_security_info_;
  int queued_error_request_id_;
  ResourceDispatcherHost* rdh_;
  base::Time pause_time_;
  ResourceType::Type resource_type_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingResourceHandler);
};


#endif  // CHROME_BROWSER_RENDERER_HOST_SAFE_BROWSING_RESOURCE_HANDLER_H_
