// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <windows.h>
#include <shellapi.h>

#include "base/file_util.h"
#include "base/gfx/size.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/gfx/icon_util.h"

#include "SkBitmap.h"

namespace {
class IconLoaderProcessor :
    public base::RefCountedThreadSafe<IconLoaderProcessor> {
 public:
  explicit IconLoaderProcessor(IconLoader* target)
      : target_(target),
        bitmap_(NULL),
        small_icon_(NULL),
        large_icon_(NULL),
        loading_from_resource_(target->loading_from_resource_),
        icon_type_(target->icon_type_),
        icon_size_(target->icon_size_) {
    DCHECK(target);
    path_ = target->path_;
    target_message_loop_ = MessageLoop::current();
  }

  virtual ~IconLoaderProcessor() {
    delete bitmap_;
    if (small_icon_)
      ::DestroyIcon(small_icon_);
    if (large_icon_) {
      ::DestroyIcon(large_icon_);
    }
  }

  // Loads the icon with the specified dimensions.
  HICON LoadSizedIcon(int width, int height) {
    return static_cast<HICON>(LoadImage(NULL,
                                        path_.c_str(),
                                        width, height,
                                        IMAGE_ICON,
                                        LR_LOADTRANSPARENT | LR_LOADFROMFILE));
  }

  // Invoked from the original thread.
  void Cancel() {
    target_ = NULL;
  }

  // Invoked in the file thread. Never access target_ from this method.
  void ReadIcon() {
    if (loading_from_resource_)
      ReadIconFromFileResource();
    else
      ReadIconFile();

    target_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &IconLoaderProcessor::NotifyFetcher));
  }

  // Invoked on the file thread to read a normal .ico file.
  void ReadIconFile() {
    // We start by loading the same icon image in the two desired dimensions,
    // based on the dimensions we get back when we query the system.
    int small_width = ::GetSystemMetrics(SM_CXSMICON);
    int small_height = ::GetSystemMetrics(SM_CYSMICON);
    int large_width = ::GetSystemMetrics(SM_CXICON);
    int large_height = ::GetSystemMetrics(SM_CYICON);
    small_icon_ = LoadSizedIcon(small_width, small_height);
    large_icon_ = LoadSizedIcon(large_width, large_height);

    // TODO(idana): Bug 991356. Currently, if the client asks for the icon in
    // the form of an SkBitmap object, then we try converting the large icon
    // into an SkBitmap, if the icon was successfully loaded. If the large icon
    // is not available, we convert the small icon. The problem with converting
    // the small or the large icon is that the resulting image is going to have
    // the same dimensions as the icon (for example, 32X32 pixels). This can be
    // problematic if, for example, the client tries to display the resulting
    // image as a thumbnail. This will result in the client streching the
    // bitmap from 32X32 to 128X128 which will decrease the image's quality.
    //
    // Since it is possible for web applications to define large .PNG images as
    // their icons, the resulting .ico files created for these web applications
    // contain icon images in sizes much larger the the system default sizes
    // for icons. We can solve the aforementioned limitation by allowing the
    // client to specify the size of the resulting image, when requesting an
    // SkBitmap. The IconLoader code can then load a larger icon from the .ico
    // file.
    //
    // Note that currently the components in Chrome that deal with SkBitmaps
    // that represent application icons use the images to display icon size
    // images and therefore the limitation above doesn't really manifest
    // itself.
    HICON icon_to_convert = NULL;
    gfx::Size s;
    if (icon_type_ == IconLoader::SK_BITMAP) {
      if (large_icon_) {
        icon_to_convert = large_icon_;
        s.SetSize(large_width, large_height);
      } else if (small_icon_) {
        icon_to_convert = small_icon_;
        s.SetSize(small_width, small_height);
      }
    }

    if (icon_to_convert) {
      bitmap_ = IconUtil::CreateSkBitmapFromHICON(icon_to_convert, s);
      DCHECK(bitmap_);
      if (small_icon_)
        ::DestroyIcon(small_icon_);
      if (large_icon_)
        ::DestroyIcon(large_icon_);

      small_icon_ = NULL;
      large_icon_ = NULL;
    }
  }

  void ReadIconFromFileResource() {
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
    if (!SHGetFileInfo(path_.c_str(), FILE_ATTRIBUTE_NORMAL, &file_info,
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
  }

  // Invoked in the target thread.
  void NotifyFetcher() {
    if (target_) {
      switch (target_->icon_type_) {
        case IconLoader::SK_BITMAP:
          if (target_->OnLoadComplete(bitmap_)) {
            // Receiver took ownership of the bitmap.
            bitmap_ = NULL;
          }
          break;
        case IconLoader::WINDOWS_HICON:
          if (target_->OnLoadComplete(small_icon_, large_icon_)) {
            // Receiver took ownership of the icons.
            small_icon_ = NULL;
            large_icon_ = NULL;
          }
          break;
        default:
          NOTREACHED();
      }
    }
  }

 private:
  // The message loop object of the thread in which we notify the delegate.
  MessageLoop* target_message_loop_;

  // The corresponding file fetcher or NULL if this task has been canceled.
  IconLoader* target_;

  // The path of the file.
  std::wstring path_;

  // Fields from IconLoader that we need to copy as we cannot access them
  // directly from the target_ in a thread-safe way.
  bool loading_from_resource_;
  IconLoader::IconSize icon_size_;
  IconLoader::IconType icon_type_;

  // The result bitmap.
  SkBitmap* bitmap_;

  // The result small icon.
  HICON small_icon_;

  // The result large icon.
  HICON large_icon_;

  DISALLOW_EVIL_CONSTRUCTORS(IconLoaderProcessor);
};
}

// static
IconLoader* IconLoader::CreateIconLoaderForFile(const std::wstring& path,
                                                IconType icon_type,
                                                Delegate* delegate) {
  // Note: the icon size is unused in this case.
  return new IconLoader(path, icon_type, false, IconLoader::NORMAL, delegate);
}

// static
IconLoader* IconLoader::CreateIconLoaderForFileResource(
    const std::wstring& path, IconSize size, Delegate* delegate) {
  return new IconLoader(path, IconLoader::SK_BITMAP, true, size, delegate);
}

IconLoader::IconLoader(const std::wstring& path,
                       IconType type,
                       bool from_resource,
                       IconSize size,
                       Delegate* delegate)
    : path_(path),
      icon_type_(type),
      loading_from_resource_(from_resource),
      icon_size_(size),
      delegate_(delegate),
      processor_(NULL) {
  DCHECK(delegate_);
}

IconLoader::~IconLoader() {
  Cancel();
}

void IconLoader::Start() {
  processor_ = new IconLoaderProcessor(this);
  processor_->AddRef();
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(processor_, &IconLoaderProcessor::ReadIcon));
}

void IconLoader::Cancel() {
  if (processor_) {
    processor_->Cancel();
    processor_->Release();
    processor_ = NULL;
  }
  delegate_ = NULL;
}

bool IconLoader::OnLoadComplete(SkBitmap* bitmap) {
  if (delegate_) {
    return delegate_->OnSkBitmapLoaded(this, bitmap);
    // We are likely deleted after this point.
  }
  return false;
}

bool IconLoader::OnLoadComplete(HICON small_icon, HICON large_icon) {
  if (delegate_) {
    return delegate_->OnHICONLoaded(this, small_icon, large_icon);
    // We are likely deleted after this point.
  }
  return false;
}
