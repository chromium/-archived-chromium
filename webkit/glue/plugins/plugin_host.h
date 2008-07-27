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
