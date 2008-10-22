// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nss_init.h"

#include <nss.h>

// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <ssl.h>
#undef Lock

#include "base/logging.h"
#include "base/singleton.h"

namespace {

class NSSInitSingleton {
 public:
  NSSInitSingleton() {
    CHECK(NSS_NoDB_Init(".") == SECSuccess);
    // Enable ciphers
    NSS_SetDomesticPolicy();
    // Enable SSL
    SSL_OptionSetDefault(SSL_SECURITY, PR_TRUE);
  }

  ~NSSInitSingleton() {
    // Have to clear the cache, or NSS_Shutdown fails with SEC_ERROR_BUSY
    SSL_ClearSessionCache();

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
