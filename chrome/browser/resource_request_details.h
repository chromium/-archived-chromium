// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The ResourceRequestDetails object contains additional details about a
// resource request.  It copies many of the publicly accessible member variables
// of URLRequest, but exists on the UI thread.

#ifndef CHROME_BROWSER_RESOURCE_REQUEST_DETAILS_H__
#define CHROME_BROWSER_RESOURCE_REQUEST_DETAILS_H__

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/cert_store.h"
#include "chrome/browser/resource_dispatcher_host.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

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
  }

  ~ResourceRequestDetails() { }

  const GURL& url() const { return url_; }
  const GURL& original_url() const { return original_url_; }
  const std::string& method() const { return method_; }
  const std::string& referrer() const { return referrer_; }
  bool has_upload() const { return has_upload_; }
  int load_flags() const { return load_flags_; }
  int origin_pid() const { return origin_pid_; }
  const URLRequestStatus& status() const { return status_; }
  int ssl_cert_id() const { return ssl_cert_id_; }
  int ssl_cert_status() const { return ssl_cert_status_; }
  ResourceType::Type resource_type() const { return resource_type_; }
 private:
  GURL url_;
  GURL original_url_;
  std::string method_;
  std::string referrer_;
  bool has_upload_;
  int load_flags_;
  int origin_pid_;
  URLRequestStatus status_;
  int ssl_cert_id_;
  int ssl_cert_status_;
  ResourceType::Type resource_type_;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceRequestDetails);
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

#endif  // CHROME_BROWSER_RESOURCE_REQUEST_DETAILS_H__
