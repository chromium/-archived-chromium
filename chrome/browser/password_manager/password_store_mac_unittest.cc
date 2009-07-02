// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/keychain_mock_mac.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"

using webkit_glue::PasswordForm;

class PasswordStoreMacTest : public testing::Test {
 public:
  virtual void SetUp() {
    MockKeychain::KeychainTestData test_data[] = {
      // Basic HTML form.
      { kSecAuthenticationTypeHTMLForm, "some.domain.com",
        kSecProtocolTypeHTTP, NULL, 0, NULL, "20020601171500Z",
        "joe_user", "sekrit", false },
      // HTML form with path.
      { kSecAuthenticationTypeHTMLForm, "some.domain.com",
        kSecProtocolTypeHTTP, "/insecure.html", 0, NULL, "19991231235959Z",
        "joe_user", "sekrit", false },
      // Secure HTML form with path.
      { kSecAuthenticationTypeHTMLForm, "some.domain.com",
        kSecProtocolTypeHTTPS, "/secure.html", 0, NULL, "20100908070605Z",
        "secure_user", "password", false },
      // True negative item.
      { kSecAuthenticationTypeHTMLForm, "dont.remember.com",
        kSecProtocolTypeHTTP, NULL, 0, NULL, "20000101000000Z",
        "", "", true },
      // De-facto negative item, type one.
      {kSecAuthenticationTypeHTMLForm, "dont.remember.com",
        kSecProtocolTypeHTTP, NULL, 0, NULL, "20000101000000Z",
        "Password Not Stored", "", false },
      // De-facto negative item, type two.
      { kSecAuthenticationTypeHTMLForm, "dont.remember.com",
        kSecProtocolTypeHTTPS, NULL, 0, NULL, "20000101000000Z",
        "Password Not Stored", " ", false },
      // HTTP auth basic, with port and path.
      { kSecAuthenticationTypeHTTPBasic, "some.domain.com",
        kSecProtocolTypeHTTP, "/insecure.html", 4567, "low_security",
        "19980330100000Z",
        "basic_auth_user", "basic", false },
      // HTTP auth digest, secure.
      { kSecAuthenticationTypeHTTPDigest, "some.domain.com",
        kSecProtocolTypeHTTPS, NULL, 0, "high_security", "19980330100000Z",
        "digest_auth_user", "digest", false },
      // An FTP password with an invalid date, for edge-case testing.
      { kSecAuthenticationTypeDefault, "a.server.com",
        kSecProtocolTypeFTP, NULL, 0, NULL, "20010203040",
        "abc", "123", false },
    };

    // Save one slot for use by AddInternetPassword.
    unsigned int capacity = arraysize(test_data) + 1;
    keychain_ = new MockKeychain(capacity);

    for (unsigned int i = 0; i < arraysize(test_data); ++i) {
      keychain_->AddTestItem(test_data[i]);
    }
  }

  virtual void TearDown() {
    ExpectCreatesAndFreesBalanced();
    ExpectCreatorCodesSet();
    delete keychain_;
  }

 protected:
  // Causes a test failure unless everything returned from keychain_'s
  // ItemCopyAttributesAndData, SearchCreateFromAttributes, and SearchCopyNext
  // was correctly freed.
  void ExpectCreatesAndFreesBalanced() {
    EXPECT_EQ(0, keychain_->UnfreedSearchCount());
    EXPECT_EQ(0, keychain_->UnfreedKeychainItemCount());
    EXPECT_EQ(0, keychain_->UnfreedAttributeDataCount());
  }

  // Causes a test failure unless any Keychain items added during the test have
  // their creator code set.
  void ExpectCreatorCodesSet() {
    EXPECT_TRUE(keychain_->CreatorCodesSetForAddedItems());
  }

  MockKeychain* keychain_;
};

#pragma mark -

// Struct used for creation of PasswordForms from static arrays of data.
struct PasswordFormData {
  const PasswordForm::Scheme scheme;
  const char* signon_realm;
  const char* origin;
  const char* action;
  const wchar_t* submit_element;
  const wchar_t* username_element;
  const wchar_t* password_element;
  const wchar_t* username_value;  // Set to NULL for a blacklist entry.
  const wchar_t* password_value;
  const bool preferred;
  const bool ssl_valid;
  const double creation_time;
};

