// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "base/compiler_specific.h"

#include "webkit/glue/feed_preview.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ResourceHandle.h"
MSVC_POP_WARNING();

#undef LOG

#include "base/logging.h"
#include "grit/webkit_resources.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

namespace WebCore {

// Generate a preview for a feed.  |url| is the (http://) URL of the feed,
// and |data| are the bytes we received in response to the HTTP request.
// Returns an HTML string.
static std::string MakeFeedPreview(const std::string& url,
                                   const std::string& data) {
  // TODO(evanm): this is just a placeholder.
  // Maybe we should make this parse the feed data and display a preview?
  // Yuck.  Seems like a lot of effort for a pretty minor feature.

  // The feed preview template has {{URL}} in place of where the URL should go.
  const std::string kUrlTemplate = "{{URL}}";
  std::string feed_template(webkit_glue::GetDataResource(IDR_FEED_PREVIEW));
  std::string::size_type template_offset = feed_template.find(kUrlTemplate);
  DCHECK(template_offset != std::string::npos);
  // TODO(evanm): URL-escape URL!
  return feed_template.substr(0, template_offset) + url +
         feed_template.substr(template_offset + kUrlTemplate.size());
}

FeedClientProxy::FeedClientProxy(ResourceHandleClient* client)
    : client_(client), do_feed_preview_(false) {
}

void FeedClientProxy::didReceiveResponse(ResourceHandle* handle,
                                         const ResourceResponse& response) {
  if (response.httpStatusCode() == 200) {
    ResourceResponse new_response(response);
    // Our feed preview has mime type text/html.
    new_response.setMimeType("text/html");
    do_feed_preview_ = true;
    client_->didReceiveResponse(handle, new_response);
  } else {
    client_->didReceiveResponse(handle, response);
  }
}

void FeedClientProxy::didReceiveData(ResourceHandle*, const char* data,
                                     int data_len, int length_received) {
  length_received_ = length_received;
  data_.append(data, data_len);
}

void FeedClientProxy::didFinishLoading(ResourceHandle* handle) {
  const std::string url =
      webkit_glue::KURLToGURL(handle->request().url()).spec();
  const std::string& data =
      do_feed_preview_ ? MakeFeedPreview(url, data_) : data_;
  client_->didReceiveData(handle, data.data(), data.size(), length_received_);
  client_->didFinishLoading(handle);
}

void FeedClientProxy::didFail(ResourceHandle* handle,
                              const ResourceError& error) {
  client_->didFail(handle, error);
}

}  // namespace WebCore

