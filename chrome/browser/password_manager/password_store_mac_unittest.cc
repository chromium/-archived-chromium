// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "chrome/browser/keychain_mock_mac.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/password_store_mac_internal.h"

using webkit_glue::PasswordForm;

// Causes a test failure unless everything returned from
// ItemCopyAttributesAndData, SearchCreateFromAttributes, and SearchCopyNext
// was correctly freed.
static void ExpectCreatesAndFreesBalanced(const MockKeychain& keychain) {
  EXPECT_EQ(0, keychain.UnfreedSearchCount());
  EXPECT_EQ(0, keychain.UnfreedKeychainItemCount());
  EXPECT_EQ(0, keychain.UnfreedAttributeDataCount());
}

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

// Frees all the Keychain items in |items|, and clears the vector.
static void FreeKeychainItems(const MacKeychain& keychain,
                              std::vector<SecKeychainItemRef>* items) {
  CHECK(items);
  for (std::vector<SecKeychainItemRef>::iterator i = items->begin();
       i != items->end(); ++i) {
    keychain.Free(*i);
  }
  items->clear();
}

// Deletes all the PasswordForms in |forms|, and clears the vector.
static void DeletePasswordForms(std::vector<PasswordForm*>* forms) {
  CHECK(forms);
  for (std::vector<PasswordForm*>::iterator i = forms->begin();
       i != forms->end(); ++i) {
    delete *i;
  }
  forms->clear();
}

#pragma mark -

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
    ExpectCreatesAndFreesBalanced(mock_keychain);

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
    ExpectCreatesAndFreesBalanced(mock_keychain);
    EXPECT_FALSE(parsed);
  }
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
    ExpectCreatesAndFreesBalanced(mock_keychain);
  }

  {  // An HTML form we haven't seen
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://www.unseendomain.com/"),
        PasswordForm::SCHEME_HTML, &matching_items);
    EXPECT_EQ(static_cast<size_t>(0), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    ExpectCreatesAndFreesBalanced(mock_keychain);
  }

  {  // Basic auth that should match.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://some.domain.com:4567/low_security"),
        PasswordForm::SCHEME_BASIC, &matching_items);
    EXPECT_EQ(static_cast<size_t>(1), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    ExpectCreatesAndFreesBalanced(mock_keychain);
  }

  {  // Basic auth with the wrong port.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://some.domain.com:1111/low_security"),
        PasswordForm::SCHEME_BASIC, &matching_items);
    EXPECT_EQ(static_cast<size_t>(0), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    ExpectCreatesAndFreesBalanced(mock_keychain);
  }

  {  // Digest auth we've saved under https, visited with http.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("http://some.domain.com/high_security"),
        PasswordForm::SCHEME_DIGEST, &matching_items);
    EXPECT_EQ(static_cast<size_t>(0), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    ExpectCreatesAndFreesBalanced(mock_keychain);
  }

  {  // Digest auth that should match.
    std::vector<SecKeychainItemRef> matching_items;
    internal_keychain_helpers::FindMatchingKeychainItems(
        mock_keychain, std::string("https://some.domain.com/high_security"),
        PasswordForm::SCHEME_DIGEST, &matching_items);
    EXPECT_EQ(static_cast<size_t>(1), matching_items.size());
    FreeKeychainItems(mock_keychain, &matching_items);
    ExpectCreatesAndFreesBalanced(mock_keychain);
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
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                search_form);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(2), match);
    mock_keychain.Free(match);

    // Make sure that the matching isn't looser than it should be.
    PasswordForm wrong_username(search_form);
    wrong_username.username_value = std::wstring(L"wrong_user");
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_username);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_path(search_form);
    wrong_path.origin = GURL("http://some.domain.com/elsewhere.html");
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_path);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_scheme(search_form);
    wrong_scheme.scheme = PasswordForm::SCHEME_BASIC;
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_scheme);
    EXPECT_EQ(NULL, match);

    // With no path, we should match the pathless Keychain entry.
    PasswordForm no_path(search_form);
    no_path.origin = GURL("http://some.domain.com/");
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                no_path);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(1), match);
    mock_keychain.Free(match);

    // We don't store blacklist entries in the keychain, and we want to ignore
    // those stored by other browsers.
    PasswordForm blacklist(search_form);
    blacklist.blacklisted_by_user = true;
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                blacklist);
    EXPECT_EQ(NULL, match);

    ExpectCreatesAndFreesBalanced(mock_keychain);
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
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                search_form);
    EXPECT_EQ(reinterpret_cast<SecKeychainItemRef>(7), match);
    mock_keychain.Free(match);

    // Make sure that the matching isn't looser than it should be.
    PasswordForm wrong_username(search_form);
    wrong_username.username_value = std::wstring(L"wrong_user");
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_username);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_path(search_form);
    wrong_path.origin = GURL("http://some.domain.com:4567/elsewhere.html");
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_path);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_scheme(search_form);
    wrong_scheme.scheme = PasswordForm::SCHEME_DIGEST;
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_scheme);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_port(search_form);
    wrong_port.signon_realm =
        std::string("http://some.domain.com:1234/low_security");
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_port);
    EXPECT_EQ(NULL, match);

    PasswordForm wrong_realm(search_form);
    wrong_realm.signon_realm =
        std::string("http://some.domain.com:4567/incorrect");
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                wrong_realm);
    EXPECT_EQ(NULL, match);

    // We don't store blacklist entries in the keychain, and we want to ignore
    // those stored by other browsers.
    PasswordForm blacklist(search_form);
    blacklist.blacklisted_by_user = true;
    match = internal_keychain_helpers::FindMatchingKeychainItem(mock_keychain,
                                                                blacklist);
    EXPECT_EQ(NULL, match);

    ExpectCreatesAndFreesBalanced(mock_keychain);
  }
}

