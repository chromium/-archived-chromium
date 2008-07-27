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

