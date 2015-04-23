// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keychain_mac.h"

OSStatus MacKeychain::ItemCopyAttributesAndData(
    SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info,
    SecItemClass *itemClass, SecKeychainAttributeList **attrList,
    UInt32 *length, void **outData) const {
  return SecKeychainItemCopyAttributesAndData(itemRef, info, itemClass,
                                              attrList, length, outData);
}

OSStatus MacKeychain::ItemModifyAttributesAndData(
    SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList,
    UInt32 length, const void *data) const {
  return SecKeychainItemModifyAttributesAndData(itemRef, attrList, length,
                                                data);
}

OSStatus MacKeychain::ItemFreeAttributesAndData(
    SecKeychainAttributeList *attrList, void *data) const {
  return SecKeychainItemFreeAttributesAndData(attrList, data);
}

OSStatus MacKeychain::SearchCreateFromAttributes(
    CFTypeRef keychainOrArray, SecItemClass itemClass,
    const SecKeychainAttributeList *attrList,
    SecKeychainSearchRef *searchRef) const {
  return SecKeychainSearchCreateFromAttributes(keychainOrArray, itemClass,
                                               attrList, searchRef);
}

OSStatus MacKeychain::SearchCopyNext(SecKeychainSearchRef searchRef,
                                     SecKeychainItemRef *itemRef) const {
  return SecKeychainSearchCopyNext(searchRef, itemRef);
}

OSStatus MacKeychain::AddInternetPassword(
    SecKeychainRef keychain,
    UInt32 serverNameLength, const char *serverName,
    UInt32 securityDomainLength, const char *securityDomain,
    UInt32 accountNameLength, const char *accountName,
    UInt32 pathLength, const char *path,
    UInt16 port, SecProtocolType protocol,
    SecAuthenticationType authenticationType,
    UInt32 passwordLength, const void *passwordData,
    SecKeychainItemRef *itemRef) const {
  return SecKeychainAddInternetPassword(keychain,
                                        serverNameLength, serverName,
                                        securityDomainLength, securityDomain,
                                        accountNameLength, accountName,
                                        pathLength, path,
                                        port, protocol, authenticationType,
                                        passwordLength, passwordData,
                                        itemRef);
}

void MacKeychain::Free(CFTypeRef ref) const {
  if (ref) {
    CFRelease(ref);
  }
}
