// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WEBKIT_GLUE_IMAGE_RESOURCE_FETCHER_H__
#define WEBKIT_GLUE_IMAGE_RESOURCE_FETCHER_H__

#include "base/basictypes.h"
#include "webkit/glue/resource_fetcher.h"

class SkBitmap;
class WebCore::ResourceResponse;
class WebViewImpl;

// ImageResourceFetcher handles downloading an image for a webview. Once
// downloading is done the hosting WebViewImpl is notified. ImageResourceFetcher
// is used to download the favicon and images for web apps.
class ImageResourceFetcher : public ResourceFetcher::Delegate {
 public:
  ImageResourceFetcher(WebViewImpl* web_view,
                       int id,
                       const GURL& image_url,
                       int image_size);

  virtual ~ImageResourceFetcher();

  // ResourceFetcher::Delegate method. Decodes the image and invokes one of
  // DownloadFailed or DownloadedImage.
  virtual void OnURLFetchComplete(const WebCore::ResourceResponse& response,
                                  const std::string& data);

  // URL of the image we're downloading.
  const GURL& image_url() const { return image_url_; }

  // Hosting WebView.
  WebViewImpl* web_view() const { return web_view_; }

  // Unique identifier for the request.
  int id() const { return id_; }

 private:
  WebViewImpl* web_view_;

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

#endif  // WEBKIT_GLUE_IMAGE_RESOURCE_FETCHER_H__
