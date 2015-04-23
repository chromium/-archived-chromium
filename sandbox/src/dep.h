// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_DEP_H__
#define SANDBOX_SRC_DEP_H__

namespace sandbox {

enum DepEnforcement {
  // DEP is completely disabled.
  DEP_DISABLED,
  // DEP is permanently enforced.
  DEP_ENABLED,
  // DEP with support for ATL7 thunking is permanently enforced.
  DEP_ENABLED_ATL7_COMPAT,
};

// Change the Data Execution Prevention (DEP) status for the current process.
// Once enabled, it cannot be disabled.
bool SetCurrentProcessDEP(DepEnforcement enforcement);

}  // namespace sandbox

#endif  // SANDBOX_SRC_DEP_H__
