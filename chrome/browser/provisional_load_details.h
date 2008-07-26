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

#ifndef CHROME_BROWSER_PROVISIONAL_LOAD_DETAILS_H__
#define CHROME_BROWSER_PROVISIONAL_LOAD_DETAILS_H__

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

// This class captures some of the information associated to the provisional
// load of a frame.  It is provided as Details with the
// NOTIFY_FRAME_PROVISIONAL_LOAD_START, NOTIFY_FRAME_PROVISIONAL_LOAD_COMMITTED
// and NOTIFY_FAIL_PROVISIONAL_LOAD_WITH_ERROR notifications
// (see notification_types.h).

class ProvisionalLoadDetails {
 public:
  ProvisionalLoadDetails(bool main_frame,
                         bool interstitial_page,
                         bool in_page_navigation,
                         const GURL& url,
                         const std::string& security_info);
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

 private:
  int error_code_;
  GURL url_;
  bool is_main_frame_;
  bool is_interstitial_page_;
  bool is_in_page_navigation_;
  int ssl_cert_id_;
  int ssl_cert_status_;
  int ssl_security_bits_;

  DISALLOW_EVIL_CONSTRUCTORS(ProvisionalLoadDetails);
};

#endif  // CHROME_BROWSER_PROVISIONAL_LOAD_DETAILS_H__
