// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  virtual bool Close(NPReason reason);

  virtual WebPluginResourceClient* AsResourceClient() {
    return static_cast<WebPluginResourceClient*>(this);
  }

  virtual void CancelRequest();

  //
  // WebPluginResourceClient methods
  //
  void WillSendRequest(const GURL& url);
  void DidReceiveResponse(const std::string& mime_type,
                          const std::string& headers,
                          uint32 expected_length,
                          uint32 last_modified,
                          bool request_is_seekable,
                          bool* cancel);
  void DidReceiveData(const char* buffer, int length, int data_offset);
  void DidFinishLoading();
  void DidFail();
  bool IsMultiByteResponseExpected() {
    return seekable();
  }


 private:
  GURL url_;
  int id_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginStreamUrl);
};

} // namespace NPAPI

#endif // WEBKIT_GLUE_PLUGIN_PLUGIN_STREAM_URL_H__
