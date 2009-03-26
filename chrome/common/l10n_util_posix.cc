// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/l10n_util.h"

namespace l10n_util {

// Return true blindly for now.
bool IsLocaleSupportedByOS(const std::wstring& locale) {
  return true;
}

}  // namespace l10n_util
