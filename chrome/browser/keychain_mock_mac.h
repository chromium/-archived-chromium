// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYCHAIN_MOCK_MAC_H_
#define CHROME_BROWSER_KEYCHAIN_MOCK_MAC_H_

#include <vector>

#include "chrome/browser/keychain_mac.h"

// Mock Keychain wrapper for testing code that interacts with the OS Keychain.
// The basic idea of this mock is that it has a static array of data, and
// SecKeychainItemRef values are just indexes into that array (offset by 1 to
// prevent problems with clients that null-check refs), cast to pointers.
//
// TODO(stuartmorgan): The test data is currently hard-coded; instead it should
// be settable by tests so that there isn't a strong coupling between the mock
// implementation and the tests.
class MockKeychain : public MacKeychain {
 public:
  MockKeychain();
  virtual ~MockKeychain();
  virtual OSStatus ItemCopyAttributesAndData(
      SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info,
      SecItemClass *itemClass, SecKeychainAttributeList **attrList,
      UInt32 *length, void **outData) const;
  // Pass "fail_me" as the data to get errSecAuthFailed.
  virtual OSStatus ItemModifyAttributesAndData(
      SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList,
      UInt32 length, const void *data) const;
  virtual OSStatus ItemFreeAttributesAndData(SecKeychainAttributeList *attrList,
                                             void *data) const;
  virtual OSStatus SearchCreateFromAttributes(
      CFTypeRef keychainOrArray, SecItemClass itemClass,
      const SecKeychainAttributeList *attrList,
      SecKeychainSearchRef *searchRef) const;
  virtual OSStatus SearchCopyNext(SecKeychainSearchRef searchRef,
                                  SecKeychainItemRef *itemRef) const;
  // Pass "some.domain.com" as the serverName to get errSecDuplicateItem.
  virtual OSStatus AddInternetPassword(SecKeychainRef keychain,
                                       UInt32 serverNameLength,
                                       const char *serverName,
                                       UInt32 securityDomainLength,
                                       const char *securityDomain,
                                       UInt32 accountNameLength,
                                       const char *accountName,
                                       UInt32 pathLength, const char *path,
                                       UInt16 port, SecProtocolType protocol,
                                       SecAuthenticationType authenticationType,
                                       UInt32 passwordLength,
                                       const void *passwordData,
                                       SecKeychainItemRef *itemRef) const;
  virtual void Free(CFTypeRef ref) const;

  // Return the counts of objects returned by Create/Copy functions but never
  // Free'd as they should have been.
  int UnfreedSearchCount() const;
  int UnfreedKeychainItemCount() const;
  int UnfreedAttributeDataCount() const;

 private:
  // Note that "const" is pretty much meaningless for this class; the const-ness
  // of MacKeychain doesn't apply to the actual keychain data, so all of our
  // data is mutable. These helpers are all const because they are needed by
  // some of the const API functions we are mocking.

  // Sets the data and length of |tag| in the item-th test item.
  void SetTestDataBytes(int item, UInt32 tag, const void* data,
                        size_t length) const;
  // Sets the data and length of |tag| in the item-th test item based on
  // |value|. The null-terminator will not be included; the Keychain Services
  // docs don't indicate whether it is or not, so clients should not assume
  // that it will be.
  void SetTestDataString(int item, UInt32 tag, const char* value) const;
  // Sets the data of the corresponding attribute of the item-th test item to
  // |value|. Assumes that the space has alread been allocated, and the length
  // set.
  void SetTestDataPort(int item, UInt32 value) const;
  void SetTestDataProtocol(int item, SecProtocolType value) const;
  void SetTestDataAuthType(int item, SecAuthenticationType value) const;
  void SetTestDataNegativeItem(int item, Boolean value) const;
  // Sets the password data and length for the item-th test item.
  void SetTestDataPasswordBytes(int item, const void* data,
                                size_t length) const;
  // Sets the password for the item-th test item. As with SetTestDataString,
  // the data will not be null-terminated.
  void SetTestDataPasswordString(int item, const char* value) const;

  // Returns the index of |tag| in |attribute_list|, or -1 if it's not found.
  static int IndexForTag(const SecKeychainAttributeList& attribute_list,
                         UInt32 tag);

  static const int kDummySearchRef = 1000;

  typedef struct  {
    void* data;
    UInt32 length;
  } KeychainPasswordData;

  SecKeychainAttributeList* keychain_attr_list_;
  KeychainPasswordData* keychain_data_;
  unsigned int item_capacity_;
  mutable unsigned int item_count_;

  // Tracks the items that should be returned in subsequent calls to
  // SearchCopyNext, based on the last call to SearchCreateFromAttributes.
  // We can't handle multiple active searches, since we don't track the search
  // ref we return, but we don't need to for our mocking.
  mutable std::vector<unsigned int> remaining_search_results_;

  // Track copies and releases to make sure they balance. Really these should
  // be maps to track per item, but this should be good enough to catch
  // real mistakes.
  mutable int search_copy_count_;
  mutable int keychain_item_copy_count_;
  mutable int attribute_data_copy_count_;
};

#endif  // CHROME_BROWSER_KEYCHAIN_MOCK_MAC_H_
