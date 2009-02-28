// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/http/http_auth_handler.h"

namespace net {

bool HttpAuthHandler::InitFromChallenge(std::string::const_iterator begin,
                                        std::string::const_iterator end,
                                        HttpAuth::Target target) {
  target_ = target;
  score_ = -1;
  properties_ = -1;

  bool ok = Init(begin, end);

  // Init() is expected to set the scheme, realm, score, and properties.  The
  // realm may be empty.
  DCHECK(!ok || !scheme().empty());
  DCHECK(!ok || score_ != -1);
  DCHECK(!ok || properties_ != -1);

  return ok;
}

}  // namespace net
