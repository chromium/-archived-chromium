// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/tab_contents/provisional_load_details.h"

#include "chrome/browser/ssl/ssl_manager.h"

ProvisionalLoadDetails::ProvisionalLoadDetails(bool is_main_frame,
                                               bool is_in_page_navigation,
                                               const GURL& url,
                                               const std::string& security_info,
                                               bool is_content_filtered)
      : error_code_(net::OK),
        url_(url),
        is_main_frame_(is_main_frame),
        is_in_page_navigation_(is_in_page_navigation),
        is_content_filtered_(is_content_filtered) {
  SSLManager::DeserializeSecurityInfo(security_info,
                                      &ssl_cert_id_,
                                      &ssl_cert_status_,
                                      &ssl_security_bits_);
}
