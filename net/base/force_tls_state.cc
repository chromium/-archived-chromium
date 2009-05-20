// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/force_tls_state.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace net {

ForceTLSState::ForceTLSState() {
}

void ForceTLSState::DidReceiveHeader(const GURL& url,
                                     const std::string& value) {
  // TODO(abarth): Actually parse |value| once the spec settles down.
  EnableHost(url.host());
}

void ForceTLSState::EnableHost(const std::string& host) {
  // TODO(abarth): Canonicalize host.
  AutoLock lock(lock_);
  enabled_hosts_.insert(host);
}

bool ForceTLSState::IsEnabledForHost(const std::string& host) {
  // TODO(abarth): Canonicalize host.
  AutoLock lock(lock_);
  return enabled_hosts_.find(host) != enabled_hosts_.end();
}

}  // namespace
