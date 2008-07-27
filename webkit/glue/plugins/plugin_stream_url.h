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

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_STREAM_URL_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_STREAM_URL_H__


#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_stream.h"
#include "googleurl/src/gurl.h"

namespace NPAPI {

class PluginInstance;

// A NPAPI Stream based on a URL.
class PluginStreamUrl : public PluginStream, 
                        public WebPluginResourceClient {
 public:
  // Create a new stream for sending to the plugin by fetching
  // a URL. If notifyNeeded is set, then the plugin will be notified 
  // when the stream has been fully sent to the plugin.  Initialize
  // must be called before the object is used.  
  PluginStreamUrl(int resource_id,
                  const GURL &url, 
                  PluginInstance *instance, 
                  bool notify_needed,
                  void *notify_data);
  virtual ~PluginStreamUrl();

  // Stop sending the stream to the client.
  // Overrides the base Close so we can cancel our fetching the URL if
  // it is still loading.
  bool Close(NPReason reason);

  //
  // WebPluginResourceClient methods
  //
  void WillSendRequest(const GURL& url);
  void DidReceiveResponse(const std::string& mime_type,
                          const std::string& headers,
                          uint32 expected_length,
                          uint32 last_modified,
                          bool* cancel);
  void DidReceiveData(const char* buffer, int length);
  void DidFinishLoading();
  void DidFail();

 private:
  GURL url_;
  int id_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginStreamUrl);
};

} // namespace NPAPI

#endif // WEBKIT_GLUE_PLUGIN_PLUGIN_STREAM_URL_H__