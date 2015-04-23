// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_STRING_STREAM_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_STRING_STREAM_H__

#include "webkit/glue/plugins/plugin_stream.h"

namespace NPAPI {

class PluginInstance;

// An NPAPI stream from a string.
class PluginStringStream : public PluginStream {
 public:
  // Create a new stream for sending to the plugin.
  // If notify_needed, will notify the plugin after the data has
  // all been sent.
  PluginStringStream(PluginInstance *instance,
                     const std::string &url,
                     bool notify_needed,
                     void *notify_data);
  virtual ~PluginStringStream();

  // Initiates the sending of data to the plugin.
  void SendToPlugin(const std::string &data,
                    const std::string &mime_type);

 private:

  DISALLOW_EVIL_CONSTRUCTORS(PluginStringStream);
};

} // namespace NPAPI

#endif // WEBKIT_GLUE_PLUGIN_PLUGIN_STRING_STREAM_H__