// Creates and returns a new PasswordForm built from form_data. Caller is
// responsible for deleting the object when finished with it.
static PasswordForm* CreatePasswordFormFromData(
    const PasswordFormData& form_data) {
  PasswordForm* form = new PasswordForm();
  form->scheme = form_data.scheme;
  form->preferred = form_data.preferred;
  form->ssl_valid = form_data.ssl_valid;
  form->date_created = base::Time::FromDoubleT(form_data.creation_time);
  if (form_data.signon_realm)
    form->signon_realm = std::string(form_data.signon_realm);
  if (form_data.origin)
    form->origin = GURL(form_data.origin);
  if (form_data.action)
    form->action = GURL(form_data.action);
  if (form_data.submit_element)
    form->submit_element = std::wstring(form_data.submit_element);
  if (form_data.username_element)
    form->username_element = std::wstring(form_data.username_element);
  if (form_data.password_element)
    form->password_element = std::wstring(form_data.password_element);
  if (form_data.username_value) {
    form->username_value = std::wstring(form_data.username_value);
    if (form_data.password_value)
      form->password_value = std::wstring(form_data.password_value);
  } else {
    form->blacklisted_by_user = true;
  }
  return form;
}

// Macro to simplify calling CheckFormsAgainstExpectations with a useful label.
#define CHECK_FORMS(forms, expectations, i) \
    CheckFormsAgainstExpectations(forms, expectations, #forms, i)

// Ensures that the data in |forms| match |expectations|, causing test failures
// for any discrepencies.
// TODO(stuartmorgan): This is current order-dependent; ideally it shouldn't
// matter if |forms| and |expectations| are scrambled.
static void CheckFormsAgainstExpectations(
    const std::vector<PasswordForm*>& forms,
    const std::vector<PasswordFormData*>& expectations,
    const char* forms_label, unsigned int test_number) {
  const unsigned int kBufferSize = 128;
  char test_label[kBufferSize];
  snprintf(test_label, kBufferSize, "%s in test %u", forms_label, test_number);

  EXPECT_EQ(expectations.size(), forms.size()) << test_label;
  if (expectations.size() != forms.size())
    return;

  for (unsigned int i = 0; i < expectations.size(); ++i) {
    snprintf(test_label, kBufferSize, "%s in test %u, item %u",
             forms_label, test_number, i);
    PasswordForm* form = forms[i];
    PasswordFormData* expectation = expectations[i];
    EXPECT_EQ(expectation->scheme, form->scheme) << test_label;
    EXPECT_EQ(std::string(expectation->signon_realm), form->signon_realm)
        << test_label;
    EXPECT_EQ(GURL(expectation->origin), form->origin) << test_label;
    EXPECT_EQ(GURL(expectation->action), form->action) << test_label;
    EXPECT_EQ(std::wstring(expectation->submit_element), form->submit_element)
        << test_label;
    EXPECT_EQ(std::wstring(expectation->username_element),
              form->username_element) << test_label;
    EXPECT_EQ(std::wstring(expectation->password_element),
              form->password_element) << test_label;
    if (expectation->username_value) {
      EXPECT_EQ(std::wstring(expectation->username_value),
                form->username_value) << test_label;
      EXPECT_EQ(std::wstring(expectation->password_value),
                form->password_value) << test_label;
    } else {
      EXPECT_TRUE(form->blacklisted_by_user) << test_label;
    }
    EXPECT_EQ(expectation->preferred, form->preferred)  << test_label;
    EXPECT_EQ(expectation->ssl_valid, form->ssl_valid) << test_label;
    EXPECT_DOUBLE_EQ(expectation->creation_time,
                     form->date_created.ToDoubleT()) << test_label;
  }
}

#pragma mark -

TEST_F(PasswordStoreMacTest, TestKeychainToFormTranslation) {
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
    { PasswordForm::SCHEME_OTHER, "http://a.server.com/",
      "http://a.server.com/", L"abc", L"123", false,
      1970,  1,  1,  0,  0,  0 },
  };

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(expected); ++i) {
    // Create our fake KeychainItemRef; see MockKeychain docs.
    SecKeychainItemRef keychain_item =
        reinterpret_cast<SecKeychainItemRef>(i + 1);
    PasswordForm form;
    bool parsed = internal_keychain_helpers::FillPasswordFormFromKeychainItem(
        *keychain_, keychain_item, &form);

    EXPECT_TRUE(parsed) << "In iteration " << i;

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
        *keychain_, keychain_item, &form);
    EXPECT_FALSE(parsed);
  }
}

