// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_IMAGE_RESOURCE_FETCHER_H_
#define WEBKIT_GLUE_IMAGE_RESOURCE_FETCHER_H_

#include "base/basictypes.h"
#include "webkit/glue/resource_fetcher.h"

class SkBitmap;
class WebViewImpl;

namespace webkit_glue {

// ImageResourceFetcher handles downloading an image for a webview. Once
// downloading is done the hosting WebViewImpl is notified. ImageResourceFetcher
// is used to download the favicon and images for web apps.
class ImageResourceFetcher {
 public:
  typedef Callback2<ImageResourceFetcher*, const SkBitmap&>::Type Callback;

  ImageResourceFetcher(const GURL& image_url,
                       WebFrame* frame,
                       int id,
                       int image_size,
                       Callback* callback);

  virtual ~ImageResourceFetcher();

  // URL of the image we're downloading.
  const GURL& image_url() const { return image_url_; }

  // Unique identifier for the request.
  int id() const { return id_; }

 private:
  // ResourceFetcher::Callback. Decodes the image and invokes callback_.
  void OnURLFetchComplete(const WebKit::WebURLResponse& response,
                          const std::string& data);

  Callback* callback_;

  // Unique identifier for the request.
  const int id_;

  // URL of the image.
  const GURL image_url_;

  // The size of the image. This is only a hint that is used if the image
  // contains multiple sizes. A value of 0 results in using the first frame
  // of the image.
  const int image_size_;

  // Does the actual download.
  scoped_ptr<ResourceFetcher> fetcher_;

  DISALLOW_EVIL_CONSTRUCTORS(ImageResourceFetcher);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_IMAGE_RESOURCE_FETCHER_H_
