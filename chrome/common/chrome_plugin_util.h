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

#ifndef CHROME_COMMON_CHROME_PLUGIN_UTIL_H__
#define CHROME_COMMON_CHROME_PLUGIN_UTIL_H__

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/common/chrome_plugin_api.h"
#include "chrome/common/notification_service.h"

class ChromePluginLib;
class MessageLoop;
namespace net{
class HttpResponseHeaders;
}

// A helper struct to ensure the CPRequest data is cleaned up when done.
// This class is reused for requests made by the browser (and intercepted by the
// plugin) as well as those made by the plugin.
struct ScopableCPRequest : public CPRequest {
  template<class T>
  static T GetData(CPRequest* request) {
    return static_cast<T>(static_cast<ScopableCPRequest*>(request)->data);
  }

  ScopableCPRequest(const char* url, const char* method,
                    CPBrowsingContext context);
  ~ScopableCPRequest();

  void* data;
};

// This is a base class for plugin-related objects that need to go away when
// the plugin unloads.  This object also verifies that it is created and
// destroyed on the same thread.
class PluginHelper : public NotificationObserver {
 public:
  static void DestroyAllHelpersForPlugin(ChromePluginLib* plugin);

  PluginHelper(ChromePluginLib* plugin);
  virtual ~PluginHelper();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  scoped_refptr<ChromePluginLib> plugin_;
#ifndef NDEBUG
  // We keep track of the message loop of the thread we were created on, so
  // we can verify that all other methods are called on the same thread.
  MessageLoop* message_loop_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(PluginHelper);
};

// A class of utility functions for dealing with request responses.
class PluginResponseUtils {
public:
  // Helper to convert request load flags from the plugin API to the net API
  // versions.
  static uint32 CPLoadFlagsToNetFlags(uint32 flags);

  // Common implementation of a CPR_GetResponseInfo call.
  static int GetResponseInfo(
      const net::HttpResponseHeaders* response_headers,
      CPResponseInfoType type, void* buf, uint32 buf_size);
};

// Helper to allocate a string using the given CPB_Alloc function.
inline char* CPB_StringDup(CPB_AllocFunc alloc, const std::string& str) {
  char* cstr = static_cast<char*>(alloc(static_cast<uint32>(str.length() + 1)));
  memcpy(cstr, str.c_str(), str.length() + 1);  // Include null terminator.
  return cstr;
}

CPError CPB_GetCommandLineArgumentsCommon(const char* url,
                                          std::string* arguments);

void* STDCALL CPB_Alloc(uint32 size);
void STDCALL CPB_Free(void* memory);

#endif  // CHROME_COMMON_CHROME_PLUGIN_UTIL_H__
