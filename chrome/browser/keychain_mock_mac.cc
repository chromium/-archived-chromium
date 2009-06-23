// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/keychain_mock_mac.h"

MockKeychain::MockKeychain()
    : search_copy_count_(0), keychain_item_copy_count_(0),
      attribute_data_copy_count_(0) {
  UInt32 tags[] = { kSecAccountItemAttr,
                    kSecServerItemAttr,
                    kSecPortItemAttr,
                    kSecPathItemAttr,
                    kSecProtocolItemAttr,
                    kSecAuthenticationTypeItemAttr,
                    kSecSecurityDomainItemAttr,
                    kSecCreationDateItemAttr,
                    kSecNegativeItemAttr };

  // Create the test keychain data to return from ItemCopyAttributesAndData,
  // and set up everything that's consistent across all the items.
  item_capacity_ = 9;
  keychain_attr_list_ = static_cast<SecKeychainAttributeList*>(
      calloc(item_capacity_, sizeof(SecKeychainAttributeList)));
  keychain_data_ = static_cast<KeychainPasswordData*>(
      calloc(item_capacity_, sizeof(KeychainPasswordData)));
  for (unsigned int i = 0; i < item_capacity_; ++i) {
    keychain_attr_list_[i].count = arraysize(tags);
    keychain_attr_list_[i].attr = static_cast<SecKeychainAttribute*>(
        calloc(keychain_attr_list_[i].count, sizeof(SecKeychainAttribute)));
    for (unsigned int j = 0; j < keychain_attr_list_[i].count; ++j) {
      keychain_attr_list_[i].attr[j].tag = tags[j];
      size_t data_size = 0;
      switch (tags[j]) {
        case kSecPortItemAttr:
          data_size = sizeof(UInt32);
          break;
        case kSecProtocolItemAttr:
          data_size = sizeof(SecProtocolType);
          break;
        case kSecAuthenticationTypeItemAttr:
          data_size = sizeof(SecAuthenticationType);
          break;
        case kSecNegativeItemAttr:
          data_size = sizeof(Boolean);
          break;
      }
      if (data_size > 0) {
        keychain_attr_list_[i].attr[j].length = data_size;
        keychain_attr_list_[i].attr[j].data = calloc(1, data_size);
      }
    }
  }

  // Save one slot for use by AddInternetPassword.
  unsigned int available_slots = item_capacity_ - 1;
  item_count_ = 0;

  // Basic HTML form.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecAccountItemAttr, "joe_user");
  SetTestDataString(item_count_, kSecServerItemAttr, "some.domain.com");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "20020601171500Z");
  SetTestDataPasswordString(item_count_, "sekrit");
  ++item_count_;

  // HTML form with path.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecAccountItemAttr, "joe_user");
  SetTestDataString(item_count_, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item_count_, kSecPathItemAttr, "/insecure.html");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "19991231235959Z");
  SetTestDataPasswordString(item_count_, "sekrit");
  ++item_count_;

  // Secure HTML form with path.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecAccountItemAttr, "secure_user");
  SetTestDataString(item_count_, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item_count_, kSecPathItemAttr, "/secure.html");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTPS);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "20100908070605Z");
  SetTestDataPasswordString(item_count_, "password");
  ++item_count_;

  // True negative item.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecServerItemAttr, "dont.remember.com");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "20000101000000Z");
  SetTestDataNegativeItem(item_count_, true);
  ++item_count_;

  // De-facto negative item, type one.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecAccountItemAttr, "Password Not Stored");
  SetTestDataString(item_count_, kSecServerItemAttr, "dont.remember.com");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "20000101000000Z");
  SetTestDataPasswordString(item_count_, "");
  ++item_count_;

  // De-facto negative item, type two.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecServerItemAttr, "dont.remember.com");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTPS);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "20000101000000Z");
  SetTestDataPasswordString(item_count_, " ");
  ++item_count_;

  // HTTP auth basic, with port and path.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecAccountItemAttr, "basic_auth_user");
  SetTestDataString(item_count_, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item_count_, kSecSecurityDomainItemAttr, "low_security");
  SetTestDataString(item_count_, kSecPathItemAttr, "/insecure.html");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTP);
  SetTestDataPort(item_count_, 4567);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTTPBasic);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "19980330100000Z");
  SetTestDataPasswordString(item_count_, "basic");
  ++item_count_;

  // HTTP auth digest, secure.
  CHECK(item_count_ < available_slots);
  SetTestDataString(item_count_, kSecAccountItemAttr, "digest_auth_user");
  SetTestDataString(item_count_, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item_count_, kSecSecurityDomainItemAttr, "high_security");
  SetTestDataProtocol(item_count_, kSecProtocolTypeHTTPS);
  SetTestDataAuthType(item_count_, kSecAuthenticationTypeHTTPDigest);
  SetTestDataString(item_count_, kSecCreationDateItemAttr, "19980330100000Z");
  SetTestDataPasswordString(item_count_, "digest");
  ++item_count_;
}

