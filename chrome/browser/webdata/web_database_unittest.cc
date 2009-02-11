// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/stl_util-inl.h"
#include "skia/include/SkBitmap.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/autofill_form.h"
#include "webkit/glue/password_form.h"

using base::Time;
using base::TimeDelta;

class WebDatabaseTest : public testing::Test {
 protected:

  virtual void SetUp() {
    PathService::Get(chrome::DIR_TEST_DATA, &file_);
    file_ += FilePath::kSeparators[0];
    file_ += L"TestWebDatabase";
    file_ += Int64ToWString(base::Time::Now().ToInternalValue());
    file_ += L".db";
    file_util::Delete(file_, false);
  }

  virtual void TearDown() {
    file_util::Delete(file_, false);
  }

  static int GetKeyCount(const DictionaryValue& d) {
    DictionaryValue::key_iterator i(d.begin_keys());
    DictionaryValue::key_iterator e(d.end_keys());

    int r = 0;
    while (i != e) {
      ++i;
      ++r;
    }
    return r;
  }

  static bool StringDictionaryValueEquals(const DictionaryValue& a,
                                          const DictionaryValue& b) {
    int a_count = 0;
    int b_count = GetKeyCount(b);
    DictionaryValue::key_iterator i(a.begin_keys());
    DictionaryValue::key_iterator e(a.end_keys());
    std::wstring av, bv;
    while (i != e) {
      if (!(a.GetString(*i, &av)) ||
          !(b.GetString(*i, &bv)) ||
          av != bv)
        return false;

      a_count++;
      ++i;
    }

    return (a_count == b_count);
  }

  static int64 GetID(const TemplateURL* url) {
    return url->id();
  }

  static void SetID(int64 new_id, TemplateURL* url) {
    url->set_id(new_id);
  }

  static void set_prepopulate_id(TemplateURL* url, int id) {
    url->set_prepopulate_id(id);
  }

  std::wstring file_;
};

TEST_F(WebDatabaseTest, Keywords) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(L"short_name");
  template_url.set_keyword(L"keyword");
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://google.com/");
  template_url.SetFavIconURL(favicon_url);
  template_url.SetURL(L"http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  template_url.set_show_in_default_list(true);
  template_url.set_originating_url(originating_url);
  template_url.set_usage_count(32);
  template_url.add_input_encoding("UTF-8");
  set_prepopulate_id(&template_url, 10);
  SetID(1, &template_url);

  EXPECT_TRUE(db.AddKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));

  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());

  EXPECT_EQ(template_url.keyword(), restored_url->keyword());

  EXPECT_FALSE(restored_url->autogenerate_keyword());

  EXPECT_TRUE(favicon_url == restored_url->GetFavIconURL());

  EXPECT_TRUE(restored_url->safe_for_autoreplace());

  EXPECT_TRUE(restored_url->show_in_default_list());

  EXPECT_EQ(GetID(&template_url), GetID(restored_url));

  EXPECT_TRUE(originating_url == restored_url->originating_url());

  EXPECT_EQ(32, restored_url->usage_count());

  ASSERT_EQ(1U, restored_url->input_encodings().size());
  EXPECT_EQ("UTF-8", restored_url->input_encodings()[0]);

  EXPECT_EQ(10, restored_url->prepopulate_id());

  EXPECT_TRUE(db.RemoveKeyword(restored_url->id()));

  template_urls.clear();
  EXPECT_TRUE(db.GetKeywords(&template_urls));

  EXPECT_EQ(0U, template_urls.size());

  delete restored_url;
}

TEST_F(WebDatabaseTest, KeywordMisc) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));

  ASSERT_EQ(0, db.GetDefaulSearchProviderID());
  ASSERT_EQ(0, db.GetBuitinKeywordVersion());

  db.SetDefaultSearchProviderID(10);
  db.SetBuitinKeywordVersion(11);

  ASSERT_EQ(10, db.GetDefaulSearchProviderID());
  ASSERT_EQ(11, db.GetBuitinKeywordVersion());
}

