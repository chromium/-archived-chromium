// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include "base/scoped_cftyperef.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "net/base/net_errors.h"

namespace {

// Utility function to pull out a value from a dictionary, check its type, and
// return it.  Returns NULL if the key is not present or of the wrong type.
CFTypeRef GetValueFromDictionary(CFDictionaryRef dict,
                                 CFStringRef key,
                                 CFTypeID expected_type) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (!value)
    return value;
  
  if (CFGetTypeID(value) != expected_type) {
    scoped_cftyperef<CFStringRef> expected_type_ref(
        CFCopyTypeIDDescription(expected_type));
    scoped_cftyperef<CFStringRef> actual_type_ref(
        CFCopyTypeIDDescription(CFGetTypeID(value)));
    LOG(WARNING) << "Expected value for key "
                 << base::SysCFStringRefToUTF8(key)
                 << " to be "
                 << base::SysCFStringRefToUTF8(expected_type_ref)
                 << " but it was "
                 << base::SysCFStringRefToUTF8(actual_type_ref)
                 << " instead";
    return NULL;
  }
  
  return value;
}

// Utility function to pull out a boolean value from a dictionary and return it,
// returning a default value if the key is not present.
bool GetBoolFromDictionary(CFDictionaryRef dict,
                           CFStringRef key,
                           bool default_value) {
  CFNumberRef number = (CFNumberRef)GetValueFromDictionary(dict, key,
                                                           CFNumberGetTypeID());
  if (!number)
    return default_value;
  
  int int_value;
  if (CFNumberGetValue(number, kCFNumberIntType, &int_value))
    return int_value;
  else
    return default_value;
}

// Utility function to pull out a host/port pair from a dictionary and format
// them into a "<host>[:<port>]" style string. Pass in a dictionary that has a
// value for the host key and optionally a value for the port key. Returns a
// formatted string. In the error condition where the host value is especially
// malformed, returns an empty string. (You may still want to check for that
// result anyway.)
std::string GetHostPortFromDictionary(CFDictionaryRef dict,
                                      CFStringRef host_key,
                                      CFStringRef port_key) {
  std::string result;
  
  CFStringRef host_ref =
      (CFStringRef)GetValueFromDictionary(dict, host_key,
                                          CFStringGetTypeID());
  if (!host_ref) {
    LOG(WARNING) << "Could not find expected key "
                 << base::SysCFStringRefToUTF8(host_key)
                 << " in the proxy dictionary";
    return result;
  }
  result = base::SysCFStringRefToUTF8(host_ref);
  
  CFNumberRef port_ref =
      (CFNumberRef)GetValueFromDictionary(dict, port_key,
                                          CFNumberGetTypeID());
  if (port_ref) {
    int port;
    CFNumberGetValue(port_ref, kCFNumberIntType, &port);
    result += ":";
    result += IntToString(port);
  }
  
  return result;
}

// Callback for CFNetworkExecuteProxyAutoConfigurationURL. |client| is a pointer
// to a CFTypeRef.  This stashes either |error| or |proxies| in that location.
void ResultCallback(void* client, CFArrayRef proxies, CFErrorRef error) {
  DCHECK((proxies != NULL) == (error == NULL));
  
  CFTypeRef* result_ptr = (CFTypeRef*)client;
  DCHECK(result_ptr != NULL);
  DCHECK(*result_ptr == NULL);
  
  if (error != NULL) {
    *result_ptr = CFRetain(error);
  } else {
    *result_ptr = CFRetain(proxies);
  }
  CFRunLoopStop(CFRunLoopGetCurrent());
}  

}  // namespace

namespace net {

int ProxyConfigServiceMac::GetProxyConfig(ProxyConfig* config) {
  scoped_cftyperef<CFDictionaryRef> config_dict(
      SCDynamicStoreCopyProxies(NULL));
  DCHECK(config_dict);
  
  // auto-detect
  
  // There appears to be no UI for this configuration option, and we're not sure
  // if Apple's proxy code even takes it into account. But the constant is in
  // the header file so we'll use it.
  config->auto_detect =
      GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesProxyAutoDiscoveryEnable,
                            false);
  