MockKeychain::~MockKeychain() {
  for (unsigned int i = 0; i < item_capacity_; ++i) {
    for (unsigned int j = 0; j < keychain_attr_list_[i].count; ++j) {
      if (keychain_attr_list_[i].attr[j].data) {
        free(keychain_attr_list_[i].attr[j].data);
      }
    }
    free(keychain_attr_list_[i].attr);
    if (keychain_data_[i].data) {
      free(keychain_data_[i].data);
    }
  }
  free(keychain_attr_list_);
  free(keychain_data_);
}

int MockKeychain::IndexForTag(const SecKeychainAttributeList& attribute_list,
                              UInt32 tag) {
  for (unsigned int i = 0; i < attribute_list.count; ++i) {
    if (attribute_list.attr[i].tag == tag) {
      return i;
    }
  }
  DCHECK(false);
  return -1;
}

void MockKeychain::SetTestDataBytes(int item, UInt32 tag, const void* data,
                                    size_t length) const {
  int attribute_index = IndexForTag(keychain_attr_list_[item], tag);
  keychain_attr_list_[item].attr[attribute_index].length = length;
  if (length > 0) {
    if (keychain_attr_list_[item].attr[attribute_index].data) {
      free(keychain_attr_list_[item].attr[attribute_index].data);
    }
    keychain_attr_list_[item].attr[attribute_index].data = malloc(length);
    CHECK(keychain_attr_list_[item].attr[attribute_index].data);
    memcpy(keychain_attr_list_[item].attr[attribute_index].data, data, length);
  } else {
    keychain_attr_list_[item].attr[attribute_index].data = NULL;
  }
}

void MockKeychain::SetTestDataString(int item, UInt32 tag,
                                     const char* value) const {
  SetTestDataBytes(item, tag, value, value ? strlen(value) : 0);
}

void MockKeychain::SetTestDataPort(int item, UInt32 value) const {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecPortItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<UInt32*>(data)) = value;
}

void MockKeychain::SetTestDataProtocol(int item, SecProtocolType value) const {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecProtocolItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<SecProtocolType*>(data)) = value;
}

void MockKeychain::SetTestDataAuthType(int item,
                                       SecAuthenticationType value) const {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecAuthenticationTypeItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<SecAuthenticationType*>(data)) = value;
}

void MockKeychain::SetTestDataNegativeItem(int item, Boolean value) const {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecNegativeItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<Boolean*>(data)) = value;
}

void MockKeychain::SetTestDataPasswordBytes(int item, const void* data,
                                            size_t length) const {
  keychain_data_[item].length = length;
  if (length > 0) {
    if (keychain_data_[item].data) {
      free(keychain_data_[item].data);
    }
    keychain_data_[item].data = malloc(length);
    memcpy(keychain_data_[item].data, data, length);
  } else {
    keychain_data_[item].data = NULL;
  }
}

void MockKeychain::SetTestDataPasswordString(int item,
                                             const char* value) const {
  SetTestDataPasswordBytes(item, value, value ? strlen(value) : 0);
}

OSStatus MockKeychain::ItemCopyAttributesAndData(
    SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info,
    SecItemClass *itemClass, SecKeychainAttributeList **attrList,
    UInt32 *length, void **outData) const {
  DCHECK(itemRef);
  unsigned int item_index = reinterpret_cast<unsigned int>(itemRef) - 1;
  if (item_index >= item_count_) {
    return errSecInvalidItemRef;
  }

  DCHECK(!itemClass);  // itemClass not implemented in the Mock.
  if (attrList) {
    *attrList  = &(keychain_attr_list_[item_index]);
  }
  if (outData) {
    *outData = keychain_data_[item_index].data;
    DCHECK(length);
    *length = keychain_data_[item_index].length;
  }

  ++attribute_data_copy_count_;
  return noErr;
}

OSStatus MockKeychain::ItemModifyAttributesAndData(
    SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList,
    UInt32 length, const void *data) const {
  DCHECK(itemRef);
  const char* fail_trigger = "fail_me";
  if (length == strlen(fail_trigger) &&
      memcmp(data, fail_trigger, length) == 0) {
    return errSecAuthFailed;
  }

  unsigned int item_index = reinterpret_cast<unsigned int>(itemRef) - 1;
  if (item_index >= item_count_) {
    return errSecInvalidItemRef;
  }

  if (attrList) {
    NOTIMPLEMENTED();
  }
  if (data) {
    SetTestDataPasswordBytes(item_index, data, length);
  }
  return noErr;
}

