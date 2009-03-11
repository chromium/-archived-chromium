// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ImageSourceSkia.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/image_resource_fetcher.h"

#include "base/gfx/size.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/webview_impl.h"
#include "skia/include/SkBitmap.h"

ImageResourceFetcher::ImageResourceFetcher(
    WebViewImpl* web_view,
    int id,
    const GURL& image_url,
    int image_size)
    : web_view_(web_view),
      id_(id),
      image_url_(image_url),
      image_size_(image_size) {
  fetcher_.reset(new ResourceFetcher(image_url,
                                     web_view->main_frame()->frame(),
                                     this));
}

ImageResourceFetcher::~ImageResourceFetcher() {
  if (!fetcher_->completed())
    fetcher_->Cancel();
}

void ImageResourceFetcher::OnURLFetchComplete(
    const WebCore::ResourceResponse& response,
    const std::string& data) {
  SkBitmap image;
  bool errored = false;
  if (response.isNull()) {
    errored = true;
  } else if (response.httpStatusCode() == 200) {
    // Request succeeded, try to convert it to an image.
    webkit_glue::ImageDecoder decoder(gfx::Size(image_size_, image_size_));
    image = decoder.Decode(
        reinterpret_cast<const unsigned char*>(data.data()), data.size());
  } // else case:
    // If we get here, it means no image from server or couldn't decode the
    // response as an image. Need to notify the webview though, otherwise the
    // browser will keep trying to download favicon (when this is used to
    // download the favicon).
  web_view_->ImageResourceDownloadDone(this, errored, image);
}
