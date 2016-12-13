// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FORCE_TLS_STATE_H_
#define NET_BASE_FORCE_TLS_STATE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/lock.h"

class GURL;

namespace net {

// ForceTLSState
//
// Tracks which hosts have enabled ForceTLS.  After a host enables ForceTLS,
// then we refuse to talk to the host over HTTP, treat all certificate errors as
// fatal, and refuse to load any mixed content.
//
class ForceTLSState {
 public:
  ForceTLSState();

  // Called when we see an X-Force-TLS header that we should process.  Modifies
  // our state as instructed by the header.
  void DidReceiveHeader(const GURL& url, const std::string& value);

  // Enable ForceTLS for |host|.
  void EnableHost(const std::string& host);

  // Returns whether |host| has had ForceTLS enabled.
  bool IsEnabledForHost(const std::string& host);

  // Returns |true| if |value| parses as a valid X-Force-TLS header value.
  // The values of max-age and and includeSubDomains are returned in |max_age|
  // and |include_subdomains|, respectively.  The out parameters are not
  // modified if the function returns |false|.
  static bool ParseHeader(const std::string& value,
                          int* max_age,
                          bool* include_subdomains);

 private:
  // The set of hosts that have enabled ForceTLS.
  std::set<std::string> enabled_hosts_;

  // Protect access to our data members with this lock.
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ForceTLSState);
};

}  // namespace net

#endif  // NET_BASE_FORCE_TLS_STATE_H_