OSStatus MockKeychain::ItemFreeAttributesAndData(
    SecKeychainAttributeList *attrList,
    void *data) const {
  --attribute_data_copy_count_;
  return noErr;
}

OSStatus MockKeychain::SearchCreateFromAttributes(
    CFTypeRef keychainOrArray, SecItemClass itemClass,
    const SecKeychainAttributeList *attrList,
    SecKeychainSearchRef *searchRef) const {
  // Figure out which of our mock items matches, and set up the array we'll use
  // to generate results out of SearchCopyNext.
  remaining_search_results_.clear();
  for (unsigned int mock_item = 0; mock_item < item_count_; ++mock_item) {
    bool mock_item_matches = true;
    for (UInt32 search_attr = 0; search_attr < attrList->count; ++search_attr) {
      int mock_attr = IndexForTag(keychain_attr_list_[mock_item],
                                  attrList->attr[search_attr].tag);
      SecKeychainAttribute* mock_attribute =
          &(keychain_attr_list_[mock_item].attr[mock_attr]);
      if (mock_attribute->length != attrList->attr[search_attr].length ||
          memcmp(mock_attribute->data, attrList->attr[search_attr].data,
                 attrList->attr[search_attr].length) != 0) {
        mock_item_matches = false;
        break;
      }
    }
    if (mock_item_matches) {
      remaining_search_results_.push_back(mock_item);
    }
  }

  DCHECK(searchRef);
  *searchRef = reinterpret_cast<SecKeychainSearchRef>(kDummySearchRef);
  ++search_copy_count_;
  return noErr;
}

OSStatus MockKeychain::AddInternetPassword(
    SecKeychainRef keychain,
    UInt32 serverNameLength, const char *serverName,
    UInt32 securityDomainLength, const char *securityDomain,
    UInt32 accountNameLength, const char *accountName,
    UInt32 pathLength, const char *path,
    UInt16 port, SecProtocolType protocol,
    SecAuthenticationType authenticationType,
    UInt32 passwordLength, const void *passwordData,
    SecKeychainItemRef *itemRef) const {

  // Check for the magic duplicate item trigger.
  if (strcmp(serverName, "some.domain.com") == 0) {
    return errSecDuplicateItem;
  }

  // Use empty slots until they run out, then just keep replacing the last item.
  int target_item = (item_count_ == item_capacity_) ? item_capacity_ - 1
                                                    : item_count_++;

  SetTestDataBytes(target_item, kSecServerItemAttr, serverName,
                   serverNameLength);
  SetTestDataBytes(target_item, kSecSecurityDomainItemAttr, securityDomain,
                   securityDomainLength);
  SetTestDataBytes(target_item, kSecAccountItemAttr, accountName,
                   accountNameLength);
  SetTestDataBytes(target_item, kSecPathItemAttr, path, pathLength);
  SetTestDataPort(target_item, port);
  SetTestDataProtocol(target_item, protocol);
  SetTestDataAuthType(target_item, authenticationType);
  SetTestDataPasswordBytes(target_item, passwordData, passwordLength);
  base::Time::Exploded exploded_time;
  base::Time::Now().UTCExplode(&exploded_time);
  char time_string[128];
  snprintf(time_string, sizeof(time_string), "%04d%02d%02d%02d%02d%02dZ",
           exploded_time.year, exploded_time.month, exploded_time.day_of_month,
           exploded_time.hour, exploded_time.minute, exploded_time.second);
  SetTestDataString(target_item, kSecCreationDateItemAttr, time_string);

  if (itemRef) {
    *itemRef = reinterpret_cast<SecKeychainItemRef>(target_item + 1);
  }
  return noErr;
}

OSStatus MockKeychain::SearchCopyNext(SecKeychainSearchRef searchRef,
                                      SecKeychainItemRef *itemRef) const {
  if (remaining_search_results_.empty()) {
    return errSecItemNotFound;
  }
  unsigned int index = remaining_search_results_.front();
  remaining_search_results_.erase(remaining_search_results_.begin());
  *itemRef = reinterpret_cast<SecKeychainItemRef>(index + 1);
  ++keychain_item_copy_count_;
  return noErr;
}

void MockKeychain::Free(CFTypeRef ref) const {
  if (!ref) {
    return;
  }

  if (reinterpret_cast<int>(ref) == kDummySearchRef) {
    --search_copy_count_;
  } else {
    --keychain_item_copy_count_;
  }
}

int MockKeychain::UnfreedSearchCount() const {
  return search_copy_count_;
}

int MockKeychain::UnfreedKeychainItemCount() const {
  return keychain_item_copy_count_;
}

int MockKeychain::UnfreedAttributeDataCount() const {
  return attribute_data_copy_count_;
}
