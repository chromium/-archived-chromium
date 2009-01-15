// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_app.h"

#include "base/gfx/png_decoder.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/tab_contents/web_contents.h"
#include "chrome/common/gfx/favicon_size.h"
#include "net/base/base64.h"
#include "net/base/data_url.h"

namespace {

static const char kPNGImageMimeType[] = "image/png";

static std::set<GURL> ExtractImageURLs(const GearsShortcutData& data) {
  std::set<GURL> image_urls;
  for (size_t i = 0; i < arraysize(data.icons); ++i) {
    if (data.icons[i].url) {
      GURL image_url(data.icons[i].url);
      if (image_url.is_valid())
        image_urls.insert(image_url);
      else
        NOTREACHED();
    }
  }
  return image_urls;
}

static SkBitmap DecodePNGEncodedURL(const GURL& url) {
  std::string mime_type, charset, data;
  if (!url.SchemeIs("data") ||
      !net::DataURL::Parse(url, &mime_type, &charset, &data) ||
      mime_type != kPNGImageMimeType) {
    return SkBitmap();
  }

  SkBitmap image;
  std::vector<unsigned char> v_data;
  v_data.resize(data.size(), 0);
  memcpy(&v_data.front(), data.c_str(), data.size());
  PNGDecoder::Decode(&v_data, &image);
  return image;
}

}  // namespace

// WebApp ----------------------------------------------------------------------

WebApp::WebApp(Profile* profile,
               const GURL& url,
               const std::wstring& name)
    : web_contents_(NULL),
      profile_(profile),
      url_(url),
      name_(name),
      loaded_images_from_web_data_(false),
      image_load_handle_(0),
      download_images_(false) {
}

WebApp::WebApp(Profile* profile,
               const GearsShortcutData& shortcut)
    : web_contents_(NULL),
      profile_(profile),
      url_(shortcut.url),
      name_(shortcut.name ? UTF8ToWide(shortcut.name) : std::wstring()),
      loaded_images_from_web_data_(false),
      image_load_handle_(0),
      image_urls_(ExtractImageURLs(shortcut)),
      download_images_(!image_urls_.empty()) {
  ExtractPNGEncodedURLs();
  // If the image urls are all data encoded urls and at least one is favicon
  // sized, then no need to load/store in web data.
  loaded_images_from_web_data_ = (GetFavIconIterator() != images_.end() &&
                                  image_urls_.empty());
}

WebApp::~WebApp() {
  if (image_load_handle_) {
    WebDataService* service =
        profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (service)
      service->CancelRequest(image_load_handle_);
  }
}

void WebApp::SetImage(const GURL& image_url, const SkBitmap& image) {
  std::set<GURL>::iterator i = image_urls_.find(image_url);
  if (i == image_urls_.end())
    return;  // We didn't request the url.

  if (image.width() == 0 || image.height() == 0) {
    // Assume there was an error downloading. By ignoring this we ensure we
    // attempt to download the image next time user launches the app.
    return;
  }

  image_urls_.erase(i);

  WebDataService* service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);

  if (!image.isNull()) {
    if (image.width() == kFavIconSize && image.height() == kFavIconSize) {
      Images::iterator fav_icon_i = GetFavIconIterator();
      if (fav_icon_i != images_.end())
        images_.erase(fav_icon_i);  // Only allow one favicon.
    }
    images_.push_back(image);
    NotifyObservers();
    if (service)
      service->SetWebAppImage(url_, image);
  }

  if (service && image_urls_.empty())
    service->SetWebAppHasAllImages(url_, true);
}

const WebApp::Images& WebApp::GetImages() {
  LoadImagesFromWebData();

  return images_;
}

SkBitmap WebApp::GetFavIcon() {
  // Force a load.
  GetImages();

  Images::iterator fav_icon_i = GetFavIconIterator();
  return (fav_icon_i == images_.end()) ? SkBitmap() : *fav_icon_i;
}

void WebApp::SetWebContents(WebContents* host) {
  web_contents_ = host;

  if (host && loaded_images_from_web_data_ && image_load_handle_ == 0 &&
      !image_urls_.empty()) {
    // We haven't downloaded all the images and got a new WebContents. Download
    // the images from it.
    DownloadImagesFromSite();
  }
}