TEST_F(WebDatabaseTest, UpdateKeyword) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(L"short_name");
  template_url.set_keyword(L"keyword");
  GURL favicon_url("http://favicon.url/");
  GURL originating_url("http://originating.url/");
  template_url.SetFavIconURL(favicon_url);
  template_url.SetURL(L"http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  template_url.set_show_in_default_list(true);
  template_url.SetSuggestionsURL(L"url2", 0, 0);
  SetID(1, &template_url);

  EXPECT_TRUE(db.AddKeyword(template_url));

  GURL originating_url2("http://originating.url/");
  template_url.set_originating_url(originating_url2);
  template_url.set_autogenerate_keyword(true);
  EXPECT_EQ(L"url", template_url.keyword());
  template_url.add_input_encoding("Shift_JIS");
  set_prepopulate_id(&template_url, 5);
  EXPECT_TRUE(db.UpdateKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));

  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());

  EXPECT_EQ(template_url.keyword(), restored_url->keyword());

  EXPECT_TRUE(restored_url->autogenerate_keyword());

  EXPECT_TRUE(favicon_url == restored_url->GetFavIconURL());

  EXPECT_TRUE(restored_url->safe_for_autoreplace());

  EXPECT_TRUE(restored_url->show_in_default_list());

  EXPECT_EQ(GetID(&template_url), GetID(restored_url));

  EXPECT_TRUE(originating_url2 == restored_url->originating_url());

  ASSERT_EQ(1U, restored_url->input_encodings().size());
  ASSERT_EQ("Shift_JIS", restored_url->input_encodings()[0]);

  EXPECT_EQ(template_url.suggestions_url()->url(),
            restored_url->suggestions_url()->url());

  EXPECT_EQ(template_url.id(), restored_url->id());

  EXPECT_EQ(template_url.prepopulate_id(), restored_url->prepopulate_id());

  delete restored_url;
}

TEST_F(WebDatabaseTest, KeywordWithNoFavicon) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));

  TemplateURL template_url;
  template_url.set_short_name(L"short_name");
  template_url.set_keyword(L"keyword");
  template_url.SetURL(L"http://url/", 0, 0);
  template_url.set_safe_for_autoreplace(true);
  SetID(-100, &template_url);

  EXPECT_TRUE(db.AddKeyword(template_url));

  std::vector<TemplateURL*> template_urls;
  EXPECT_TRUE(db.GetKeywords(&template_urls));
  EXPECT_EQ(1U, template_urls.size());
  const TemplateURL* restored_url = template_urls.front();

  EXPECT_EQ(template_url.short_name(), restored_url->short_name());
  EXPECT_EQ(template_url.keyword(), restored_url->keyword());
  EXPECT_TRUE(!restored_url->GetFavIconURL().is_valid());
  EXPECT_TRUE(restored_url->safe_for_autoreplace());
  EXPECT_EQ(GetID(&template_url), GetID(restored_url));
  delete restored_url;
}