TEST(PasswordStoreMacTest, TestKeychainModify) {
  MockKeychain mock_keychain;
  SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(1);
  EXPECT_TRUE(internal_keychain_helpers::SetKeychainItemPassword(
      mock_keychain, keychain_item, std::string("allnewpassword")));
  PasswordForm form;
  internal_keychain_helpers::FillPasswordFormFromKeychainItem(mock_keychain,
                                                              keychain_item,
                                                              &form);
  EXPECT_EQ(L"allnewpassword", form.password_value);

  // Check that invalid items fail to update
  SecKeychainItemRef invalid_item = reinterpret_cast<SecKeychainItemRef>(1000);
  EXPECT_FALSE(internal_keychain_helpers::SetKeychainItemPassword(
      mock_keychain, invalid_item, std::string("allnewpassword")));

  // Check that other errors are reported (using the magic failure value).
  EXPECT_FALSE(internal_keychain_helpers::SetKeychainItemPassword(
      mock_keychain, keychain_item, std::string("fail_me")));

  ExpectCreatesAndFreesBalanced(mock_keychain);
}

TEST(PasswordStoreMacTest, TestKeychainAdd) {
  MockKeychain mock_keychain;
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

  for (unsigned int i = 0; i < ARRAYSIZE_UNSAFE(test_data); ++i) {
    PasswordForm* in_form = CreatePasswordFormFromData(test_data[i].data);
    bool add_succeeded =
        internal_keychain_helpers::AddKeychainEntryForForm(mock_keychain,
                                                           *in_form);
    EXPECT_EQ(test_data[i].should_succeed, add_succeeded);
    if (add_succeeded) {
      SecKeychainItemRef matching_item;
      matching_item = internal_keychain_helpers::FindMatchingKeychainItem(
          mock_keychain, *in_form);
      EXPECT_TRUE(matching_item != NULL);
      PasswordForm out_form;
      internal_keychain_helpers::FillPasswordFormFromKeychainItem(
          mock_keychain, matching_item, &out_form);
      EXPECT_EQ(out_form.scheme, in_form->scheme);
      EXPECT_EQ(out_form.signon_realm, in_form->signon_realm);
      EXPECT_EQ(out_form.origin, in_form->origin);
      EXPECT_EQ(out_form.username_value, in_form->username_value);
      EXPECT_EQ(out_form.password_value, in_form->password_value);
      mock_keychain.Free(matching_item);
    }
    ExpectCreatesAndFreesBalanced(mock_keychain);
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
    EXPECT_TRUE(internal_keychain_helpers::AddKeychainEntryForForm(
        mock_keychain, *update_form));
    SecKeychainItemRef keychain_item = reinterpret_cast<SecKeychainItemRef>(2);
    PasswordForm stored_form;
    internal_keychain_helpers::FillPasswordFormFromKeychainItem(mock_keychain,
                                                                keychain_item,
                                                                &stored_form);
    EXPECT_EQ(update_form->password_value, stored_form.password_value);
    delete update_form;
  }

  ExpectCreatesAndFreesBalanced(mock_keychain);
}

TEST(PasswordStoreMacTest, TestFormMatch) {
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

TEST(PasswordStoreMacTest, TestFormMerge) {
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

    DeletePasswordForms(&keychain_forms);
    DeletePasswordForms(&database_forms);
    DeletePasswordForms(&merged_forms);
  }
}
