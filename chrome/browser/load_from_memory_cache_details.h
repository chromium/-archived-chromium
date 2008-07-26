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

#ifndef CHROME_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H__
#define CHROME_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H__

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

class LoadFromMemoryCacheDetails {
 public:
  LoadFromMemoryCacheDetails(const GURL& url, int cert_id, int cert_status)
      : url_(url),
        cert_id_(cert_id),
        cert_status_(cert_status)
  { }

  ~LoadFromMemoryCacheDetails() { }

  const GURL& url() const { return url_; }
  int ssl_cert_id() const { return cert_id_; }
  int ssl_cert_status() const { return cert_status_; }

private:
  GURL url_;
  int cert_id_;
  int cert_status_;

  DISALLOW_EVIL_CONSTRUCTORS(LoadFromMemoryCacheDetails);
};

#endif  // CHROME_BROWSER_LOAD_FROM_MEMORY_CACHE_DETAILS_H__
