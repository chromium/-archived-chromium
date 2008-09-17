// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nss_init.h"

#include <nss.h>

#include "base/logging.h"
#include "base/singleton.h"

namespace {

class NSSInitSingleton {
 public:
  NSSInitSingleton() {
    CHECK(NSS_NoDB_Init(".") == SECSuccess);
  }

  ~NSSInitSingleton() {
    SECStatus status = NSS_Shutdown();
    DCHECK(status == SECSuccess);
  }
};

}  // namespace

namespace base {

void EnsureNSSInit() {
  Singleton<NSSInitSingleton>::get();
}

}  // namespace base
