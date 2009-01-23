// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fav_icon_helper.h"

#include "base/gfx/png_decoder.h"
#include "base/gfx/png_encoder.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/gfx/favicon_size.h"
#include "skia/ext/image_operations.h"

FavIconHelper::FavIconHelper(WebContents* web_contents)
    : web_contents_(web_contents),
      got_fav_icon_url_(false),
      got_fav_icon_from_history_(false),
      fav_icon_expired_(false) {
}

void FavIconHelper::FetchFavIcon(const GURL& url) {
  cancelable_consumer_.CancelAllRequests();

  url_ = url;

  fav_icon_expired_ = got_fav_icon_from_history_ = got_fav_icon_url_ = false;

  // Request the favicon from the history service. In parallel to this the
  // renderer is going to notify us (well webcontents) when the favicon url is
  // available.
  if (GetHistoryService()) {
    GetHistoryService()->GetFavIconForURL(url_, &cancelable_consumer_,
        NewCallback(this, &FavIconHelper::OnFavIconDataForInitialURL));
  }
}

void FavIconHelper::SetFavIconURL(const GURL& icon_url) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  got_fav_icon_url_ = true;

  if (!GetHistoryService())
    return;

  if (!fav_icon_expired_ && entry->favicon().is_valid() &&
      entry->favicon().url() == icon_url) {
    // We already have the icon, no need to proceed.
    return;
  }

  entry->favicon().set_url(icon_url);

  if (got_fav_icon_from_history_)
    DownloadFavIconOrAskHistory(entry);
}

Profile* FavIconHelper::profile() {
  return web_contents_->profile();
}

HistoryService* FavIconHelper::GetHistoryService() {
  return profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
}

