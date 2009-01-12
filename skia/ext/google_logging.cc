// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides integration with Google-style "base/logging.h" assertions
// for Skia SkASSERT. If you don't want this, you can link with another file
// that provides integration with the logging of your choice.

#include "base/logging.h"
#include "base/string_util.h"

void SkDebugf_FileLine(const char* file, int line, const char* format, ...) {
  va_list ap;
  va_start(ap, format);

  std::string msg;
  StringAppendV(&msg, format, ap);

  logging::LogMessage(file, line, logging::LOG_ERROR).stream() << msg;
}