TEST_F(PasswordStoreMacTest, TestKeychainSearch) {
  struct TestDataAndExpectation {
    const PasswordFormData data;
    const size_t expected_matches;
  };
  // Most fields are left blank because we don't care about them for searching.
  TestDataAndExpectation test_data[] = {
    // An HTML form we've seen.
    { { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, 0 }, 2 },
    // An HTML form we haven't seen
    { { PasswordForm::SCHEME_HTML, "http://www.unseendomain.com/",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, 0 }, 0 },
    // Basic auth that should match.
    { { PasswordForm::SCHEME_BASIC, "http://some.domain.com:4567/low_security",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, 0 }, 1 },
    // Basic auth with the wrong port.
    { { PasswordForm::SCHEME_BASIC, "http://some.domain.com:1111/low_security",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, 0 }, 0 },
    // Digest auth we've saved under https, visited with http.
    { { PasswordForm::SCHEME_DIGEST, "http://some.domain.com/high_security",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, 0 }, 0 },
    // Digest auth that should match.
    { { PasswordForm::SCHEME_DIGEST, "https://some.domain.com/high_security",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, true, 0 }, 1 },
    // Digest auth with the wrong domain.
    { { PasswordForm::SCHEME_DIGEST, "https://some.domain.com/other_domain",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, true, 0 }, 0 },
    // Garbage forms should have no matches.
    { { PasswordForm::SCHEME_HTML, "foo/bar/baz",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, 0 }, 0 },
  };

  MacKeychainPasswordFormAdapter keychainAdapter(keychain_);
  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    scoped_ptr<PasswordForm> query_form(
        CreatePasswordFormFromData(test_data[i].data));
    std::vector<PasswordForm*> matching_items =
        keychainAdapter.PasswordsMatchingForm(*query_form);
    EXPECT_EQ(test_data[i].expected_matches, matching_items.size());
    STLDeleteElements(&matching_items);
  }
}

TEST_F(PasswordStoreMacTest, TestKeychainExactSearch) {
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
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            search_form);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(2), match);
    keychain_->Free(match);

    // Make sure that the matching isn't looser than it should be.
    PasswordForm wrong_username(search_form);
    wrong_username.username_value = std::wstring(L"wrong_user");
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_username);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_path(search_form);
    wrong_path.origin = GURL("http://some.domain.com/elsewhere.html");
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_path);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_scheme(search_form);
    wrong_scheme.scheme = PasswordForm::SCHEME_BASIC;
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_scheme);
    EXPECT_EQ(NULL, match);

    // With no path, we should match the pathless Keychain entry.
    PasswordForm no_path(search_form);
    no_path.origin = GURL("http://some.domain.com/");
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            no_path);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(1), match);
    keychain_->Free(match);

    // We don't store blacklist entries in the keychain, and we want to ignore
    // those stored by other browsers.
    PasswordForm blacklist(search_form);
    blacklist.blacklisted_by_user = true;
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            blacklist);
    EXPECT_EQ(NULL, match);
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
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            search_form);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(7), match);
    keychain_->Free(match);

    // Make sure that the matching isn't looser than it should be.
    PasswordForm wrong_username(search_form);
    wrong_username.username_value = std::wstring(L"wrong_user");
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_username);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_path(search_form);
    wrong_path.origin = GURL("http://some.domain.com:4567/elsewhere.html");
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_path);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_scheme(search_form);
    wrong_scheme.scheme = PasswordForm::SCHEME_DIGEST;
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_scheme);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_port(search_form);
    wrong_port.signon_realm =
        std::string("http://some.domain.com:1234/low_security");
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_port);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_realm(search_form);
    wrong_realm.signon_realm =
        std::string("http://some.domain.com:4567/incorrect");
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            wrong_realm);
    EXPECT_EQ(NULL, match);

    // We don't store blacklist entries in the keychain, and we want to ignore
    // those stored by other browsers.
    PasswordForm blacklist(search_form);
    blacklist.blacklisted_by_user = true;
    match = internal_keychain_helpers::MatchingKeychainItem(*keychain_,
                                                            blacklist);
    EXPECT_EQ(NULL, match);
  }
}

