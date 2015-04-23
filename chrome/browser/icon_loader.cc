// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "third_party/skia/include/core/SkBitmap.h"

IconLoader::IconLoader(const IconGroupID& group, IconSize size,
                       Delegate* delegate)
    : group_(group),
      icon_size_(size),
      bitmap_(NULL),
      delegate_(delegate) {
}

IconLoader::~IconLoader() {
  delete bitmap_;
}

void IconLoader::Start() {
  target_message_loop_ = MessageLoop::current();

  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoader::ReadIcon));
}

void IconLoader::NotifyDelegate() {
  delegate_->OnBitmapLoaded(this, bitmap_);
  bitmap_ = NULL;
}
