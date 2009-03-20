// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ResourceRequestDetails object contains additional details about a
// resource request.  It copies many of the publicly accessible member variables
// of URLRequest, but exists on the UI thread.

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_DETAILS_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_DETAILS_H_

#include <string>

#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

class URLRequest;

#if defined(OS_WIN)
// TODO(port): Move header to the above section when CertStore has been ported.
#include "chrome/browser/cert_store.h"
#elif defined(OS_POSIX)
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// Details about a resource request notification.
class ResourceRequestDetails {
 public:
  ResourceRequestDetails(const URLRequest* request, int cert_id)
      : url_(request->url()),
        original_url_(request->original_url()),
        method_(request->method()),
        referrer_(request->referrer()),
        has_upload_(request->has_upload()),
        load_flags_(request->load_flags()),
        origin_pid_(request->origin_pid()),
        status_(request->status()),
        ssl_cert_id_(cert_id),
        ssl_cert_status_(request->ssl_info().cert_status) {
    const ResourceDispatcherHost::ExtraRequestInfo* info =
        ResourceDispatcherHost::ExtraInfoForRequest(request);
    DCHECK(info);
    resource_type_ = info->resource_type;
    frame_origin_ = info->frame_origin;
    main_frame_origin_ = info->main_frame_origin;
    filter_policy_ = info->filter_policy;
  }

  ~ResourceRequestDetails() { }

  const GURL& url() const { return url_; }
  const GURL& original_url() const { return original_url_; }
  const std::string& method() const { return method_; }
  const std::string& referrer() const { return referrer_; }
  const std::string& frame_origin() const { return frame_origin_; }
  const std::string& main_frame_origin() const { return main_frame_origin_; }
  bool has_upload() const { return has_upload_; }
  int load_flags() const { return load_flags_; }
  int origin_pid() const { return origin_pid_; }
  const URLRequestStatus& status() const { return status_; }
  int ssl_cert_id() const { return ssl_cert_id_; }
  int ssl_cert_status() const { return ssl_cert_status_; }
  ResourceType::Type resource_type() const { return resource_type_; }
  FilterPolicy::Type filter_policy() const { return filter_policy_; }

 private:
  GURL url_;
  GURL original_url_;
  std::string method_;
  std::string referrer_;
  std::string frame_origin_;
  std::string main_frame_origin_;
  bool has_upload_;
  int load_flags_;
  int origin_pid_;
  URLRequestStatus status_;
  int ssl_cert_id_;
  int ssl_cert_status_;
  ResourceType::Type resource_type_;
  FilterPolicy::Type filter_policy_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestDetails);
};

// Details about a redirection of a resource request.
class ResourceRedirectDetails : public ResourceRequestDetails {
 public:
  ResourceRedirectDetails(const URLRequest* request,
                          int cert_id,
                          const GURL& new_url)
      : ResourceRequestDetails(request, cert_id),
        new_url_(new_url) { }

  // The URL to which we are being redirected.
  const GURL& new_url() const { return new_url_; }

 private:
  GURL new_url_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_REQUEST_DETAILS_H_