TEST_F(WebDatabaseTest, Logins) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());

  // Example password form.
  PasswordForm form;
  form.origin = GURL("http://www.google.com/accounts/LoginAuth");
  form.action = GURL("http://www.google.com/accounts/Login");
  form.username_element = L"Email";
  form.username_value = L"test@gmail.com";
  form.password_element = L"Passwd";
  form.password_value = L"test";
  form.submit_element = L"signIn";
  form.signon_realm = "http://www.google.com/";
  form.ssl_valid = false;
  form.preferred = false;
  form.scheme = PasswordForm::SCHEME_HTML;

  // Add it and make sure it is there.
  EXPECT_TRUE(db.AddLogin(form));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.origin = GURL("http://www.google.com/new/accounts/LoginAuth");
  form2.submit_element = L"reallySignIn";

  // Match against an inexact copy
  EXPECT_TRUE(db.GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Uh oh, the site changed origin & action URL's all at once!
  PasswordForm form3(form2);
  form3.action = GURL("http://www.google.com/new/accounts/Login");

  // signon_realm is the same, should match.
  EXPECT_TRUE(db.GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Imagine the site moves to a secure server for login.
  PasswordForm form4(form3);
  form4.signon_realm = "https://www.google.com/";
  form4.ssl_valid = true;

  // We have only an http record, so no match for this.
  EXPECT_TRUE(db.GetLogins(form4, &result));
  EXPECT_EQ(0U, result.size());

  // Let's imagine the user logs into the secure site.
  EXPECT_TRUE(db.AddLogin(form4));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  delete result[0];
  delete result[1];
  result.clear();

  // Now the match works
  EXPECT_TRUE(db.GetLogins(form4, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db.RemoveLogin(form));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db.GetLogins(form, &result));
  EXPECT_EQ(0U, result.size());

  // The user's request for the HTTPS site is intercepted
  // by an attacker who presents an invalid SSL cert.
  PasswordForm form5(form4);
  form5.ssl_valid = 0;

  // It will match in this case.
  EXPECT_TRUE(db.GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // User changes his password.
  PasswordForm form6(form5);
  form6.password_value = L"test6";
  form6.preferred = true;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  EXPECT_TRUE(db.UpdateLogin(form6));
  // matches
  EXPECT_TRUE(db.GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();
  // Only one record.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  // password element was updated.
  EXPECT_EQ(form6.password_value, result[0]->password_value);
  // Preferred login.
  EXPECT_TRUE(form6.preferred);
  delete result[0];
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db.RemoveLogin(form4));
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
}

TEST_F(WebDatabaseTest, Autofill) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));

  Time t1 = Time::Now();

  // Simulate the submission of a handful of entries in a field called "Name",
  // some more often than others.
  EXPECT_TRUE(db.AddAutofillFormElement(
      AutofillForm::Element(L"Name", L"Superman")));
  std::vector<std::wstring> v;
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(db.AddAutofillFormElement(
        AutofillForm::Element(L"Name", L"Clark Kent")));
  }
  for (int i = 0; i < 3; i++) {
    EXPECT_TRUE(db.AddAutofillFormElement(
        AutofillForm::Element(L"Name", L"Clark Sutter")));
  }
  for (int i = 0; i < 2; i++) {
    EXPECT_TRUE(db.AddAutofillFormElement(
        AutofillForm::Element(L"Favorite Color", L"Green")));
  }

  int count = 0;
  int64 pair_id = 0;

  // We have added the name Clark Kent 5 times, so count should be 5 and pair_id
  // should be somthing non-zero.
  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      AutofillForm::Element(L"Name", L"Clark Kent"), &pair_id, &count));
  EXPECT_EQ(5, count);
  EXPECT_NE(0, pair_id);

  // Storing in the data base should be case sensitive, so there should be no
  // database entry for clark kent lowercase.
  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      AutofillForm::Element(L"Name", L"clark kent"), &pair_id, &count));
  EXPECT_EQ(0, count);

  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      AutofillForm::Element(L"Favorite Color", L"Green"), &pair_id, &count));
  EXPECT_EQ(2, count);

  // This is meant to get a list of suggestions for Name.  The empty prefix
  // in the second argument means it should return all suggestions for a name
  // no matter what they start with.  The order that the names occur in the list
  // should be decreasing order by count.
  EXPECT_TRUE(db.GetFormValuesForElementName(L"Name", std::wstring(), &v, 6));
  EXPECT_EQ(3U, v.size());
  if (v.size() == 3) {
    EXPECT_EQ(L"Clark Kent", v[0]);
    EXPECT_EQ(L"Clark Sutter", v[1]);
    EXPECT_EQ(L"Superman", v[2]);
  }

  // If we query again limiting the list size to 1, we should only get the most
  // frequent entry.
  EXPECT_TRUE(db.GetFormValuesForElementName(L"Name", L"", &v, 1));
  EXPECT_EQ(1U, v.size());
  if (v.size() == 1) {
    EXPECT_EQ(L"Clark Kent", v[0]);
  }

  // Querying for suggestions given a prefix is case-insensitive, so the prefix
  // "cLa" shoud get suggestions for both Clarks.
  EXPECT_TRUE(db.GetFormValuesForElementName(L"Name", L"cLa", &v, 6));
  EXPECT_EQ(2U, v.size());
  if (v.size() == 2) {
    EXPECT_EQ(L"Clark Kent", v[0]);
    EXPECT_EQ(L"Clark Sutter", v[1]);
  }

  // Removing all elements since the beginning of this function should remove
  // everything from the database.
  EXPECT_TRUE(db.RemoveFormElementsAddedBetween(t1, Time()));

  EXPECT_TRUE(db.GetIDAndCountOfFormElement(
      AutofillForm::Element(L"Name", L"Clark Kent"), &pair_id, &count));
  EXPECT_EQ(0, count);

  EXPECT_TRUE(db.GetFormValuesForElementName(L"Name", L"", &v, 6));
  EXPECT_EQ(0U, v.size());

  // Now add some values with empty strings.
  const std::wstring kValue = L"  toto   ";
  EXPECT_TRUE(db.AddAutofillFormElement(AutofillForm::Element(L"blank", L"")));
  EXPECT_TRUE(db.AddAutofillFormElement(AutofillForm::Element(L"blank",
                                                              L" ")));
  EXPECT_TRUE(db.AddAutofillFormElement(AutofillForm::Element(L"blank",
                                                              L"      ")));
  EXPECT_TRUE(db.AddAutofillFormElement(AutofillForm::Element(L"blank",
                                                              kValue)));

  // They should be stored normally as the DB layer does not check for empty
  // values.
  v.clear();
  EXPECT_TRUE(db.GetFormValuesForElementName(L"blank", L"", &v, 10));
  EXPECT_EQ(4U, v.size());

  // Now we'll check that ClearAutofillEmptyValueElements() works as expected.
  db.ClearAutofillEmptyValueElements();

  v.clear();
  EXPECT_TRUE(db.GetFormValuesForElementName(L"blank", L"", &v, 10));
  ASSERT_EQ(1U, v.size());

  EXPECT_EQ(kValue, v[0]);
}

