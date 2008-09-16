// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nss_init.h"

#include "base/logging.h"
#include "base/singleton.h"

// Include this header last, since NSS will define "Lock" in an enum.
//   https://bugzilla.mozilla.org/show_bug.cgi?id=455424
#include <nss.h>

namespace base {

namespace {

class NSSInitSingleton {
 public:
  NSSInitSingleton() {
    CHECK(NSS_NoDB_Init(".") == SECSuccess);
  }

  ~NSSInitSingleton() {
    NSS_Shutdown();
  }
};

}  // namespace

void EnsureNSSInit() {
  Singleton<NSSInitSingleton>::get();
}

}  // namespace base
