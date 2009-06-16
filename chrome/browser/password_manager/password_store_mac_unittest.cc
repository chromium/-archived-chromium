// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"

using webkit_glue::PasswordForm;

#pragma mark Mock Keychain

// TODO(stuartmorgan): Replace this with gMock. You know, once we have it.
// The basic idea of this mock is that it has a static array of data to use
// for ItemCopyAttributesAndData, and SecKeychainItemRef values are just indexes
// into that array (offset by 1 to prevent problems with client null-checking
// refs), cast to pointers.
class MockKeychain : public MacKeychain {
 public:
  MockKeychain();
  virtual ~MockKeychain();
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
  virtual void Free(CFTypeRef ref) const;

  // Causes a test failure unless everything returned from
  // ItemCopyAttributesAndData, SearchCreateFromAttributes, and SearchCopyNext
  // was correctly freed.
  void ExpectCreatesAndFreesBalanced();

 private:
  // Sets the data and length of |tag| in the item-th test item based on
  // |value|. The null-terminator will not be included; the Keychain Services
  // docs don't indicate whether it is or not, so clients should not assume
  // that it will be.
  void SetTestDataString(int item, UInt32 tag, const char* value);
  // Sets the data of the corresponding attribute of the item-th test item to
  // |value|. Assumes that the space has alread been allocated, and the length
  // set.
  void SetTestDataPort(int item, UInt32 value);
  void SetTestDataProtocol(int item, SecProtocolType value);
  void SetTestDataAuthType(int item, SecAuthenticationType value);
  void SetTestDataNegativeItem(int item, Boolean value);
  // Sets the password for the item-th test item. As with SetTestDataString,
  // the data will not be null-terminated.
  void SetTestDataPassword(int item, const char* value);

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
  unsigned int item_count_;

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

#pragma mark -

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
  item_count_ = 8;
  keychain_attr_list_ = static_cast<SecKeychainAttributeList*>(
      calloc(item_count_, sizeof(SecKeychainAttributeList)));
  keychain_data_ = static_cast<KeychainPasswordData*>(
      calloc(item_count_, sizeof(KeychainPasswordData)));
  for (unsigned int i = 0; i < item_count_; ++i) {
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

  // Basic HTML form.
  unsigned int item = 0;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecAccountItemAttr, "joe_user");
  SetTestDataString(item, kSecServerItemAttr, "some.domain.com");
  SetTestDataProtocol(item, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item, kSecCreationDateItemAttr, "20020601171500Z");
  SetTestDataPassword(item, "sekrit");

  // HTML form with path.
  ++item;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecAccountItemAttr, "joe_user");
  SetTestDataString(item, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item, kSecPathItemAttr, "/insecure.html");
  SetTestDataProtocol(item, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item, kSecCreationDateItemAttr, "19991231235959Z");
  SetTestDataPassword(item, "sekrit");

  // Secure HTML form with path.
  ++item;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecAccountItemAttr, "secure_user");
  SetTestDataString(item, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item, kSecPathItemAttr, "/secure.html");
  SetTestDataProtocol(item, kSecProtocolTypeHTTPS);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item, kSecCreationDateItemAttr, "20100908070605Z");
  SetTestDataPassword(item, "password");

  // True negative item.
  ++item;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecServerItemAttr, "dont.remember.com");
  SetTestDataProtocol(item, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item, kSecCreationDateItemAttr, "20000101000000Z");
  SetTestDataNegativeItem(item, true);

  // De-facto negative item, type one.
  ++item;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecAccountItemAttr, "Password Not Stored");
  SetTestDataString(item, kSecServerItemAttr, "dont.remember.com");
  SetTestDataProtocol(item, kSecProtocolTypeHTTP);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item, kSecCreationDateItemAttr, "20000101000000Z");
  SetTestDataPassword(item, "");

  // De-facto negative item, type two.
  ++item;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecServerItemAttr, "dont.remember.com");
  SetTestDataProtocol(item, kSecProtocolTypeHTTPS);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTMLForm);
  SetTestDataString(item, kSecCreationDateItemAttr, "20000101000000Z");
  SetTestDataPassword(item, " ");

  // HTTP auth basic, with port and path.
  ++item;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecAccountItemAttr, "basic_auth_user");
  SetTestDataString(item, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item, kSecSecurityDomainItemAttr, "low_security");
  SetTestDataString(item, kSecPathItemAttr, "/insecure.html");
  SetTestDataProtocol(item, kSecProtocolTypeHTTP);
  SetTestDataPort(item, 4567);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTTPBasic);
  SetTestDataString(item, kSecCreationDateItemAttr, "19980330100000Z");
  SetTestDataPassword(item, "basic");

  // HTTP auth digest, secure.
  ++item;
  CHECK(item < item_count_);
  SetTestDataString(item, kSecAccountItemAttr, "digest_auth_user");
  SetTestDataString(item, kSecServerItemAttr, "some.domain.com");
  SetTestDataString(item, kSecSecurityDomainItemAttr, "high_security");
  SetTestDataProtocol(item, kSecProtocolTypeHTTPS);
  SetTestDataAuthType(item, kSecAuthenticationTypeHTTPDigest);
  SetTestDataString(item, kSecCreationDateItemAttr, "19980330100000Z");
  SetTestDataPassword(item, "digest");
}

