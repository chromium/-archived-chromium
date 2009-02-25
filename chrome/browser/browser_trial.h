// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BrowserTrial contains methods specific to field trials (using FieldTrial
// functionality) for browser tests.

#ifndef CHROME_BROWSER_BROWSER_TRIAL_H_
#define CHROME_BROWSER_BROWSER_TRIAL_H_

#include "base/field_trial.h"

// Currently we use this as a name space, to hold static shared constants which
// define current and past trials.
class BrowserTrial {
 public:

  // The following is a sample line for what should be listed in this file.
  // static const wchar_t* kMemoryModelFieldTrial;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTrial);
};

#endif  // CHROME_BROWSER_BROWSER_TRIAL_H_