  // PAC file
  
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesProxyAutoConfigEnable,
                            false)) {
    CFStringRef pac_url_ref =
        (CFStringRef)GetValueFromDictionary(
            config_dict.get(),
            kSCPropNetProxiesProxyAutoConfigURLString,
            CFStringGetTypeID());
    if (pac_url_ref)
      config->pac_url = GURL(base::SysCFStringRefToUTF8(pac_url_ref));
  }
  
  // proxies (for now only ftp, http and https)
  
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesFTPEnable,
                            false)) {
    std::string host_port =
        GetHostPortFromDictionary(config_dict.get(),
                                  kSCPropNetProxiesFTPProxy,
                                  kSCPropNetProxiesFTPPort);
    if (!host_port.empty()) {
      config->proxy_rules += "ftp=";
      config->proxy_rules += host_port;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesHTTPEnable,
                            false)) {
    std::string host_port =
        GetHostPortFromDictionary(config_dict.get(),
                                  kSCPropNetProxiesHTTPProxy,
                                  kSCPropNetProxiesHTTPPort);
    if (!host_port.empty()) {
      if (!config->proxy_rules.empty())
        config->proxy_rules += ";";
      config->proxy_rules += "http=";
      config->proxy_rules += host_port;
    }
  }
  if (GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesHTTPSEnable,
                            false)) {
    std::string host_port =
        GetHostPortFromDictionary(config_dict.get(),
                                  kSCPropNetProxiesHTTPSProxy,
                                  kSCPropNetProxiesHTTPSPort);
    if (!host_port.empty()) {
      if (!config->proxy_rules.empty())
        config->proxy_rules += ";";
      config->proxy_rules += "https=";
      config->proxy_rules += host_port;
    }
  }
  
  // proxy bypass list
  
  CFArrayRef bypass_array_ref =
      (CFArrayRef)GetValueFromDictionary(config_dict.get(),
                                         kSCPropNetProxiesExceptionsList,
                                         CFArrayGetTypeID());
  if (bypass_array_ref) {
    CFIndex bypass_array_count = CFArrayGetCount(bypass_array_ref);
    for (CFIndex i = 0; i < bypass_array_count; ++i) {
      CFStringRef bypass_item_ref =
          (CFStringRef)CFArrayGetValueAtIndex(bypass_array_ref, i);
      if (CFGetTypeID(bypass_item_ref) != CFStringGetTypeID()) {
        LOG(WARNING) << "Expected value for item " << i
                     << " in the kSCPropNetProxiesExceptionsList"
                        " to be a CFStringRef but it was not";      
        
      } else {
        config->proxy_bypass.push_back(
            base::SysCFStringRefToUTF8(bypass_item_ref));
      }
    }
  }
  
  // proxy bypass boolean
  
  config->proxy_bypass_local_names =
      GetBoolFromDictionary(config_dict.get(),
                            kSCPropNetProxiesExcludeSimpleHostnames,
                            false);
  
  return OK;
}

