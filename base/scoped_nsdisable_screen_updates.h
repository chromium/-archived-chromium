// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_NSDISABLE_SCREEN_UPDATES_H_
#define BASE_SCOPED_NSDISABLE_SCREEN_UPDATES_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"

namespace base {

// A stack-based class to disable Cocoa screen updates. When instantiated, it
// disables screen updates and enables them when destroyed. Update disabling
// can be nested, and there is a time-maximum (about 1 second) after which
// Cocoa will automatically re-enable updating. This class doesn't attempt to
// overrule that.

class ScopedNSDisableScreenUpdates {
 public:
  ScopedNSDisableScreenUpdates() {
    NSDisableScreenUpdates();
  }
  ~ScopedNSDisableScreenUpdates() {
    NSEnableScreenUpdates();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedNSDisableScreenUpdates);
};

}  // namespace

#endif
