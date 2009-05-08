// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/message_loop.h"
#include "base/thread.h"

void IconLoader::ReadIcon() {
  NOTIMPLEMENTED();

  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::NotifyDelegate));
}
