// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CLIPBOARD_SERVICE_H_
#define CHROME_COMMON_CLIPBOARD_SERVICE_H_

#include "base/clipboard.h"

class ClipboardService : public Clipboard {
 public:
  ClipboardService() {}

 private:

  DISALLOW_COPY_AND_ASSIGN(ClipboardService);
};

#endif  // CHROME_COMMON_CLIPBOARD_SERVICE_H_
