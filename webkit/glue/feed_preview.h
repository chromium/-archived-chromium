// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// URLs of the form "feed://foo" are implemented by handing an "http://"
// URL up to the resource fetching code, then generating a preview at this
// layer and handing that back to WebKit.

#ifndef WEBKIT_GLUE_FEED_PREVIEW_H__
#define WEBKIT_GLUE_FEED_PREVIEW_H__

#include <string>

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "ResourceHandleClient.h"
MSVC_POP_WARNING();

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