MockKeychain::~MockKeychain() {
  for (unsigned int i = 0; i < item_count_; ++i) {
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

void MockKeychain::ExpectCreatesAndFreesBalanced() {
  EXPECT_EQ(0, search_copy_count_);
  EXPECT_EQ(0, keychain_item_copy_count_);
  EXPECT_EQ(0, attribute_data_copy_count_);
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

void MockKeychain::SetTestDataString(int item, UInt32 tag, const char* value) {
  int attribute_index = IndexForTag(keychain_attr_list_[item], tag);
  size_t data_size = strlen(value);
  keychain_attr_list_[item].attr[attribute_index].length = data_size;
  if (data_size > 0) {
    keychain_attr_list_[item].attr[attribute_index].data = malloc(data_size);
    // Use memcpy rather than str*cpy because we are deliberately omitting the
    // null-terminator (see method declaration comment).
    CHECK(keychain_attr_list_[item].attr[attribute_index].data);
    memcpy(keychain_attr_list_[item].attr[attribute_index].data, value,
           data_size);
  } else {
    keychain_attr_list_[item].attr[attribute_index].data = NULL;
  }
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

void MockKeychain::SetTestDataPassword(int item, const char* value) {
  size_t data_size = strlen(value);
  keychain_data_[item].length = data_size;
  if (data_size > 0) {
    keychain_data_[item].data = malloc(data_size);
    // Use memcpy rather than str*cpy because we are deliberately omitting the
    // null-terminator (see method declaration comment).
    memcpy(keychain_data_[item].data, value, data_size);
  } else {
    keychain_data_[item].data = NULL;
  }
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

#pragma mark -
#pragma mark Unit Tests
////////////////////////////////////////////////////////////////////////////////

TEST(PasswordStoreMacTest, TestSignonRealmParsing) {
  typedef struct {
    const char* signon_realm;
    const bool expected_parsed;
    const char* expected_server;
    const bool expected_is_secure;
    const int expected_port;
    const char* expected_security_domain;
  } TestData;

  TestData test_data[] = {
    // HTML form signon realms.
    { "http://www.domain.com/",
        true, "www.domain.com", false, 0, "" },
    { "https://foo.org:9999/",
        true, "foo.org", true, 9999, "" },
    // HTTP auth signon realms.
    { "http://httpauth.com:8080/lowsecurity",
        true, "httpauth.com", false, 8080, "lowsecurity" },
    { "https://httpauth.com/highsecurity",
        true, "httpauth.com", true, 0 , "highsecurity" },
    // Bogus realms
    { "blahblahblah",
        false, false, "", 0, "" },
    { "foo/bar/baz",
        false, false, "", 0, "" },
  };

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    std::string server;
    std::string security_domain;
    bool is_secure = false;
    int port = -1;
    bool parsed = internal_keychain_helpers::ExtractSignonRealmComponents(
        std::string(test_data[i].signon_realm), &server, &port, &is_secure,
        &security_domain);
    EXPECT_EQ(test_data[i].expected_parsed, parsed) << "In iteration " << i;

    if (!parsed) {
      continue;  // If parse failed, out params are undefined.
    }
    EXPECT_EQ(std::string(test_data[i].expected_server), server)
        << "In iteration " << i;
    EXPECT_EQ(std::string(test_data[i].expected_security_domain),
              security_domain)
        << "In iteration " << i;
    EXPECT_EQ(test_data[i].expected_is_secure, is_secure)
        << "In iteration " << i;
    EXPECT_EQ(test_data[i].expected_port, port) << "In iteration " << i;
  }

  // NULLs are allowed for out params.
  bool parsed = internal_keychain_helpers::ExtractSignonRealmComponents(
      std::string("http://foo.bar.com:1234/baz"), NULL, NULL, NULL, NULL);
  EXPECT_TRUE(parsed);
}

TEST(PasswordStoreMacTest, TestURLConstruction) {
  std::string host("exampledomain.com");
  std::string path("/path/to/page.html");

  GURL full_url = internal_keychain_helpers::URLFromComponents(false, host,
                                                               1234, path);
  EXPECT_TRUE(full_url.is_valid());
  EXPECT_EQ(GURL("http://exampledomain.com:1234/path/to/page.html"), full_url);

  GURL simple_secure_url = internal_keychain_helpers::URLFromComponents(
      true, host, 0, std::string(""));
  EXPECT_TRUE(simple_secure_url.is_valid());
  EXPECT_EQ(GURL("https://exampledomain.com/"), simple_secure_url);
}

TEST(PasswordStoreMacTest, TestKeychainTime) {
  typedef struct {
    const char* time_string;
    const bool expected_parsed;
    const int expected_year;
    const int expected_month;
    const int expected_day;
    const int expected_hour;
    const int expected_minute;
    const int expected_second;
  } TestData;

  TestData test_data[] = {
    // HTML form signon realms.
    { "19980330100000Z", true,  1998,  3, 30, 10,  0,  0 },
    { "19991231235959Z", true,  1999, 12, 31, 23, 59, 59 },
    { "20000101000000Z", true,  2000,  1,  1,  0,  0,  0 },
    { "20011112012843Z", true,  2001, 11, 12,  1, 28, 43 },
    { "20020601171530Z", true,  2002,  6,  1, 17, 15, 30 },
    { "20100908070605Z", true,  2010,  9,  8,  7,  6,  5 },
    { "20010203040",     false, 0, 0, 0, 0, 0, 0 },
  };

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    base::Time time;
    bool parsed = internal_keychain_helpers::TimeFromKeychainTimeString(
        test_data[i].time_string, strlen(test_data[i].time_string), &time);
    EXPECT_EQ(test_data[i].expected_parsed, parsed) << "In iteration " << i;
    if (!parsed) {
      continue;
    }

    base::Time::Exploded exploded_time;
    time.UTCExplode(&exploded_time);
    EXPECT_EQ(test_data[i].expected_year, exploded_time.year)
        << "In iteration " << i;
    EXPECT_EQ(test_data[i].expected_month, exploded_time.month)
        << "In iteration " << i;
    EXPECT_EQ(test_data[i].expected_day, exploded_time.day_of_month)
        << "In iteration " << i;
    EXPECT_EQ(test_data[i].expected_hour, exploded_time.hour)
        << "In iteration " << i;
    EXPECT_EQ(test_data[i].expected_minute, exploded_time.minute)
        << "In iteration " << i;
    EXPECT_EQ(test_data[i].expected_second, exploded_time.second)
        << "In iteration " << i;
  }
}

TEST(PasswordStoreMacTest, TestAuthTypeSchemeTranslation) {
  // Our defined types should round-trip correctly.
  SecAuthenticationType auth_types[] = { kSecAuthenticationTypeHTMLForm,
                                         kSecAuthenticationTypeHTTPBasic,
                                         kSecAuthenticationTypeHTTPDigest };
  const int auth_count = sizeof(auth_types) / sizeof(SecAuthenticationType);
  for (int i = 0; i < auth_count; ++i) {
    SecAuthenticationType round_tripped_auth_type =
        internal_keychain_helpers::AuthTypeForScheme(
            internal_keychain_helpers::SchemeForAuthType(auth_types[i]));
    EXPECT_EQ(auth_types[i], round_tripped_auth_type);
  }
  // Anything else should become SCHEME_OTHER and come back as Default.
  PasswordForm::Scheme scheme_for_other_auth_type =
      internal_keychain_helpers::SchemeForAuthType(kSecAuthenticationTypeNTLM);
  SecAuthenticationType round_tripped_other_auth_type =
      internal_keychain_helpers::AuthTypeForScheme(scheme_for_other_auth_type);
  EXPECT_EQ(PasswordForm::SCHEME_OTHER, scheme_for_other_auth_type);
  EXPECT_EQ(kSecAuthenticationTypeDefault, round_tripped_other_auth_type);
}

TEST(PasswordStoreMacTest, TestKeychainToFormTranslation) {
  typedef struct {
    const PasswordForm::Scheme scheme;
    const char* signon_realm;
    const char* origin;
    const wchar_t* username;  // Set to NULL to check for a blacklist entry.
    const wchar_t* password;
    const bool ssl_valid;
    const int creation_year;
    const int creation_month;
    const int creation_day;
    const int creation_hour;
    const int creation_minute;
    const int creation_second;
  } TestExpectations;

  TestExpectations expected[] = {
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/", L"joe_user", L"sekrit", false,
      2002,  6,  1, 17, 15,  0 },
    { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
      "http://some.domain.com/insecure.html", L"joe_user", L"sekrit", false,
      1999, 12, 31, 23, 59, 59 },
    { PasswordForm::SCHEME_HTML, "https://some.domain.com/",
      "https://some.domain.com/secure.html", L"secure_user", L"password", true,
      2010,  9,  8,  7,  6,  5 },
    { PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
      "http://dont.remember.com/", NULL, NULL, false,
      2000,  1,  1,  0,  0,  0 },
    { PasswordForm::SCHEME_HTML, "http://dont.remember.com/",
      "http://dont.remember.com/", NULL, NULL, false,
      2000,  1,  1,  0,  0,  0 },
    { PasswordForm::SCHEME_HTML, "https://dont.remember.com/",
      "https://dont.remember.com/", NULL, NULL, true,
      2000,  1,  1,  0,  0,  0 },
    { PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
      "http://some.domain.com:4567/insecure.html", L"basic_auth_user", L"basic",
      false, 1998, 03, 30, 10, 00, 00 },
    { PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
      "https://some.domain.com/", L"digest_auth_user", L"digest", true,
      1998,  3, 30, 10,  0,  0 },
  };

  MockKeychain mock_keychain;

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(expected); ++i) {
    // Create our fake KeychainItemRef; see MockKeychain docs.
    SecKeychainItemRef keychain_item =
        reinterpret_cast<SecKeychainItemRef>(i + 1);
    PasswordForm form;
    bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        mock_keychain, keychain_item, &form);

    EXPECT_TRUE(parsed) << "In iteration " << i;
    mock_keychain.ExpectCreatesAndFreesBalanced();

    EXPECT_EQ(expected[i].scheme, form.scheme) << "In iteration " << i;
    EXPECT_EQ(GURL(expected[i].origin), form.origin) << "In iteration " << i;
    EXPECT_EQ(expected[i].ssl_valid, form.ssl_valid) << "In iteration " << i;
    EXPECT_EQ(std::string(expected[i].signon_realm), form.signon_realm)
        << "In iteration " << i;
    if (expected[i].username) {
      EXPECT_EQ(std::wstring(expected[i].username), form.username_value)
          << "In iteration " << i;
      EXPECT_EQ(std::wstring(expected[i].password), form.password_value)
          << "In iteration " << i;
      EXPECT_FALSE(form.blacklisted_by_user) << "In iteration " << i;
    } else {
      EXPECT_TRUE(form.blacklisted_by_user) << "In iteration " << i;
    }
    base::Time::Exploded exploded_time;
    form.date_created.UTCExplode(&exploded_time);
    EXPECT_EQ(expected[i].creation_year, exploded_time.year)
         << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_month, exploded_time.month)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_day, exploded_time.day_of_month)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_hour, exploded_time.hour)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_minute, exploded_time.minute)
        << "In iteration " << i;
    EXPECT_EQ(expected[i].creation_second, exploded_time.second)
        << "In iteration " << i;
  }

  {
    // Use an invalid ref, to make sure errors are reported.
    SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(99);
    PasswordForm form;
    bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        mock_keychain, keychain_item, &form);
    mock_keychain.ExpectCreatesAndFreesBalanced();
    EXPECT_FALSE(parsed);
  }
}