void WebApp::AddObserver(Observer* obs) {
  observer_list_.AddObserver(obs);
}

void WebApp::RemoveObserver(Observer* obs) {
  observer_list_.RemoveObserver(obs);
}

void WebApp::LoadImagesFromWebData() {
  if (loaded_images_from_web_data_)
    return;

  loaded_images_from_web_data_ = true;
  WebDataService* service =
      profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (service)
    image_load_handle_ = service->GetWebAppImages(url_, this);
}

void WebApp::OnWebDataServiceRequestDone(WebDataService::Handle h,
                                         const WDTypedResult* r) {
  image_load_handle_ = 0;

  if (!r) {
    // Results are null if the database went away.
    return;
  }

  WDAppImagesResult result = reinterpret_cast<
      const WDResult<WDAppImagesResult>*>(r)->GetValue();
  images_.insert(images_.end(), result.images.begin(), result.images.end());

  if (!result.has_all_images) {
    // Not all of the images for the app have been downloaded yet; download them
    // now.
    DownloadImagesFromSite();
  } else {
    // We have all the images. Clear image_urls_ to indicate we've got all the
    // images.
    image_urls_.clear();
  }

  if (GetFavIconIterator() == images_.end()) {
    // No favicon. Request one from the history db.
    LoadFavIconFromHistory();
  }

  if (!images_.empty())
    NotifyObservers();
}

void WebApp::OnFavIconFromHistory(HistoryService::Handle handle,
                                  bool know_favicon,
                                  scoped_refptr<RefCountedBytes> data,
                                  bool expired,
                                  GURL icon_url) {
  // Make sure we still don't have a favicon.
  if (GetFavIconIterator() != images_.end() || !data || data->data.empty())
    return;

  SkBitmap fav_icon;
  if (PNGDecoder::Decode(&data->data, &fav_icon)) {
    images_.push_back(fav_icon);
    NotifyObservers();
  }
}

void WebApp::LoadFavIconFromHistory() {
  HistoryService* service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!service)
    return;

  service->GetFavIconForURL(url_, &request_consumer_,
                            NewCallback(this, &WebApp::OnFavIconFromHistory));
}

void WebApp::DownloadImagesFromSite() {
  if (!download_images_)
    return;

  RenderViewHost* rvh =
      web_contents_ ? web_contents_->render_view_host() : NULL;
  if (!rvh)
    return;

  // Copy off the images to load as we may need to mutate image_urls_ while
  // iterating.
  std::set<GURL> image_urls = image_urls_;
  for (std::set<GURL>::iterator i = image_urls.begin(); i != image_urls.end();
       ++i) {
    const GURL image_url = *i;
    SkBitmap data_image = DecodePNGEncodedURL(image_url);
    if (!data_image.isNull())
      SetImage(image_url, data_image);
    else if (rvh)
      rvh->DownloadImage(image_url, 0);  // Download the image via the renderer.
  }

  if (image_urls_.empty()) {
    // We got all the images immediately, notifiy the web db.
    WebDataService* service =
        profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (service)
      service->SetWebAppHasAllImages(url_, true);
  }
}

WebApp::Images::iterator WebApp::GetFavIconIterator() {
  for (Images::iterator i = images_.begin(); i != images_.end(); ++i) {
    if (i->width() == kFavIconSize && i->height() == kFavIconSize)
      return i;
  }
  return images_.end();
}

void WebApp::ExtractPNGEncodedURLs() {
  for (std::set<GURL>::iterator i = image_urls_.begin();
       i != image_urls_.end();) {
    const GURL image_url = *i;
    SkBitmap data_image = DecodePNGEncodedURL(image_url);
    if (!data_image.isNull()) {
      i = image_urls_.erase(i);
      images_.push_back(data_image);
    } else {
      ++i;
    }
  }
}

void WebApp::NotifyObservers() {
  FOR_EACH_OBSERVER(Observer, observer_list_, WebAppImagesChanged(this));
}

