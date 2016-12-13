// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "webkit/glue/plugins/plugin_string_stream.h"

namespace NPAPI {

PluginStringStream::PluginStringStream(
    PluginInstance *instance,
    const std::string &url,
    bool notify_needed,
    void *notify_data)
    : PluginStream(instance, url.c_str(), notify_needed, notify_data) {
}

PluginStringStream::~PluginStringStream() {
}

void PluginStringStream::SendToPlugin(const std::string &data,
                                      const std::string &mime_type) {
  int length = static_cast<int>(data.length());
  if (Open(mime_type, std::string(), length, 0, false)) {
    // TODO - check if it was not fully sent, and figure out a backup plan.
    int written = Write(data.c_str(), length, 0);
    NPReason reason = written == length ? NPRES_DONE : NPRES_NETWORK_ERR;
    Close(reason);
  }
}

}
