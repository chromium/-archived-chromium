// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/plugin_data_stream.h"

#include "base/logging.h"

namespace NPAPI {

PluginDataStream::PluginDataStream(PluginInstance *instance, 
                                   const std::string& url,
                                   const std::string& mime_type,
                                   const std::string& headers,
                                   uint32 expected_length,
                                   uint32 last_modified)
    : PluginStream(instance, url.c_str(), false, 0),
      mime_type_(mime_type),
      headers_(headers),
      expected_length_(expected_length),
      last_modified_(last_modified),
      stream_open_failed_(false) {
}

PluginDataStream::~PluginDataStream() {
}

void PluginDataStream::SendToPlugin(const char* buffer, int length) {
  if (stream_open_failed_)
    return;

  if (!open()) {
    if (!Open(mime_type_, headers_, expected_length_, last_modified_)) {
      stream_open_failed_ = true;
      return;
    }
  }

  // TODO(iyengar) - check if it was not fully sent, and figure out a 
  // backup plan.
  int written = Write(buffer, length);
  DCHECK(written == length);
}

} // namespace NPAPI

