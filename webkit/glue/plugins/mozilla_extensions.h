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

#ifndef WEBKIT_GLUE_PLUGINS_MOZILLA_EXTENSIONS_H__
#define WEBKIT_GLUE_PLUGINS_MOZILLA_EXTENSIONS_H__

#include <windows.h>

// Include npapi first to avoid definition clashes due to 
// XP_WIN
#include "third_party/npapi/bindings/npapi.h"

#include "third_party/mozilla/include/nsIServiceManager.h"
#include "third_party/mozilla/include/nsIPluginManager2.h"
#include "third_party/mozilla/include/nsICookieStorage.h"
#include "third_party/mozilla/include/nsError.h"

#include "base/ref_counted.h"

// NS_DECL_NSIPLUGINMANAGER doesn't include methods described as "C++" in the
// nsIPluginManager.idl. 
#define NS_DECL_NSIPLUGINMANAGER_FIXED                                        \
  NS_DECL_NSIPLUGINMANAGER                                                    \
    NS_IMETHOD                                                                \
    GetURL(nsISupports* pluginInst,                                           \
           const char* url,                                                   \
           const char* target = NULL,                                         \
           nsIPluginStreamListener* streamListener = NULL,                    \
           const char* altHost = NULL,                                        \
           const char* referrer = NULL,                                       \
           PRBool forceJSEnabled = PR_FALSE);                                 \
    NS_IMETHOD                                                                \
    PostURL(nsISupports* pluginInst,                                          \
            const char* url,                                                  \
            PRUint32 postDataLen,                                             \
            const char* postData,                                             \
            PRBool isFile = PR_FALSE,                                         \
            const char* target = NULL,                                        \
            nsIPluginStreamListener* streamListener = NULL,                   \
            const char* altHost = NULL,                                       \
            const char* referrer = NULL,                                      \
            PRBool forceJSEnabled = PR_FALSE,                                 \
            PRUint32 postHeadersLength = 0,                                   \
            const char* postHeaders = NULL);                                  \
    NS_IMETHOD                                                                \
    GetURLWithHeaders(nsISupports* pluginInst,                                \
                      const char* url,                                        \
                      const char* target = NULL,                              \
                      nsIPluginStreamListener* streamListener = NULL,         \
                      const char* altHost = NULL,                             \
                      const char* referrer = NULL,                            \
                      PRBool forceJSEnabled = PR_FALSE,                       \
                      PRUint32 getHeadersLength = 0,                          \
                      const char* getHeaders = NULL);

// Avoid dependence on the nsIsupportsImpl.h and so on.
#ifndef NS_DECL_ISUPPORTS
#define NS_DECL_ISUPPORTS                                                     \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                         \
  NS_IMETHOD_(nsrefcnt) Release(void);
#endif // NS_DECL_ISUPPORTS

namespace NPAPI
{

class PluginInstance;

// Implementation of extended Mozilla interfaces needed to support 
// Sun's new Java plugin.
class MozillaExtensionApi : public nsIServiceManager,
                            public nsIPluginManager2,
                            public nsICookieStorage {
 public:
  MozillaExtensionApi(PluginInstance* plugin_instance) :
      plugin_instance_(plugin_instance), ref_count_(0) {
  }

  void DetachFromInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEMANAGER
  NS_DECL_NSIPLUGINMANAGER_FIXED
  NS_DECL_NSIPLUGINMANAGER2
  NS_DECL_NSICOOKIESTORAGE

 protected:
  bool FindProxyForUrl(const char* url, std::string* proxy);

 protected:
  scoped_refptr<NPAPI::PluginInstance> plugin_instance_;
  unsigned long ref_count_;
};

} // namespace NPAPI

#endif  // WEBKIT_GLUE_PLUGINS_MOZILLA_EXTENSIONS_H__