// Gets the proxy information for a query URL from a PAC. Implementation
// inspired by http://developer.apple.com/samplecode/CFProxySupportTool/
int ProxyResolverMac::GetProxyForURL(const GURL& query_url,
                                     const GURL& pac_url,
                                     ProxyInfo* results) {
  scoped_cftyperef<CFStringRef> query_ref(
      base::SysUTF8ToCFStringRef(query_url.spec()));
  scoped_cftyperef<CFStringRef> pac_ref(
      base::SysUTF8ToCFStringRef(pac_url.spec()));
  scoped_cftyperef<CFURLRef> query_url_ref(
      CFURLCreateWithString(kCFAllocatorDefault,
                            query_ref.get(),
                            NULL));
  scoped_cftyperef<CFURLRef> pac_url_ref(
      CFURLCreateWithString(kCFAllocatorDefault,
                            pac_ref.get(),
                            NULL));
  
  // Work around <rdar://problem/5530166>. This dummy call to
  // CFNetworkCopyProxiesForURL initializes some state within CFNetwork that is
  // required by CFNetworkExecuteProxyAutoConfigurationURL.
  
  CFArrayRef dummy_result = CFNetworkCopyProxiesForURL(query_url_ref.get(),
                                                       NULL);
  if (dummy_result)
    CFRelease(dummy_result);
  
  // We cheat here. We need to act as if we were synchronous, so we pump the
  // runloop ourselves. Our caller moved us to a new thread anyway, so this is
  // OK to do. (BTW, CFNetworkExecuteProxyAutoConfigurationURL returns a
  // runloop source we need to release despite its name.)
  
  CFTypeRef result = NULL;
  CFStreamClientContext context = { 0, &result, NULL, NULL, NULL };
  scoped_cftyperef<CFRunLoopSourceRef> runloop_source(
      CFNetworkExecuteProxyAutoConfigurationURL(pac_url_ref.get(),
                                                query_url_ref.get(),
                                                ResultCallback,
                                                &context));
  if (!runloop_source)
    return ERR_FAILED;
  
  const CFStringRef private_runloop_mode =
      CFSTR("org.chromium.ProxyResolverMac");
  
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runloop_source.get(),
                     private_runloop_mode);
  CFRunLoopRunInMode(private_runloop_mode, DBL_MAX, false);
  CFRunLoopRemoveSource(CFRunLoopGetCurrent(), runloop_source.get(),
                        private_runloop_mode);
  DCHECK(result != NULL);
  
  if (CFGetTypeID(result) == CFErrorGetTypeID()) {
    // TODO(avi): do something better than this
    CFRelease(result);
    return ERR_FAILED;
  }
  DCHECK(CFGetTypeID(result) == CFArrayGetTypeID());
  scoped_cftyperef<CFArrayRef> proxy_array_ref((CFArrayRef)result);
  
  // Proxy information. If we're allowed to contact the site directly, we set
  // allow_direct to true. If we've found proxies to use, apart from
  // accumulating them in proxy_list, we set found_proxy to true. These
  // bool results are orthogonal.
  bool allow_direct = false;
  std::string proxy_list;
  bool found_proxy = false;
  
  CFIndex proxy_array_count = CFArrayGetCount(proxy_array_ref.get());
  for (CFIndex i = 0; i < proxy_array_count; ++i) {
    CFDictionaryRef proxy_dictionary =
        (CFDictionaryRef)CFArrayGetValueAtIndex(proxy_array_ref.get(), i);
    DCHECK(CFGetTypeID(proxy_dictionary) == CFDictionaryGetTypeID());
    
    // The dictionary may have the following keys:
    // - kCFProxyTypeKey : The type of the proxy. We're assuming that it's of
    //                     the type we asked for (e.g. kCFProxyTypeHTTP for
    //                     http://... ) if it's not kCFProxyTypeNone.
    // - kCFProxyHostNameKey
    // - kCFProxyPortNumberKey : The meat we're after.
    // - kCFProxyUsernameKey
    // - kCFProxyPasswordKey : Despite the existence of these keys in the
    //                         documentation, they're never populated. Even if a
    //                         username/password were to be set in the network
    //                         proxy system preferences, we'd need to fetch it
    //                         from the Keychain ourselves. CFProxy is such a
    //                         tease.
    // - kCFProxyAutoConfigurationURLKey : If the PAC file specifies another
    //                                     PAC file, I'm going home.
    
    CFStringRef proxy_type =
        (CFStringRef)GetValueFromDictionary(proxy_dictionary,
                                            kCFProxyTypeKey,
                                            CFStringGetTypeID());
    if (CFEqual(proxy_type, kCFProxyTypeNone))
      allow_direct = true;
    if (CFEqual(proxy_type, kCFProxyTypeNone) ||
        // TODO(eroman): Include the SOCKS proxies in the result list.
        // While chromium does not yet support SOCKS, it is safe to
        // include it in the list.
        CFEqual(proxy_type, kCFProxyTypeSOCKS) ||
        CFEqual(proxy_type, kCFProxyTypeAutoConfigurationURL))
      continue;
    
    if (found_proxy)
      proxy_list += ";";
    else
      found_proxy = true;
    
    proxy_list += GetHostPortFromDictionary(proxy_dictionary,
                                            kCFProxyHostNameKey,
                                            kCFProxyPortNumberKey);
  }
  
  if (found_proxy)
    results->UseNamedProxy(proxy_list);
  if (allow_direct)
    results->UseDirect();
  
  return OK;
}

}  // namespace net
