// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H__
#define CHROME_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H__

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

class LoadFromMemoryCacheDetails {
 public:
   LoadFromMemoryCacheDetails(
       const GURL& url,
       const std::string& frame_origin,
       const std::string& main_frame_origin,
       int cert_id,
       int cert_status)
       : url_(url),
         frame_origin_(frame_origin),
         main_frame_origin_(main_frame_origin_),
         cert_id_(cert_id),
         cert_status_(cert_status)
  { }

  ~LoadFromMemoryCacheDetails() { }

  const GURL& url() const { return url_; }
  const std::string& frame_origin() const { return frame_origin_; }
  const std::string& main_frame_origin() const { return main_frame_origin_; }
  int ssl_cert_id() const { return cert_id_; }
  int ssl_cert_status() const { return cert_status_; }

private:
  GURL url_;
  std::string frame_origin_;
  std::string main_frame_origin_;
  int cert_id_;
  int cert_status_;

  DISALLOW_EVIL_CONSTRUCTORS(LoadFromMemoryCacheDetails);
};

#endif  // CHROME_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H__
