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

#include "config.h"

#pragma warning(push, 0)
#include "ImageSourceSkia.h"
#pragma warning(pop)
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
