// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/fileicon_source.h"

#include "base/gfx/png_encoder.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/time_format.h"

#include "generated_resources.h"

// The path used in internal URLs to file icon data.
static const char kFileIconPath[] = "fileicon";

FileIconSource::FileIconSource()
    : DataSource(kFileIconPath, MessageLoop::current()) {}

FileIconSource::~FileIconSource() {
  cancelable_consumer_.CancelAllRequests();
}

void FileIconSource::StartDataRequest(const std::string& path,
                                      int request_id) {
  IconManager* im = g_browser_process->icon_manager();

  // Fast look up.
  SkBitmap* icon = im->LookupIcon(UTF8ToWide(path), IconLoader::NORMAL);

  if (icon) {
    std::vector<unsigned char> png_bytes;
    PNGEncoder::EncodeBGRASkBitmap(*icon, false, &png_bytes);

    scoped_refptr<RefCountedBytes> icon_data = new RefCountedBytes(png_bytes);
    SendResponse(request_id, icon_data);
  } else {
    // Icon was not in cache, go fetch it slowly.
    IconManager::Handle h = im->LoadIcon(UTF8ToWide(path), IconLoader::NORMAL,
        &cancelable_consumer_,
        NewCallback(this, &FileIconSource::OnFileIconDataAvailable));

    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(im, h, request_id);
  }
}

void FileIconSource::OnFileIconDataAvailable(IconManager::Handle handle,
                                             SkBitmap* icon) {
  IconManager* im = g_browser_process->icon_manager();
  int request_id = cancelable_consumer_.GetClientData(im, handle);

  if (icon) {
    std::vector<unsigned char> png_bytes;
    PNGEncoder::EncodeBGRASkBitmap(*icon, false, &png_bytes);

    scoped_refptr<RefCountedBytes> icon_data = new RefCountedBytes(png_bytes);
    SendResponse(request_id, icon_data);
  } else {
    // TODO(glen): send a dummy icon.
    SendResponse(request_id, NULL);
  }
}