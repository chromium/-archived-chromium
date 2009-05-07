// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader_win.h"

#include <windows.h>
#include <shellapi.h>

#include "app/gfx/icon_util.h"
#include "base/gfx/size.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "skia/include/SkBitmap.h"

IconLoader* IconLoader::Create(const FilePath& path, IconSize size,
                               Delegate* delegate) {
  return new IconLoaderWin(path, size, delegate);
}

IconLoaderWin::IconLoaderWin(const FilePath& path, IconSize size,
                             Delegate* delegate)
    : path_(path),
      icon_size_(size),
      bitmap_(NULL),
      delegate_(delegate) {
}

IconLoaderWin::~IconLoaderWin() {
  delete bitmap_;
}

void IconLoaderWin::Start() {
  target_message_loop_ = MessageLoop::current();

  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoaderWin::ReadIcon));
}

void IconLoaderWin::ReadIcon() {
  int size = 0;
  switch (icon_size_) {
    case IconLoader::SMALL:
      size = SHGFI_SMALLICON;
      break;
    case IconLoader::NORMAL:
      size = 0;
      break;
    case IconLoader::LARGE:
      size = SHGFI_LARGEICON;
      break;
    default:
      NOTREACHED();
  }
  SHFILEINFO file_info = { 0 };
  if (!SHGetFileInfo(path_.value().c_str(), FILE_ATTRIBUTE_NORMAL, &file_info,
                     sizeof(SHFILEINFO),
                     SHGFI_ICON | size | SHGFI_USEFILEATTRIBUTES))
    return;

  ICONINFO icon_info = { 0 };
  BITMAP bitmap_info = { 0 };

  BOOL r = ::GetIconInfo(file_info.hIcon, &icon_info);
  DCHECK(r);
  r = ::GetObject(icon_info.hbmMask, sizeof(bitmap_info), &bitmap_info);
  DCHECK(r);

  gfx::Size icon_size(bitmap_info.bmWidth, bitmap_info.bmHeight);
  bitmap_ = IconUtil::CreateSkBitmapFromHICON(file_info.hIcon, icon_size);

  target_message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &IconLoaderWin::NotifyDelegate));
}

void IconLoaderWin::NotifyDelegate() {
  delegate_->OnBitmapLoaded(this, bitmap_);
  bitmap_ = NULL;
}
