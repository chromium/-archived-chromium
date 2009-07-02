// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/password_manager/login_database.h"
#if defined(OS_MACOSX)
#include "chrome/browser/password_manager/login_database_mac.h"
#endif
#include "chrome/common/chrome_paths.h"
#include "webkit/glue/password_form.h"

using webkit_glue::PasswordForm;

class LoginDatabaseTest : public testing::Test {
 protected:
  virtual void SetUp() {
    PathService::Get(chrome::DIR_TEST_DATA, &file_);
    const std::string test_db =
        "TestMetadataStoreMacDatabase" +
        Int64ToString(base::Time::Now().ToInternalValue()) + ".db";
    file_ = file_.AppendASCII(test_db);
    file_util::Delete(file_, false);
  }

  virtual void TearDown() {
    file_util::Delete(file_, false);
  }

  FilePath file_;
};

// Returns the correct concrete subclass for the platform. Caller is responsible
// for delete-ing the return object.
static LoginDatabase* CreateLoginDatabase() {
#if defined(OS_MACOSX)
  return new LoginDatabaseMac();
#else
  return NULL;
#endif
}

TEST_F(LoginDatabaseTest, Logins) {
  scoped_ptr<LoginDatabase> db(CreateLoginDatabase());
  if (!db.get())
    return;

  ASSERT_TRUE(db->Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
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
  EXPECT_TRUE(db->AddLogin(form));
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Match against an exact copy.
  EXPECT_TRUE(db->GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The example site changes...
  PasswordForm form2(form);
  form2.origin = GURL("http://www.google.com/new/accounts/LoginAuth");
  form2.submit_element = L"reallySignIn";

  // Match against an inexact copy
  EXPECT_TRUE(db->GetLogins(form2, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Uh oh, the site changed origin & action URLs all at once!
  PasswordForm form3(form2);
  form3.action = GURL("http://www.google.com/new/accounts/Login");

  // signon_realm is the same, should match.
  EXPECT_TRUE(db->GetLogins(form3, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // Imagine the site moves to a secure server for login.
  PasswordForm form4(form3);
  form4.signon_realm = "https://www.google.com/";
  form4.ssl_valid = true;

  // We have only an http record, so no match for this.
  EXPECT_TRUE(db->GetLogins(form4, &result));
  EXPECT_EQ(0U, result.size());

  // Let's imagine the user logs into the secure site.
  EXPECT_TRUE(db->AddLogin(form4));
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  delete result[0];
  delete result[1];
  result.clear();

  // Now the match works
  EXPECT_TRUE(db->GetLogins(form4, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The user chose to forget the original but not the new.
  EXPECT_TRUE(db->RemoveLogin(form));
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // The old form wont match the new site (http vs https).
  EXPECT_TRUE(db->GetLogins(form, &result));
  EXPECT_EQ(0U, result.size());

  // The user's request for the HTTPS site is intercepted
  // by an attacker who presents an invalid SSL cert.
  PasswordForm form5(form4);
  form5.ssl_valid = 0;

  // It will match in this case.
  EXPECT_TRUE(db->GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();

  // User changes his password.
  PasswordForm form6(form5);
  form6.password_value = L"test6";
  form6.preferred = true;

  // We update, and check to make sure it matches the
  // old form, and there is only one record.
  int rows_changed = 0;
  EXPECT_TRUE(db->UpdateLogin(form6, &rows_changed));
  EXPECT_EQ(1, rows_changed);
  // matches
  EXPECT_TRUE(db->GetLogins(form5, &result));
  EXPECT_EQ(1U, result.size());
  delete result[0];
  result.clear();
  // Only one record.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  // Password element was updated.
#if defined(OS_MACOSX)
  // On the Mac we should never be storing passwords in the database.
  EXPECT_EQ(L"", result[0]->password_value);
#else
  EXPECT_EQ(form6.password_value, result[0]->password_value);
#endif
  // Preferred login.
  EXPECT_TRUE(form6.preferred);
  delete result[0];
  result.clear();

  // Make sure everything can disappear.
  EXPECT_TRUE(db->RemoveLogin(form4));
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
}

static bool AddTimestampedLogin(LoginDatabase* db, std::string url,
                                std::wstring unique_string,
                                const base::Time& time) {
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

TEST_F(LoginDatabaseTest, ClearPrivateData_SavedPasswords) {
  scoped_ptr<LoginDatabase> db(CreateLoginDatabase());
  if (!db.get())
    return;

  EXPECT_TRUE(db->Init(file_));

  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());

  base::Time now = base::Time::Now();
  base::TimeDelta one_day = base::TimeDelta::FromDays(1);

  // Create one with a 0 time.
  EXPECT_TRUE(AddTimestampedLogin(db.get(), "1", L"foo1", base::Time()));
  // Create one for now and +/- 1 day.
  EXPECT_TRUE(AddTimestampedLogin(db.get(), "2", L"foo2", now - one_day));
  EXPECT_TRUE(AddTimestampedLogin(db.get(), "3", L"foo3", now));
  EXPECT_TRUE(AddTimestampedLogin(db.get(), "4", L"foo4", now + one_day));

  // Verify inserts worked.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(4U, result.size());
  ClearResults(&result);

  // Delete everything from today's date and on.
  db->RemoveLoginsCreatedBetween(now, base::Time());

  // Should have deleted half of what we inserted.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(2U, result.size());
  ClearResults(&result);

  // Delete with 0 date (should delete all).
  db->RemoveLoginsCreatedBetween(base::Time(), base::Time());

  // Verify nothing is left.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(0U, result.size());
}

TEST_F(LoginDatabaseTest, BlacklistedLogins) {
  scoped_ptr<LoginDatabase> db(CreateLoginDatabase());
  if (!db.get())
    return;

  EXPECT_TRUE(db->Init(file_));
  std::vector<PasswordForm*> result;

  // Verify the database is empty.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
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
  EXPECT_TRUE(db->AddLogin(form));

  // Get all non-blacklisted logins (should be none).
  EXPECT_TRUE(db->GetAllLogins(&result, false));
  ASSERT_EQ(0U, result.size());

  // GetLogins should give the blacklisted result.
  EXPECT_TRUE(db->GetLogins(form, &result));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);

  // So should GetAll including blacklisted.
  EXPECT_TRUE(db->GetAllLogins(&result, true));
  EXPECT_EQ(1U, result.size());
  ClearResults(&result);
}
