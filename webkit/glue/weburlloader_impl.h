// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLLOADER_IMPL_H_
#define WEBKIT_GLUE_WEBURLLOADER_IMPL_H_

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "webkit/api/public/WebURLLoader.h"
#include "webkit/glue/multipart_response_delegate.h"
#include "webkit/glue/resource_loader_bridge.h"

namespace webkit_glue {

class WebURLLoaderImpl : public WebKit::WebURLLoader,
                         public ResourceLoaderBridge::Peer {
 public:
  WebURLLoaderImpl();
  ~WebURLLoaderImpl();

  // WebURLLoader methods:
  virtual void loadSynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLResponse& response,
      WebKit::WebURLError& error,
      WebKit::WebData& data);
  virtual void loadAsynchronously(
      const WebKit::WebURLRequest& request,
      WebKit::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool value);

  // ResourceLoaderBridge::Peer methods:
  virtual void OnUploadProgress(uint64 position, uint64 size);
  virtual void OnReceivedRedirect(const GURL& new_url);
  virtual void OnReceivedResponse(
      const ResourceLoaderBridge::ResponseInfo& info, bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(
      const URLRequestStatus& status, const std::string& security_info);
  virtual std::string GetURLForDebugging();

 private:
  void Start(
      const WebKit::WebURLRequest& request,
      ResourceLoaderBridge::SyncLoadResponse* sync_load_response);
  void HandleDataURL();

  ScopedRunnableMethodFactory<WebURLLoaderImpl> task_factory_;

  GURL url_;
  WebKit::WebURLLoaderClient* client_;
  scoped_ptr<ResourceLoaderBridge> bridge_;
  scoped_ptr<MultipartResponseDelegate> multipart_delegate_;
  int64 expected_content_length_;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBURLLOADER_IMPL_H_
