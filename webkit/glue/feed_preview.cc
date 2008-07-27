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

#include "webkit/glue/feed_preview.h"

#pragma warning(push, 0)
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ResourceHandle.h"
#pragma warning(pop)

#undef LOG

#include "base/logging.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkit_resources.h"

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
