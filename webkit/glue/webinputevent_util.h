// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBINPUTEVENT_UTIL_H_
#define WEBKIT_GLUE_WEBINPUTEVENT_UTIL_H_

#include <string>

// The shared Linux and Windows keyboard event code lives here.

namespace webkit_glue {

std::string GetKeyIdentifierForWindowsKeyCode(unsigned short key_code);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBINPUTEVENT_UTIL_H_