void FavIconHelper::SetFavIcon(
    int download_id,
    const GURL& image_url,
    const SkBitmap& image) {
  DownloadRequests::iterator i = download_requests_.find(download_id);
  if (i == download_requests_.end()) {
    // Currently WebContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }

  const SkBitmap& sized_image =
      (image.width() == kFavIconSize && image.height() == kFavIconSize)
      ? image : ConvertToFavIconSize(image);

  if (GetHistoryService() && !profile()->IsOffTheRecord()) {
    std::vector<unsigned char> image_data;
    PNGEncoder::EncodeBGRASkBitmap(sized_image, false, &image_data);
    GetHistoryService()->SetFavIcon(i->second.url, i->second.fav_icon_url,
                                    image_data);
  }

  if (i->second.url == url_) {
    NavigationEntry* entry = GetEntry();
    if (entry)
      UpdateFavIcon(entry, sized_image);
  }

  download_requests_.erase(i);
}

void FavIconHelper::FavIconDownloadFailed(int download_id) {
  DownloadRequests::iterator i = download_requests_.find(download_id);
  if (i != download_requests_.end())
    download_requests_.erase(i);
}

void FavIconHelper::UpdateFavIcon(NavigationEntry* entry,
                                  const std::vector<unsigned char>& data) {
  SkBitmap image;
  PNGDecoder::Decode(&data, &image);
  UpdateFavIcon(entry, image);
}

void FavIconHelper::UpdateFavIcon(NavigationEntry* entry,
                                  const SkBitmap& image) {
  // No matter what happens, we need to mark the favicon as being set.
  entry->favicon().set_is_valid(true);

  if (image.empty())
    return;

  entry->favicon().set_bitmap(image);
  if (web_contents_->delegate()) {
    web_contents_->delegate()->NavigationStateChanged(
        web_contents_, TabContents::INVALIDATE_FAVICON);
  }
}

NavigationEntry* FavIconHelper::GetEntry() {
  NavigationEntry* entry = web_contents_->controller()->GetActiveEntry();
  if (entry && entry->url() == url_ &&
      web_contents_->IsActiveEntry(entry->page_id())) {
    return entry;
  }
  // If the URL has changed out from under us (as will happen with redirects)
  // return NULL.
  return NULL;
}

void FavIconHelper::OnFavIconDataForInitialURL(
    HistoryService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  got_fav_icon_from_history_ = true;

  fav_icon_expired_ = (know_favicon && expired);

  if (know_favicon && !entry->favicon().is_valid() &&
      (!got_fav_icon_url_ || entry->favicon().url() == icon_url)) {
    // The db knows the favicon (although it may be out of date) and the entry
    // doesn't have an icon. Set the favicon now, and if the favicon turns out
    // to be expired (or the wrong url) we'll fetch later on. This way the
    // user doesn't see a flash of the default favicon.
    entry->favicon().set_url(icon_url);
    if (data && !data->data.empty())
      UpdateFavIcon(entry, data->data);
    entry->favicon().set_is_valid(true);
  }

  if (know_favicon && !expired) {
    if (got_fav_icon_url_ && entry->favicon().url() != icon_url) {
      // Mapping in the database is wrong. DownloadFavIconOrAskHistory will
      // update the mapping for this url and download the favicon if we don't
      // already have it.
      DownloadFavIconOrAskHistory(entry);
    }
  } else if (got_fav_icon_url_) {
    // We know the official url for the favicon, by either don't have the
    // favicon or its expired. Continue on to DownloadFavIconOrAskHistory to
    // either download or check history again.
    DownloadFavIconOrAskHistory(entry);
  } // else we haven't got the icon url. When we get it we'll ask the
    // renderer to download the icon.
}

void FavIconHelper::DownloadFavIconOrAskHistory(NavigationEntry* entry) {
  DCHECK(entry); // We should only get here if entry is valid.
  if (fav_icon_expired_) {
    // We have the mapping, but the favicon is out of date. Download it now.
    ScheduleDownload(entry);
  } else if (GetHistoryService()) {
    // We don't know the favicon, but we may have previously downloaded the
    // favicon for another page that shares the same favicon. Ask for the
    // favicon given the favicon URL.
    if (profile()->IsOffTheRecord()) {
      GetHistoryService()->GetFavIcon(
          entry->favicon().url(),
          &cancelable_consumer_,
          NewCallback(this, &FavIconHelper::OnFavIconData));
    } else {
      // Ask the history service for the icon. This does two things:
      // 1. Attempts to fetch the favicon data from the database.
      // 2. If the favicon exists in the database, this updates the database to
      //    include the mapping between the page url and the favicon url.
      // This is asynchronous. The history service will call back when done.
      // Issue the request and associate the current page ID with it.
      GetHistoryService()->UpdateFavIconMappingAndFetch(
          entry->url(),
          entry->favicon().url(), &cancelable_consumer_,
          NewCallback(this, &FavIconHelper::OnFavIconData));
    }
  }
}

void FavIconHelper::OnFavIconData(
    HistoryService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedBytes> data,
    bool expired,
    GURL icon_url) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  // No need to update the favicon url. By the time we get here
  // UpdateFavIconURL will have set the favicon url.

  if (know_favicon && data && !data->data.empty()) {
    // There is a favicon, set it now. If expired we'll download the current
    // one again, but at least the user will get some icon instead of the
    // default and most likely the current one is fine anyway.
    UpdateFavIcon(entry, data->data);
  }

  if (!know_favicon || expired) {
    // We don't know the favicon, or it is out of date. Request the current one.
    ScheduleDownload(entry);
  }
}

void FavIconHelper::ScheduleDownload(NavigationEntry* entry) {
  const int download_id = web_contents_->render_view_host()->DownloadImage(
      entry->favicon().url(), kFavIconSize);
  if (!download_id) {
    // Download request failed.
    return;
  }
  // Download ids should be unique.
  DCHECK(download_requests_.find(download_id) == download_requests_.end());
  download_requests_[download_id] =
      DownloadRequest(entry->url(), entry->favicon().url());
}

SkBitmap FavIconHelper::ConvertToFavIconSize(const SkBitmap& image) {
  int width = image.width();
  int height = image.height();
  if (width > 0 && height > 0) {
    calc_favicon_target_size(&width, &height);
    return skia::ImageOperations::Resize(
          image, skia::ImageOperations::RESIZE_LANCZOS3,
          width, height);
  }
  return image;
}

