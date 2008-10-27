// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_certificate.h"

#include "base/logging.h"

namespace net {

// TODO(port): finish implementing.  
// At the moment, all that's here is just enough to prevent a link error.

X509Certificate::~X509Certificate() {
  // We might not be in the cache, but it is safe to remove ourselves anyway.
  X509Certificate::Cache::GetInstance()->Remove(this);
}

// static
X509Certificate* X509Certificate::CreateFromPickle(const Pickle& pickle,
                                                   void** pickle_iter) {
  NOTIMPLEMENTED();
  return NULL;
}

void X509Certificate::Persist(Pickle* pickle) {
  NOTIMPLEMENTED();
}

}  // namespace net