TEST_F(PasswordStoreMacTest, TestKeychainAdd) {
  struct TestDataAndExpectation {
    PasswordFormData data;
    bool should_succeed;
  };
  TestDataAndExpectation test_data[] = {
    // Test a variety of scheme/port/protocol/path variations.
    { { PasswordForm::SCHEME_HTML, "http://web.site.com/",
        "http://web.site.com/path/to/page.html", NULL, NULL, NULL, NULL,
        L"anonymous", L"knock-knock", false, false, 0 }, true },
    { { PasswordForm::SCHEME_HTML, "https://web.site.com/",
        "https://web.site.com/", NULL, NULL, NULL, NULL,
        L"admin", L"p4ssw0rd", false, false, 0 }, true },
    { { PasswordForm::SCHEME_BASIC, "http://a.site.com:2222/therealm",
        "http://a.site.com:2222/", NULL, NULL, NULL, NULL,
        L"username", L"password", false, false, 0 }, true },
    { { PasswordForm::SCHEME_DIGEST, "https://digest.site.com/differentrealm",
        "https://digest.site.com/secure.html", NULL, NULL, NULL, NULL,
        L"testname", L"testpass", false, false, 0 }, true },
    // Make sure that garbage forms are rejected.
    { { PasswordForm::SCHEME_HTML, "gobbledygook",
        "gobbledygook", NULL, NULL, NULL, NULL,
        L"anonymous", L"knock-knock", false, false, 0 }, false },
    // Test that failing to update a duplicate (forced using the magic failure
    // password; see MockKeychain::ItemModifyAttributesAndData) is reported.
    { { PasswordForm::SCHEME_HTML, "http://some.domain.com",
        "http://some.domain.com/insecure.html", NULL, NULL, NULL, NULL,
        L"joe_user", L"fail_me", false, false, 0 }, false },
  };

  MacKeychainPasswordFormAdapter keychainAdapter(keychain_);

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    PasswordForm* in_form = CreatePasswordFormFromData(test_data[i].data);
    bool add_succeeded = keychainAdapter.AddLogin(*in_form);
    EXPECT_EQ(test_data[i].should_succeed, add_succeeded);
    if (add_succeeded) {
      SecKeychainItemRef matching_item;
      matching_item = internal_keychain_helpers::MatchingKeychainItem(
          *keychain_, *in_form);
      EXPECT_TRUE(matching_item != NULL);
      PasswordForm out_form;
      internal_keychain_helpers::FillPasswordFormFromKeychainItem(
          *keychain_, matching_item, &out_form);
      EXPECT_EQ(out_form.scheme, in_form->scheme);
      EXPECT_EQ(out_form.signon_realm, in_form->signon_realm);
      EXPECT_EQ(out_form.origin, in_form->origin);
      EXPECT_EQ(out_form.username_value, in_form->username_value);
      EXPECT_EQ(out_form.password_value, in_form->password_value);
      keychain_->Free(matching_item);
    }
    delete in_form;
  }

  // Test that adding duplicate item updates the existing item.
  {
    PasswordFormData data = {
      PasswordForm::SCHEME_HTML, "http://some.domain.com",
      "http://some.domain.com/insecure.html", NULL,
      NULL, NULL, NULL, L"joe_user", L"updated_password", false, false, 0
    };
    PasswordForm* update_form = CreatePasswordFormFromData(data);
    EXPECT_TRUE(keychainAdapter.AddLogin(*update_form));
    SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(2);
    PasswordForm stored_form;
    internal_keychain_helpers::FillPasswordFormFromKeychainItem(*keychain_,
                                                                keychain_item,
                                                                &stored_form);
    EXPECT_EQ(update_form->password_value, stored_form.password_value);
    delete update_form;
  }
}

