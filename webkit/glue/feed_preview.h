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

// URLs of the form "feed://foo" are implemented by handing an "http://"
// URL up to the resource fetching code, then generating a preview at this
// layer and handing that back to WebKit.

#ifndef WEBKIT_GLUE_FEED_PREVIEW_H__
#define WEBKIT_GLUE_FEED_PREVIEW_H__

#include <string>

#pragma warning(push, 0)
#include "ResourceHandleClient.h"
#pragma warning(pop)

namespace WebCore {

// FeedClientProxy serves as a ResourceHandleClient that forwards calls to
// a "real" ResourceHandleClient, buffering the response so it can provide
// a feed preview if the underlying resource request succeeds.
class FeedClientProxy : public ResourceHandleClient {
 public:
  FeedClientProxy(ResourceHandleClient* client);
  virtual ~FeedClientProxy() { }

  // ResourceHandleClient overrides.
  virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
  virtual void didReceiveData(ResourceHandle*, const char*, int, int);
  virtual void didFinishLoading(ResourceHandle*);
  virtual void didFail(ResourceHandle*, const ResourceError&);

 private:
  // The "real" ResourceHandleClient that we're forwarding responses to.
  ResourceHandleClient* client_;

  // Whether we should insert a feed preview -- only if the request came
  // back ok.
  bool do_feed_preview_;

  // The response data, which we can parse for the feed preview.
  std::string data_;

  // The value of the mystery lengthReceived parameter.  We accept this via
  // didReceiveData() and forward it along unmodified.
  // TODO(evanm): do the right thing here, once we know what that is.
  // (See TODOs in resource_handle_win.cc.)
  int length_received_;
};

}  // namespace WebCore

#endif  // WEBKIT_GLUE_FEED_PREVIEW_H__
