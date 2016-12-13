// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_ERROR_UTILS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_ERROR_UTILS_H_

#include <string>

class ExtensionErrorUtils {
public:
  // Creates an error messages from a pattern. Places first instance if "*"
  // with |s1|.
  static std::string FormatErrorMessage(const std::string& format,
    const std::string s1);

  // Creates an error messages from a pattern. Places first instance if "*"
  // with |s1| and second instance of "*" with |s2|.
  static std::string FormatErrorMessage(const std::string& format,
    const std::string s1,
    const std::string s2);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_ERROR_UTILS_H_