static bool AddTimestampedLogin(WebDatabase* db, std::string url,
                                std::wstring unique_string, const Time& time) {
  // Example password form.
  PasswordForm form;
  form.origin = GURL(url + std::string("/LoginAuth"));
  form.username_element = unique_string.c_str();
  form.username_value = unique_string.c_str();
  form.password_element = unique_string.c_str();
  form.submit_element = L"signIn";
  form.signon_realm = url;
  form.date_created = time;
  return db->AddLogin(form);
}

static void ClearResults(std::vector<PasswordForm*>* results) {
  for (size_t i = 0; i < results->size(); ++i) {
    delete (*results)[i];
  }
  results->clear();
}

TEST_F(WebDatabaseTest, ClearPrivateData_SavedPasswords) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());

  Time now = Time::Now();
  TimeDelta one_day = TimeDelta::FromDays(1);

  // Create one with a 0 time.
  EXPECT_TRUE(AddTimestampedLogin(&db, "1", L"foo1", Time()));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(AddTimestampedLogin(&db, "2", L"foo2", now - one_day));
  EXPECT_TRUE(AddTimestampedLogin(&db, "3", L"foo3", now));
  EXPECT_TRUE(AddTimestampedLogin(&db, "4", L"foo4", now + one_day));

  // Verify inserts worked.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(4U, result.size());
  ClearResults(&result);

  // Delete everything from today's date and on.
  db.RemoveLoginsCreatedBetween(now, Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  ClearResults(&result);

  // Delete with 0 date (should delete all).
  db.RemoveLoginsCreatedBetween(Time(), Time());

  // Verify nothing is left.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
}

