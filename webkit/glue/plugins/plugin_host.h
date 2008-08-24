// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Need mechanism to cleanup the static instance

#ifndef WEBKIT_GLUE_PLUGIN_PLUGIN_HOST_H__
#define WEBKIT_GLUE_PLUGIN_PLUGIN_HOST_H__

#include <string>
#include <vector>
#include <map>

#include "base/ref_counted.h"
#include "base/gfx/rect.h"
#include "base/task.h"
#include "webkit/glue/plugins/nphostapi.h"
#include "third_party/npapi/bindings/npapi.h"

namespace NPAPI
{
class PluginInstance;

// The Plugin Host implements the NPN_xxx functions for NPAPI plugins.
// These are the functions exposed from the Plugin Host for use 
// by the Plugin.
//
// The PluginHost is managed as a singleton.  This isn't strictly 
// necessary, but since the callback functions are all global C 
// functions, there is really no point in having per-instance PluginHosts.
class PluginHost : public base::RefCounted<PluginHost> {
 public:
  // Access the single PluginHost instance.  Callers
  // must call deref() when finished with the object.
  static PluginHost *Singleton();
  virtual ~PluginHost();
  
  // The table of functions provided to the plugin.
  NPNetscapeFuncs *host_functions() { return &host_funcs_; }

  // Helper function for parsing post headers, and applying attributes 
  // to the stream.  NPAPI post data include headers + data combined.  
  // This function parses it out and adds it to the stream in a WebKit 
  // style.
  static bool SetPostData(const char *buf,
                          uint32 length,
                          std::vector<std::string>* names,
                          std::vector<std::string>* values,
                          std::vector<char>* body);

  void PatchNPNetscapeFuncs(NPNetscapeFuncs* overrides);

  // Handles invalidateRect requests for windowless plugins.
  void InvalidateRect(NPP id, NPRect* invalidRect);

 private:
  PluginHost();
  void InitializeHostFuncs();
  // For certain plugins like flash we need to throttle invalidateRect
  // requests as they are made at a high frequency. 
  void OnInvalidateRect(NPP id, PluginInstance* instance);

  static scoped_refptr<PluginHost> singleton_;
  NPNetscapeFuncs host_funcs_;
  DISALLOW_EVIL_CONSTRUCTORS(PluginHost);

  // This structure keeps track of individual plugin instance invalidates.
  struct ThrottledInvalidates {
    std::vector<gfx::Rect> throttled_invalidates;
  };

  // We need to track throttled invalidate rects on a per plugin instance
  // basis. 
  typedef std::map<NPP, ThrottledInvalidates> InstanceThrottledInvalidatesMap;
  InstanceThrottledInvalidatesMap instance_throttled_invalidates_;

  ScopedRunnableMethodFactory<PluginHost> throttle_factory_;
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGIN_PLUGIN_HOST_H__

