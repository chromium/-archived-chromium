// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "webkit/glue/plugins/plugin_stream_url.h"

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "webkit/glue/plugins/plugin_instance.h"

namespace NPAPI {

PluginStreamUrl::PluginStreamUrl(
    int resource_id,
    const GURL &url,
    PluginInstance *instance, 
    bool notify_needed,
    void *notify_data)
    : PluginStream(instance, url.spec().c_str(), notify_needed, notify_data),
      url_(url),
      id_(resource_id) {
}

PluginStreamUrl::~PluginStreamUrl() {
}

bool PluginStreamUrl::Close(NPReason reason) {
  CancelRequest();
  bool result = PluginStream::Close(reason);
  instance()->RemoveStream(this);
  return result;
}

void PluginStreamUrl::WillSendRequest(const GURL& url) {
  url_ = url;
  UpdateUrl(url.spec().c_str());
}

void PluginStreamUrl::DidReceiveResponse(const std::string& mime_type,
                                         const std::string& headers,
                                         uint32 expected_length,
                                         uint32 last_modified,
                                         bool request_is_seekable,
                                         bool* cancel) {
  bool opened = Open(mime_type,
                     headers,
                     expected_length,
                     last_modified,
                     request_is_seekable);
  if (!opened) {
    instance()->RemoveStream(this);
    *cancel = true;
  }
}

void PluginStreamUrl::DidReceiveData(const char* buffer, int length,
                                     int data_offset) {
  if (!open())
    return;

  if (length > 0)
    Write(const_cast<char*>(buffer), length, data_offset);
}

void PluginStreamUrl::DidFinishLoading() {
  if (!seekable()) {
    Close(NPRES_DONE);
  }
}

void PluginStreamUrl::DidFail() {
  Close(NPRES_NETWORK_ERR);
}

void PluginStreamUrl::CancelRequest() {
  if (id_ > 0) {
    if (instance()->webplugin()) {
      instance()->webplugin()->CancelResource(id_);
    }
    id_ = 0;
  }
}

} // namespace NPAPI

