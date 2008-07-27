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
//
// Thes file contains stuff that should be shared among projects that do some
// special handling with default plugin

#ifndef WEBKIT_DEFAULT_PLUGIN_DEFAULT_PLUGIN_SHARED_H
#define WEBKIT_DEFAULT_PLUGIN_DEFAULT_PLUGIN_SHARED_H

namespace default_plugin {

// We use the NPNGetValue host function to send notification message to host.
// This corresponds to NPNVariable defined in npapi.h. However, we don't care
// too much about value conflicts because on the host side, it will only respond
// to such request coming from default plugin.
const int kMissingPluginStatusStart = 3000;

enum MissingPluginStatus {
  MISSING_PLUGIN_AVAILABLE,
  MISSING_PLUGIN_USER_STARTED_DOWNLOAD
};

}  // namespace activex_shim

#endif // #ifndef WEBKIT_DEFAULT_PLUGIN_DEFAULT_PLUGIN_SHARED_H
