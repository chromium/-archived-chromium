// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROVISIONAL_LOAD_DETAILS_H__
#define CHROME_BROWSER_PROVISIONAL_LOAD_DETAILS_H__

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

// This class captures some of the information associated to the provisional
// load of a frame.  It is provided as Details with the
// NOTIFY_FRAME_PROVISIONAL_LOAD_START, NOTIFY_FRAME_PROVISIONAL_LOAD_COMMITTED
// and NOTIFY_FAIL_PROVISIONAL_LOAD_WITH_ERROR notifications
// (see notification_types.h).

// TODO(brettw) this mostly duplicates
// NavigationController::LoadCommittedDetails, it would be nice to unify these
// somehow.
class ProvisionalLoadDetails {
 public:
  ProvisionalLoadDetails(bool main_frame,
                         bool interstitial_page,
                         bool in_page_navigation,
                         const GURL& url,
                         const std::string& security_info,
                         bool is_filtered);
  virtual ~ProvisionalLoadDetails() { }

  void set_error_code(int error_code) { error_code_ = error_code; };
  int error_code() const { return error_code_; }

  const GURL& url() const { return url_; }

  bool main_frame() const { return is_main_frame_; }

  bool interstitial_page() const { return is_interstitial_page_; }

  bool in_page_navigation() const { return is_in_page_navigation_; }

  int ssl_cert_id() const { return ssl_cert_id_; }

  int ssl_cert_status() const { return ssl_cert_status_; }

  int ssl_security_bits() const { return ssl_security_bits_; }

  bool is_content_filtered() const { return is_content_filtered_; }

 private:
  int error_code_;
  GURL url_;
  bool is_main_frame_;
  bool is_interstitial_page_;
  bool is_in_page_navigation_;
  int ssl_cert_id_;
  int ssl_cert_status_;
  int ssl_security_bits_;
  bool is_content_filtered_;

  DISALLOW_EVIL_CONSTRUCTORS(ProvisionalLoadDetails);
};

#endif  // CHROME_BROWSER_PROVISIONAL_LOAD_DETAILS_H__

