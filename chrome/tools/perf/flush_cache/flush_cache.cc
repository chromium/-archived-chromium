// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This little program attempts to flush the disk cache for some files.
// It's useful for testing Chrome with a cold database.

#include "build/build_config.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_piece.h"
#include "base/process_util.h"
#include "base/sys_string_conversions.h"

int main(int argc, const char* argv[]) {
  process_util::EnableTerminationOnHeapCorruption();
  if (argc <= 1) {
    fprintf(stderr, "flushes disk cache for files\n");
    fprintf(stderr, "usage: %s <filenames>\n", argv[0]);
    return 1;
  }

  for (int i = 1; i < argc; ++i) {
#if defined(OS_POSIX)
    std::string filename(argv[i]);
#elif defined(OS_WIN)
    std::wstring filename = base::SysNativeMBToWide(argv[i]);
#endif
    if (!file_util::EvictFileFromSystemCache(FilePath(filename))) {
      fprintf(stderr, "Failed to evict %s from cache -- is it a directory?\n",
              argv[i]);
    }
  }

  return 0;
}

