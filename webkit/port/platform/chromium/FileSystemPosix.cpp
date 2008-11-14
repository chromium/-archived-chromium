// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#include "FileSystem.h"
#include "CString.h"

namespace WebCore {

// This function is tasked with transforming a String to a CString for the
// underlying operating system. On Linux the kernel doesn't care about the
// filenames so long as NUL and '/' are respected. UTF8 filenames seem to be
// pretty common, but are not universal, so this could potentially be different
// on different systems.  Until we figure out how to pick the right encoding,
// we use UTF8.
// TODO(evanm) - when we figure out how this works in base, abstract this
// concept into ChromiumBridge.
CString fileSystemRepresentation(const String& path) {
    return path.utf8();
}

}
