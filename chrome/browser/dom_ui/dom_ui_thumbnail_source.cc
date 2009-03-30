// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/dom_ui_thumbnail_source.h"

#include "chrome/browser/profile.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/theme_resources.h"

DOMUIThumbnailSource::DOMUIThumbnailSource(Profile* profile)
    : DataSource(chrome::kChromeUIThumbnailPath, MessageLoop::current()),
      profile_(profile) {
}

void DOMUIThumbnailSource::StartDataRequest(const std::string& path,
                                            int request_id) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs) {
    HistoryService::Handle handle = hs->GetPageThumbnail(
        GURL(path),
        &cancelable_consumer_,
        NewCallback(this, &DOMUIThumbnailSource::OnThumbnailDataAvailable));
    // Attach the ChromeURLDataManager request ID to the history request.
    cancelable_consumer_.SetClientData(hs, handle, request_id);
  } else {
    // Tell the caller that no thumbnail is available.
    SendResponse(request_id, NULL);
  }
}

void DOMUIThumbnailSource::OnThumbnailDataAvailable(
    HistoryService::Handle request_handle,
    scoped_refptr<RefCountedBytes> data) {
  HistoryService* hs =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  int request_id = cancelable_consumer_.GetClientData(hs, request_handle);
  // Forward the data along to the networking system.
  if (data.get() && !data->data.empty()) {
    SendResponse(request_id, data);
  } else {
    if (!default_thumbnail_.get()) {
      default_thumbnail_ = new RefCountedBytes;
      ResourceBundle::GetSharedInstance().LoadImageResourceBytes(
          IDR_DEFAULT_THUMBNAIL, &default_thumbnail_->data);
    }

    SendResponse(request_id, default_thumbnail_);
  }
}

