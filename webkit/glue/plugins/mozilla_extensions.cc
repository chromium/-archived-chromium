// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/mozilla_extensions.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"
#include "webkit/glue/plugins/plugin_instance.h"

#define QI_SUPPORTS_IID(iid, iface)                                           \
  QI_SUPPORTS_IID_(iid, iface::GetIID(), iface)

#define QI_SUPPORTS_IID_(src_iid, iface_iid, iface)                           \
  if (iid.Equals(iface_iid)) {                                                \
    AddRef();                                                                 \
    *result = static_cast<iface*>(this);                                      \
    return NS_OK;                                                             \
  }

namespace NPAPI
{

void MozillaExtensionApi::DetachFromInstance() {
  plugin_instance_ = NULL;
}

bool MozillaExtensionApi::FindProxyForUrl(const char* url,
                                          std::string* proxy) {
  if ((!url) || (!proxy)) {
    NOTREACHED();
    return false;
  }

  return webkit_glue::FindProxyForUrl(GURL(std::string(url)), proxy);
}

// nsISupports implementation
NS_IMETHODIMP MozillaExtensionApi::QueryInterface(REFNSIID iid,
                                                  void** result) {
  static const nsIID knsISupportsIID = NS_ISUPPORTS_IID;
  QI_SUPPORTS_IID_(iid, knsISupportsIID, nsIServiceManager)
  QI_SUPPORTS_IID(iid, nsIServiceManager)
  QI_SUPPORTS_IID(iid, nsIPluginManager)
  QI_SUPPORTS_IID(iid, nsIPluginManager2)
  QI_SUPPORTS_IID(iid, nsICookieStorage)

  NOTREACHED();
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP_(nsrefcnt) MozillaExtensionApi::AddRef(void) {
  return InterlockedIncrement(reinterpret_cast<LONG*>(&ref_count_));
}

NS_IMETHODIMP_(nsrefcnt) MozillaExtensionApi::Release(void) {
  DCHECK(static_cast<int>(ref_count_) > 0);
  if (InterlockedDecrement(reinterpret_cast<LONG*>(&ref_count_)) == 0) {
    delete this;
    return 0;
  }

  return ref_count_;
}

NS_IMETHODIMP MozillaExtensionApi::GetService(REFNSIID class_guid,
                                              REFNSIID iid,
                                              void** result) {

  static const nsIID kPluginManagerCID = NS_PLUGINMANAGER_CID;
  static const nsIID kCookieStorageCID = NS_COOKIESTORAGE_CID;

  nsresult rv = NS_ERROR_FAILURE;

  if ((class_guid.Equals(kPluginManagerCID)) ||
      (class_guid.Equals(kCookieStorageCID))) {
    rv = QueryInterface(iid, result);
  }

  DCHECK(rv == NS_OK);
  return rv;
}

NS_IMETHODIMP MozillaExtensionApi::GetServiceByContractID(
    const char* contract_id,
    REFNSIID iid,
    void** result) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::IsServiceInstantiated(REFNSIID class_guid,
                                                         REFNSIID iid,
                                                         PRBool* result) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::IsServiceInstantiatedByContractID(
    const char* contract_id,
    REFNSIID iid,
    PRBool* result) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP MozillaExtensionApi::GetValue(nsPluginManagerVariable variable,
                                            void * value) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::ReloadPlugins(PRBool reloadPages) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::UserAgent(
    const char** resultingAgentString) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::GetURL(
    nsISupports* pluginInst,
    const char* url,
    const char* target,
    nsIPluginStreamListener* streamListener,
    const char* altHost,
    const char* referrer,
    PRBool forceJSEnabled) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::PostURL(
    nsISupports* pluginInst,
    const char* url,
    unsigned int postDataLen,
    const char* postData,
    PRBool isFile,
    const char* target,
    nsIPluginStreamListener* streamListener,
    const char* altHost,
    const char* referrer,
    PRBool forceJSEnabled ,
    unsigned int postHeadersLength,
    const char* postHeaders) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::RegisterPlugin(
    REFNSIID aCID,
    const char *aPluginName,
    const char *aDescription,
    const char * * aMimeTypes,
    const char * * aMimeDescriptions,
    const char * * aFileExtensions,
    PRInt32 aCount) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::UnregisterPlugin(REFNSIID aCID) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::GetURLWithHeaders(
    nsISupports* pluginInst,
    const char* url,
    const char* target /* = NULL */,
    nsIPluginStreamListener* streamListener /* = NULL */,
    const char* altHost /* = NULL */,
    const char* referrer /* = NULL */,
    PRBool forceJSEnabled /* = PR_FALSE */,
    PRUint32 getHeadersLength /* = 0 */,
    const char* getHeaders /* = NULL */){
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

// nsIPluginManager2
NS_IMETHODIMP MozillaExtensionApi::BeginWaitCursor() {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::EndWaitCursor() {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::SupportsURLProtocol(const char* aProtocol,
                                                       PRBool* aResult) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::NotifyStatusChange(nsIPlugin* aPlugin,
                                                      nsresult aStatus) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::FindProxyForURL(
    const char* aURL,
    char** aResult) {
  std::string proxy = "DIRECT";
  FindProxyForUrl(aURL, &proxy);

  // Allocate this using the NPAPI allocator. The plugin will call
  // NPN_Free to free this.
  char* result = static_cast<char*>(NPN_MemAlloc(proxy.length() + 1));
  strncpy(result, proxy.c_str(), proxy.length() + 1);

  *aResult = result;
  return NS_OK;
}

NS_IMETHODIMP MozillaExtensionApi::RegisterWindow(
    nsIEventHandler* handler,
    nsPluginPlatformWindowRef window) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::UnregisterWindow(
    nsIEventHandler* handler,
    nsPluginPlatformWindowRef win) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::AllocateMenuID(nsIEventHandler* aHandler,
                                                  PRBool aIsSubmenu,
                                                  PRInt16 *aResult) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::DeallocateMenuID(nsIEventHandler* aHandler,
                                                    PRInt16 aMenuID) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP MozillaExtensionApi::HasAllocatedMenuID(nsIEventHandler* aHandler,
                                                      PRInt16 aMenuID,
                                                      PRBool* aResult) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

// nsICookieStorage
NS_IMETHODIMP MozillaExtensionApi::GetCookie(
    const char* url,
    void* cookie_buffer,
    PRUint32& buffer_size) {
  if ((!url) || (!cookie_buffer)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!plugin_instance_)
    return NS_ERROR_FAILURE;

  WebPlugin* webplugin = plugin_instance_->webplugin();
  if (!webplugin)
    return NS_ERROR_FAILURE;

  // Bypass third-party cookie blocking by using the url as the policy_url.
  GURL cookies_url((std::string(url)));
  std::string cookies = webplugin->GetCookies(cookies_url, cookies_url);

  if (cookies.empty())
    return NS_ERROR_FAILURE;

  if(cookies.length() >= buffer_size)
    return NS_ERROR_FAILURE;

  strncpy(static_cast<char*>(cookie_buffer),
          cookies.c_str(),
          cookies.length() + 1);

  buffer_size = cookies.length();
  return NS_OK;
}

NS_IMETHODIMP MozillaExtensionApi::SetCookie(
    const char* url,
    const void* cookie_buffer,
    PRUint32 buffer_size){
  if ((!url) || (!cookie_buffer) || (!buffer_size)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!plugin_instance_)
    return NS_ERROR_FAILURE;

  WebPlugin* webplugin = plugin_instance_->webplugin();
  if (!webplugin)
    return NS_ERROR_FAILURE;

  std::string cookie(static_cast<const char*>(cookie_buffer),
                     buffer_size);
  GURL cookies_url((std::string(url)));
  webplugin->SetCookie(cookies_url,
                       cookies_url,
                       cookie);
  return NS_OK;
}


} // namespace NPAPI
