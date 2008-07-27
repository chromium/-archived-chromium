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
  if (id_ != 0) {
    if (instance()->webplugin()) {
      instance()->webplugin()->CancelResource(id_);
    }

    id_ = 0;
  }

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
                                         bool* cancel) {

  bool opened = Open(mime_type,
                     headers,
                     expected_length,
                     last_modified);
  if (!opened) {
    instance()->RemoveStream(this);
    *cancel = true;
  }
}

void PluginStreamUrl::DidReceiveData(const char* buffer, int length) {
  if (!open())
    return;

  if (length > 0)
    Write(const_cast<char*>(buffer), length);
}

void PluginStreamUrl::DidFinishLoading() {
  Close(NPRES_DONE);
}

void PluginStreamUrl::DidFail() {
  Close(NPRES_NETWORK_ERR);
}

} // namespace NPAPI