static void FreeKeychainItems(const MacKeychain& keychain,
                              std::vector<SecKeychainItemRef>* items) {
  for (std::vector<SecKeychainItemRef>::iterator i = items->begin();
       i != items->end(); ++i) {
    keychain.Free(*i);
  }
  items->clear();
}

TEST(PasswordStoreMacTest, TestKeychainSearch) {
  MockKeychain mock_keychain;

  {  // An HTML form we've seen.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://some.domain.com/"),
        PasswordForm::SCHEME_HTML, &matching_items);
    EXPECT_EQ(static_cast<size_t>(2), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    mock_keychain.ExpectCreatesAndFreesBalanced();
  }

  {  // An HTML form we haven't seen
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://www.unseendomain.com/"),
        PasswordForm::SCHEME_HTML, &matching_items);
    EXPECT_EQ(static_cast<size_t>(0), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    mock_keychain.ExpectCreatesAndFreesBalanced();
  }

  {  // Basic auth that should match.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://some.domain.com:4567/low_security"),
        PasswordForm::SCHEME_BASIC, &matching_items);
    EXPECT_EQ(static_cast<size_t>(1), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    mock_keychain.ExpectCreatesAndFreesBalanced();
  }

  {  // Basic auth with the wrong port.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://some.domain.com:1111/low_security"),
        PasswordForm::SCHEME_BASIC, &matching_items);
    EXPECT_EQ(static_cast<size_t>(0), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    mock_keychain.ExpectCreatesAndFreesBalanced();
  }

  {  // Digest auth we've saved under https, visited with http.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://some.domain.com/high_security"),
        PasswordForm::SCHEME_DIGEST, &matching_items);
    EXPECT_EQ(static_cast<size_t>(0), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    mock_keychain.ExpectCreatesAndFreesBalanced();
  }

  {  // Digest auth that should match.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("https://some.domain.com/high_security"),
        PasswordForm::SCHEME_DIGEST, &matching_items);
    EXPECT_EQ(static_cast<size_t>(1), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    mock_keychain.ExpectCreatesAndFreesBalanced();
  }
}

