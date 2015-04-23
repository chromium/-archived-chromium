// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"

#include "base/basictypes.h"

#define STRINGIZE(x) #x

namespace net {

const char kErrorDomain[] = "net";

const char* ErrorToString(int error) {
  if (error == 0)
    return "net::OK";

  switch (error) {
#define NET_ERROR(label, value) \
  case ERR_ ## label: \
    return "net::" STRINGIZE(ERR_ ## label);
#include "net/base/net_error_list.h"
#undef NET_ERROR
  default:
    return "net::<unknown>";
  }
}

}  // namespace net
