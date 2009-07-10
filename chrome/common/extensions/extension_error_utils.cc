// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_error_utils.h"

#include "base/string_util.h"

std::string ExtensionErrorUtils::FormatErrorMessage(
    const std::string& format,
    const std::string s1) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  return ret_val;
}

std::string ExtensionErrorUtils::FormatErrorMessage(
    const std::string& format,
    const std::string s1,
    const std::string s2) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  return ret_val;
}