TEST(PasswordStoreMacTest, TestKeychainExactSearch) {
  MockKeychain mock_keychain;

  // Test a web form entry (SCHEME_HTML).
  {
    PasswordForm search_form;
    search_form.signon_realm = std::string("http://some.domain.com/");
    search_form.origin = GURL("http://some.domain.com/insecure.html");
    search_form.action = GURL("http://some.domain.com/submit.cgi");
    search_form.username_element = std::wstring(L"username");
    search_form.username_value = std::wstring(L"joe_user");
    search_form.password_element = std::wstring(L"password");
    search_form.preferred = true;
    SecKeychainItemRef match;
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        search_form, &match);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(2), match);
    mock_keychain.Free(match);

    // Make sure that the matching isn't looser than it should be.
    PasswordForm wrong_username(search_form);
    wrong_username.username_value = std::wstring(L"wrong_user");
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_username, &match);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_path(search_form);
    wrong_path.origin = GURL("http://some.domain.com/elsewhere.html");
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_path, &match);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_scheme(search_form);
    wrong_scheme.scheme = PasswordForm::SCHEME_BASIC;
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_scheme, &match);
    EXPECT_EQ(NULL, match);

    // With no path, we should match the pathless Keychain entry.
    PasswordForm no_path(search_form);
    no_path.origin = GURL("http://some.domain.com/");
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        no_path, &match);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(1), match);
    mock_keychain.Free(match);

    // We don't store blacklist entries in the keychain, and we want to ignore
    // those stored by other browsers.
    PasswordForm blacklist(search_form);
    blacklist.blacklisted_by_user = true;
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        blacklist, &match);
    EXPECT_EQ(NULL, match);

    mock_keychain.ExpectCreatesAndFreesBalanced();
  }

  // Test an http auth entry (SCHEME_BASIC, but SCHEME_DIGEST works is searched
  // the same way, so this gives sufficient coverage of both).
  {
    PasswordForm search_form;
    search_form.signon_realm =
        std::string("http://some.domain.com:4567/low_security");
    search_form.origin = GURL("http://some.domain.com:4567/insecure.html");
    search_form.username_value = std::wstring(L"basic_auth_user");
    search_form.scheme = PasswordForm::SCHEME_BASIC;
    SecKeychainItemRef match;
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        search_form, &match);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(7), match);
    mock_keychain.Free(match);

    // Make sure that the matching isn't looser than it should be.
    PasswordForm wrong_username(search_form);
    wrong_username.username_value = std::wstring(L"wrong_user");
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_username, &match);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_path(search_form);
    wrong_path.origin = GURL("http://some.domain.com:4567/elsewhere.html");
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_path, &match);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_scheme(search_form);
    wrong_scheme.scheme = PasswordForm::SCHEME_DIGEST;
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_scheme, &match);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_port(search_form);
    wrong_port.signon_realm =
        std::string("http://some.domain.com:1234/low_security");
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_port, &match);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_realm(search_form);
    wrong_realm.signon_realm =
        std::string("http://some.domain.com:4567/incorrect");
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        wrong_realm, &match);
    EXPECT_EQ(NULL, match);

    // We don't store blacklist entries in the keychain, and we want to ignore
    // those stored by other browsers.
    PasswordForm blacklist(search_form);
    blacklist.blacklisted_by_user = true;
    internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                        blacklist, &match);
    EXPECT_EQ(NULL, match);

    mock_keychain.ExpectCreatesAndFreesBalanced();
  }
}
