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

void MacKeychain::Free(CFTypeRef ref) const {
  if (ref) {
    CFRelease(ref);
  }
}
