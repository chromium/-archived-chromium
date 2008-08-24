// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_DATA_STREAM_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_DATA_STREAM_H__

#include "webkit/glue/plugins/plugin_stream.h"

namespace NPAPI {

class PluginInstance;

// A NPAPI stream based on data received from the renderer.
class PluginDataStream : public PluginStream {
 public:
  // Create a new stream for sending to the plugin.
  PluginDataStream(PluginInstance *instance, const std::string& url,
                     const std::string& mime_type, const std::string& headers,
                     uint32 expected_length, uint32 last_modified);
  virtual ~PluginDataStream();

  // Initiates the sending of data to the plugin.
  void SendToPlugin(const char* buffer, int length);

 private:
  std::string mime_type_;
  std::string headers_;
  uint32 expected_length_;
  uint32 last_modified_;
  // This flag when set serves as an indicator that subsequent 
  // data coming from the renderer should not be handed off to the plugin.
  bool stream_open_failed_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginDataStream);
};

} // namespace NPAPI

#endif // WEBKIT_GLUE_PLUGIN_PLUGIN_DATA_STREAM_H__


