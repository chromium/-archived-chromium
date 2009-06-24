// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/keychain_mock_mac.h"

MockKeychain::MockKeychain(unsigned int item_capacity)
    : item_capacity_(item_capacity), item_count_(0), search_copy_count_(0),
      keychain_item_copy_count_(0), attribute_data_copy_count_(0) {
  UInt32 tags[] = { kSecAccountItemAttr,
                    kSecServerItemAttr,
                    kSecPortItemAttr,
                    kSecPathItemAttr,
                    kSecProtocolItemAttr,
                    kSecAuthenticationTypeItemAttr,
                    kSecSecurityDomainItemAttr,
                    kSecCreationDateItemAttr,
                    kSecNegativeItemAttr };

  // Create the test keychain data storage.
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
                                    size_t length) {
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

void MockKeychain::SetTestDataString(int item, UInt32 tag, const char* value) {
  SetTestDataBytes(item, tag, value, value ? strlen(value) : 0);
}

void MockKeychain::SetTestDataPort(int item, UInt32 value) {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecPortItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<UInt32*>(data)) = value;
}

void MockKeychain::SetTestDataProtocol(int item, SecProtocolType value) {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecProtocolItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<SecProtocolType*>(data)) = value;
}

void MockKeychain::SetTestDataAuthType(int item, SecAuthenticationType value) {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecAuthenticationTypeItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<SecAuthenticationType*>(data)) = value;
}

void MockKeychain::SetTestDataNegativeItem(int item, Boolean value) {
  int attribute_index = IndexForTag(keychain_attr_list_[item],
                                    kSecNegativeItemAttr);
  void* data = keychain_attr_list_[item].attr[attribute_index].data;
  *(static_cast<Boolean*>(data)) = value;
}

void MockKeychain::SetTestDataPasswordBytes(int item, const void* data,
                                            size_t length) {
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

void MockKeychain::SetTestDataPasswordString(int item, const char* value) {
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
    MockKeychain* mutable_this = const_cast<MockKeychain*>(this);
    mutable_this->SetTestDataPasswordBytes(item_index, data, length);
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

  MockKeychain* mutable_this = const_cast<MockKeychain*>(this);
  mutable_this->SetTestDataBytes(target_item, kSecServerItemAttr, serverName,
                                 serverNameLength);
  mutable_this->SetTestDataBytes(target_item, kSecSecurityDomainItemAttr,
                                 securityDomain, securityDomainLength);
  mutable_this->SetTestDataBytes(target_item, kSecAccountItemAttr, accountName,
                                 accountNameLength);
  mutable_this->SetTestDataBytes(target_item, kSecPathItemAttr, path,
                                 pathLength);
  mutable_this->SetTestDataPort(target_item, port);
  mutable_this->SetTestDataProtocol(target_item, protocol);
  mutable_this->SetTestDataAuthType(target_item, authenticationType);
  mutable_this->SetTestDataPasswordBytes(target_item, passwordData,
                                         passwordLength);
  base::Time::Exploded exploded_time;
  base::Time::Now().UTCExplode(&exploded_time);
  char time_string[128];
  snprintf(time_string, sizeof(time_string), "%04d%02d%02d%02d%02d%02dZ",
           exploded_time.year, exploded_time.month, exploded_time.day_of_month,
           exploded_time.hour, exploded_time.minute, exploded_time.second);
  mutable_this->SetTestDataString(target_item, kSecCreationDateItemAttr,
                                  time_string);

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

void MockKeychain::AddTestItem(const KeychainTestData& item_data) {
  unsigned int index = item_count_++;
  CHECK(index < item_capacity_);

  SetTestDataAuthType(index, item_data.auth_type);
  SetTestDataString(index, kSecServerItemAttr, item_data.server);
  SetTestDataProtocol(index, item_data.protocol);
  SetTestDataString(index, kSecPathItemAttr, item_data.path);
  SetTestDataPort(index, item_data.port);
  SetTestDataString(index, kSecSecurityDomainItemAttr,
                    item_data.security_domain);
  SetTestDataString(index, kSecCreationDateItemAttr, item_data.creation_date);
  SetTestDataString(index, kSecAccountItemAttr, item_data.username);
  SetTestDataPasswordString(index, item_data.password);
  SetTestDataNegativeItem(index, item_data.negative_item);
}
