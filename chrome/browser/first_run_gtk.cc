
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "chrome/browser/gtk/first_run_dialog.h"

bool OpenFirstRunDialog(Profile* profile, ProcessSingleton* process_singleton) {
  // TODO(port): Use process_singleton to make sure Chrome can not be started
  // while this process is active.
  return FirstRunDialog::Show(profile);
}
