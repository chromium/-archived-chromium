// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "config.h"
#include "FileSystem.h"
#include "CString.h"

#include "base/sys_string_conversions.h"
#include "glue/glue_util.h"

namespace WebCore {

// This function is tasked with transforming a String to a CString for the
// underlying operating system. On Linux the kernel doesn't care about the
// filenames so long as NUL and '/' are respected. UTF8 filenames seem to be
// pretty common, but are not universal so we punt on the decision here and pass
// the buck to a function in base.
CString fileSystemRepresentation(const String &path) {
    return webkit_glue::StdStringToCString(
        base::SysWideToNativeMB(webkit_glue::StringToStdWString(path)));
}

}
