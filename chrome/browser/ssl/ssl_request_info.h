// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_REQUEST_INFO_H_
#define CHROME_BROWSER_SSL_SSL_REQUEST_INFO_H_

#include <string>

#include "chrome/common/filter_policy.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/resource_type.h"

// SSLRequestInfo wraps up the information SSLPolicy needs about a request in
// order to update our security IU.  SSLRequestInfo is RefCounted in case we
// need to deal with the request asynchronously.
class SSLRequestInfo : public base::RefCounted<SSLRequestInfo> {
 public:
  SSLRequestInfo(const GURL& url,
                 ResourceType::Type resource_type,
                 const std::string& frame_origin,
                 const std::string& main_frame_origin,
                 FilterPolicy::Type filter_policy,
                 int pid,
                 int ssl_cert_id,
                 int ssl_cert_status)
      : url_(url),
        resource_type_(resource_type),
        frame_origin_(frame_origin),
        main_frame_origin_(main_frame_origin),
        filter_policy_(filter_policy),
        pid_(pid),
        ssl_cert_id_(ssl_cert_id),
        ssl_cert_status_(ssl_cert_status) {
  }

  const GURL& url() const { return url_; }
  ResourceType::Type resource_type() const { return resource_type_; }
  const std::string& frame_origin() const { return frame_origin_; }
  const std::string& main_frame_origin() const { return main_frame_origin_; }
  FilterPolicy::Type filter_policy() const { return filter_policy_; }
  int pid() const { return pid_; }
  int ssl_cert_id() const { return ssl_cert_id_; }
  int ssl_cert_status() const { return ssl_cert_status_; }

 private:
  GURL url_;
  ResourceType::Type resource_type_;
  std::string frame_origin_;
  std::string main_frame_origin_;
  FilterPolicy::Type filter_policy_;
  int pid_;
  int ssl_cert_id_;
  int ssl_cert_status_;

  DISALLOW_COPY_AND_ASSIGN(SSLRequestInfo);
};

#endif  // CHROME_BROWSER_SSL_SSL_REQUEST_INFO_H_