TEST_F(PasswordStoreMacTest, TestFormMatch) {
  PasswordForm base_form;
  base_form.signon_realm = std::string("http://some.domain.com/");
  base_form.origin = GURL("http://some.domain.com/page.html");
  base_form.username_value = std::wstring(L"joe_user");

  {
    // Check that everything unimportant can be changed.
    PasswordForm different_form(base_form);
    different_form.username_element = std::wstring(L"username");
    different_form.submit_element = std::wstring(L"submit");
    different_form.username_element = std::wstring(L"password");
    different_form.password_value = std::wstring(L"sekrit");
    different_form.action = GURL("http://some.domain.com/action.cgi");
    different_form.ssl_valid = true;
    different_form.preferred = true;
    different_form.date_created = base::Time::Now();
    bool paths_match = false;
    EXPECT_TRUE(internal_keychain_helpers::FormsMatchForMerge(base_form,
                                                              different_form,
                                                              &paths_match));
    EXPECT_TRUE(paths_match);

    // Check that we detect path differences, but still match.
    base_form.origin = GURL("http://some.domain.com/other_page.html");
    EXPECT_TRUE(internal_keychain_helpers::FormsMatchForMerge(base_form,
                                                              different_form,
                                                              &paths_match));
    EXPECT_FALSE(paths_match);
  }

  // Check that any one primary key changing is enough to prevent matching.
  {
    PasswordForm different_form(base_form);
    different_form.scheme = PasswordForm::SCHEME_DIGEST;
    EXPECT_FALSE(internal_keychain_helpers::FormsMatchForMerge(base_form,
                                                               different_form,
                                                               NULL));
  }
  {
    PasswordForm different_form(base_form);
    different_form.signon_realm = std::string("http://some.domain.com:8080/");
    EXPECT_FALSE(internal_keychain_helpers::FormsMatchForMerge(base_form,
                                                               different_form,
                                                               NULL));
  }
  {
    PasswordForm different_form(base_form);
    different_form.username_value = std::wstring(L"john.doe");
    EXPECT_FALSE(internal_keychain_helpers::FormsMatchForMerge(base_form,
                                                               different_form,
                                                               NULL));
  }
  {
    PasswordForm different_form(base_form);
    different_form.blacklisted_by_user = true;
    EXPECT_FALSE(internal_keychain_helpers::FormsMatchForMerge(base_form,
                                                               different_form,
                                                               NULL));
  }

  // Blacklist forms should *never* match for merging, even when identical
  // (and certainly not when only one is a blacklist entry).
  {
    PasswordForm form_a(base_form);
    form_a.blacklisted_by_user = true;
    PasswordForm form_b(form_a);
    EXPECT_FALSE(internal_keychain_helpers::FormsMatchForMerge(form_a, form_b,
                                                               NULL));
  }
}

