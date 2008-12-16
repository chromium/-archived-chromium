// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/clipboard.h"

#include "base/logging.h"

void Clipboard::DispatchObject(ObjectType type, const ObjectMapParams& params) {
  switch (type) {
    case CBF_TEXT:
      WriteText(&(params[0].front()), params[0].size());
      break;

    case CBF_HTML:
      if (params.size() == 2)
        WriteHTML(&(params[0].front()), params[0].size(),
                  &(params[1].front()), params[1].size());
      else
        WriteHTML(&(params[0].front()), params[0].size(), NULL, 0);
      break;

    case CBF_BOOKMARK:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case CBF_LINK:
      WriteHyperlink(&(params[0].front()), params[0].size(),
                     &(params[1].front()), params[1].size());
      break;

    case CBF_FILES:
      WriteFiles(&(params[0].front()), params[0].size());
      break;

    case CBF_WEBKIT:
      WriteWebSmartPaste();
      break;

#if defined(OS_WIN) || defined(OS_LINUX)  // This is just a stub on Linux
    case CBF_BITMAP:
      WriteBitmap(&(params[0].front()), &(params[1].front()));
      break;
#endif  // defined(OS_WIN) || defined(OS_LINUX)

    default:
      NOTREACHED();
  }
}
