// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYCHAIN_MAC_H_
#define CHROME_BROWSER_KEYCHAIN_MAC_H_

#include <Security/Security.h>

#include "base/basictypes.h"

// Wraps the KeychainServices API in a very thin layer, to allow it to be
// mocked out for testing.

// See Keychain Services documentation for function documentation, as these call
// through directly to their Keychain Services equivalents (Foo ->
// SecKeychainFoo). The only exception is Free, which should be used for
// anything returned from this class that would normally be freed with
// CFRelease (to aid in testing).
class MacKeychain {
 public:
  MacKeychain() {}
  virtual ~MacKeychain() {}

  virtual OSStatus ItemCopyAttributesAndData(
      SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info,
      SecItemClass *itemClass, SecKeychainAttributeList **attrList,
      UInt32 *length, void **outData) const;

  virtual OSStatus ItemFreeAttributesAndData(SecKeychainAttributeList *attrList,
                                             void *data) const;

  virtual OSStatus SearchCreateFromAttributes(
      CFTypeRef keychainOrArray, SecItemClass itemClass,
      const SecKeychainAttributeList *attrList,
      SecKeychainSearchRef *searchRef) const;

  virtual OSStatus SearchCopyNext(SecKeychainSearchRef searchRef,
                                  SecKeychainItemRef *itemRef) const;

  // Calls CFRelease on the given ref, after checking that |ref| is non-NULL.
  virtual void Free(CFTypeRef ref) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MacKeychain);
};

#endif  // CHROME_BROWSER_KEYCHAIN_MAC_H_
