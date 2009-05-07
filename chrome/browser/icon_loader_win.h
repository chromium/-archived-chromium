// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ICON_LOADER_WIN_H_
#define CHROME_BROWSER_ICON_LOADER_WIN_H_

#include "chrome/browser/icon_loader.h"

class MessageLoop;

class IconLoaderWin : public IconLoader {
 public:
  IconLoaderWin(const FilePath& path, IconSize size, Delegate* delegate);

  virtual ~IconLoaderWin();

  // IconLoader implementation.
  virtual void Start();

 private:
  void ReadIcon();

  void NotifyDelegate();

  // The message loop object of the thread in which we notify the delegate.
  MessageLoop* target_message_loop_;

  FilePath path_;

  IconSize icon_size_;

  SkBitmap* bitmap_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(IconLoaderWin);
};

#endif  // CHROME_BROWSER_ICON_LOADER_WIN_H_