TEST_F(PasswordStoreMacTest, TestFormMerge) {
  // Set up a bunch of test data to use in varying combinations.
  PasswordFormData keychain_user_1 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "", L"", L"", L"", L"joe_user", L"sekrit",
        false, false, 1010101010 };
  PasswordFormData keychain_user_1_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "", L"", L"", L"", L"joe_user", L"otherpassword",
        false, false, 1010101010 };
  PasswordFormData keychain_user_2 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "", L"", L"", L"", L"john.doe", L"sesame",
        false, false, 958739876 };
  PasswordFormData keychain_blacklist =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "", L"", L"", L"", NULL, NULL,
        false, false, 1010101010 };

  PasswordFormData db_user_1 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "http://some.domain.com/action.cgi",
        L"submit", L"username", L"password", L"joe_user", L"",
        true, false, 1212121212 };
  PasswordFormData db_user_1_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"joe_user", L"",
        true, false, 1234567890 };
  PasswordFormData db_user_3_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"second-account", L"",
        true, false, 1240000000 };
  PasswordFormData database_blacklist_with_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/path.html", "http://some.domain.com/action.cgi",
        L"submit", L"username", L"password", NULL, NULL,
        true, false, 1212121212 };

  PasswordFormData merged_user_1 =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/", "http://some.domain.com/action.cgi",
        L"submit", L"username", L"password", L"joe_user", L"sekrit",
        true, false, 1212121212 };
  PasswordFormData merged_user_1_with_db_path =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"joe_user", L"sekrit",
        true, false, 1234567890 };
  PasswordFormData merged_user_1_with_both_paths =
      { PasswordForm::SCHEME_HTML, "http://some.domain.com/",
        "http://some.domain.com/page.html",
        "http://some.domain.com/handlepage.cgi",
        L"submit", L"username", L"password", L"joe_user", L"otherpassword",
        true, false, 1234567890 };

  // Build up the big multi-dimensional array of data sets that will actually
  // drive the test. Use vectors rather than arrays so that initialization is
  // simple.
  enum {
    KEYCHAIN_INPUT = 0,
    DATABASE_INPUT,
    MERGE_OUTPUT,
    KEYCHAIN_OUTPUT,
    DATABASE_OUTPUT,
    MERGE_IO_ARRAY_COUNT  // termination marker
  };
  const unsigned int kTestCount = 4;
  std::vector< std::vector< std::vector<PasswordFormData*> > > test_data(
      MERGE_IO_ARRAY_COUNT, std::vector< std::vector<PasswordFormData*> >(
          kTestCount, std::vector<PasswordFormData*>()));
  unsigned int current_test = 0;

  // Test a merge with a few accounts in both systems, with partial overlap.
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_2);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1_with_path);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_3_with_path);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1_with_db_path);
  test_data[MERGE_OUTPUT][current_test].push_back(&keychain_user_2);
  test_data[DATABASE_OUTPUT][current_test].push_back(&db_user_3_with_path);

  // Test a merge where Chrome has a blacklist entry, and the keychain has
  // a stored account.
  ++current_test;
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(
      &database_blacklist_with_path);
  // We expect both to be present because a blacklist could be specific to a
  // subpath, and we want access to the password on other paths.
  test_data[MERGE_OUTPUT][current_test].push_back(
      &database_blacklist_with_path);
  test_data[MERGE_OUTPUT][current_test].push_back(&keychain_user_1);

  // Test a merge where Chrome has an account, and Keychain has a blacklist
  // (from another browser) and the Chrome password data.
  ++current_test;
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_blacklist);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1);
  test_data[KEYCHAIN_OUTPUT][current_test].push_back(&keychain_blacklist);

  // Test that matches are done using exact path when possible.
  ++current_test;
  CHECK(current_test < kTestCount);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1);
  test_data[KEYCHAIN_INPUT][current_test].push_back(&keychain_user_1_with_path);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1);
  test_data[DATABASE_INPUT][current_test].push_back(&db_user_1_with_path);
  test_data[MERGE_OUTPUT][current_test].push_back(&merged_user_1);
  test_data[MERGE_OUTPUT][current_test].push_back(
      &merged_user_1_with_both_paths);

  for (unsigned int test_case = 0; test_case <= current_test; ++test_case) {
    std::vector<PasswordForm*> keychain_forms;
    for (std::vector<PasswordFormData*>::iterator i =
             test_data[KEYCHAIN_INPUT][test_case].begin();
         i != test_data[KEYCHAIN_INPUT][test_case].end(); ++i) {
      keychain_forms.push_back(CreatePasswordFormFromData(*(*i)));
    }
    std::vector<PasswordForm*> database_forms;
    for (std::vector<PasswordFormData*>::iterator i =
             test_data[DATABASE_INPUT][test_case].begin();
         i != test_data[DATABASE_INPUT][test_case].end(); ++i) {
      database_forms.push_back(CreatePasswordFormFromData(*(*i)));
    }

    std::vector<PasswordForm*> merged_forms;
    internal_keychain_helpers::MergePasswordForms(&keychain_forms,
                                                  &database_forms,
                                                  &merged_forms);

    CHECK_FORMS(keychain_forms, test_data[KEYCHAIN_OUTPUT][test_case],
                test_case);
    CHECK_FORMS(database_forms, test_data[DATABASE_OUTPUT][test_case],
                test_case);
    CHECK_FORMS(merged_forms, test_data[MERGE_OUTPUT][test_case], test_case);

    STLDeleteElements(&keychain_forms);
    STLDeleteElements(&database_forms);
    STLDeleteElements(&merged_forms);
  }
}
