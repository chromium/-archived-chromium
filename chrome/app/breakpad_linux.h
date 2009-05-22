// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_LINUX_H_
#define CHROME_APP_BREAKPAD_LINUX_H_

extern void EnableCrashDumping();
extern int UploadCrashDump(const char* filename, const char* crash_url,
                           unsigned crash_url_length);

#endif  // CHROME_APP_BREAKPAD_LINUX_H_