TEST_F(WebDatabaseTest, BlacklistedLogins) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  ASSERT_EQ(0U, result.size());

  // Save a form as blacklisted.
  PasswordForm form;
  form.origin = GURL("http://www.google.com/accounts/LoginAuth");
  form.action = GURL("http://www.google.com/accounts/Login");
  form.username_element = L"Email";
  form.password_element = L"Passwd";
  form.submit_element = L"signIn";
  form.signon_realm = "http://www.google.com/";
  form.ssl_valid = false;
  form.preferred = true;
  form.blacklisted_by_user = true;
  form.scheme = PasswordForm::SCHEME_HTML;
  EXPECT_TRUE(db.AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db.GetAllLogins(&result, false));
  ASSERT_EQ(0U, result.size());

  // GetLogins should give the blacklisted result.
  EXPECT_TRUE(db.GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);

  // So should GetAll including blacklisted.
  EXPECT_TRUE(db.GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);
}

TEST_F(WebDatabaseTest, WebAppHasAllImages) {
  WebDatabase db;

  EXPECT_TRUE(db.Init(file_));
  GURL url("http://google.com/");

  // Initial value for unknown web app should be false.
  EXPECT_FALSE(db.GetWebAppHasAllImages(url));

  // Set the value and make sure it took.
  EXPECT_TRUE(db.SetWebAppHasAllImages(url, true));
  EXPECT_TRUE(db.GetWebAppHasAllImages(url));

  // Remove the app and make sure value reverts to default.
  EXPECT_TRUE(db.RemoveWebApp(url));
  EXPECT_FALSE(db.GetWebAppHasAllImages(url));
}

TEST_F(WebDatabaseTest, WebAppImages) {
  WebDatabase db;

  ASSERT_TRUE(db.Init(file_));
  GURL url("http://google.com/");

  // Web app should initially have no images.
  std::vector<SkBitmap> images;
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(0U, images.size());

  // Add an image.
  SkBitmap image;
  image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);
  ASSERT_TRUE(db.SetWebAppImage(url, image));

  // Make sure we get the image back.
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(1U, images.size());
  ASSERT_EQ(16, images[0].width());
  ASSERT_EQ(16, images[0].height());

  // Add another 16x16 image and make sure it replaces the original.
  image.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);
  // Some random pixels so that we can identify the image.
  *(reinterpret_cast<unsigned char*>(image.getPixels())) = 0xAB;
  ASSERT_TRUE(db.SetWebAppImage(url, image));
  images.clear();
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(1U, images.size());
  ASSERT_EQ(16, images[0].width());
  ASSERT_EQ(16, images[0].height());
  images[0].lockPixels();
  unsigned char* pixels =
      reinterpret_cast<unsigned char*>(images[0].getPixels());
  ASSERT_TRUE(pixels != NULL);
  ASSERT_EQ(0xAB, *pixels);
  images[0].unlockPixels();

  // Add another image at a bigger size.
  image.setConfig(SkBitmap::kARGB_8888_Config, 32, 32);
  image.allocPixels();
  image.eraseColor(SK_ColorBLACK);
  ASSERT_TRUE(db.SetWebAppImage(url, image));

  // Make sure we get both images back.
  images.clear();
  ASSERT_TRUE(db.GetWebAppImages(url, &images));
  ASSERT_EQ(2U, images.size());
  if (images[0].width() == 16) {
    ASSERT_EQ(16, images[0].width());
    ASSERT_EQ(16, images[0].height());
    ASSERT_EQ(32, images[1].width());
    ASSERT_EQ(32, images[1].height());
  } else {
    ASSERT_EQ(32, images[0].width());
    ASSERT_EQ(32, images[0].height());
    ASSERT_EQ(16, images[1].width());
    ASSERT_EQ(16, images[1].height());
  }
}

