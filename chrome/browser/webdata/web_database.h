// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/meta_table_helper.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/sqlite_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/glue/autofill_form.h"

namespace base {
class Time;
}

namespace webkit_glue {
struct PasswordForm;
}

#if defined(OS_WIN)
struct IE7PasswordInfo;
#endif

////////////////////////////////////////////////////////////////////////////////
//
// A Sqlite database instance to store all the meta data we have about web pages
//
////////////////////////////////////////////////////////////////////////////////
class WebDatabase {
 public:
  WebDatabase();
  ~WebDatabase();

  // Initialize the database given a name. The name defines where the sqlite
  // file is. If false is returned, no other method should be called.
  bool Init(const std::wstring& db_name);

  // Transactions management
  void BeginTransaction();
  void CommitTransaction();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Keywords
  //
  //////////////////////////////////////////////////////////////////////////////

  // Adds a new keyword, updating the id field on success.
  // Returns true if successful.
  bool AddKeyword(const TemplateURL& url);

  // Removes the specified keyword.
  // Returns true if successful.
  bool RemoveKeyword(TemplateURL::IDType id);

  // Loads the keywords into the specified vector. It's up to the caller to
  // delete the returned objects.
  // Returns true on success.
  bool GetKeywords(std::vector<TemplateURL*>* urls) const;

  // Updates the database values for the specified url.
  // Returns true on success.
  bool UpdateKeyword(const TemplateURL& url);

  // ID (TemplateURL->id) of the default search provider.
  bool SetDefaultSearchProviderID(int64 id);
  int64 GetDefaulSearchProviderID();

  // Version of the builtin keywords.
  bool SetBuitinKeywordVersion(int version);
  int GetBuitinKeywordVersion();

  //////////////////////////////////////////////////////////////////////////////
  //
  // Password manager support
  //
  //////////////////////////////////////////////////////////////////////////////

  // Adds |form| to the list of remembered password forms.
  bool AddLogin(const webkit_glue::PasswordForm& form);

#if defined(OS_WIN)
  // Adds |info| to the list of imported passwords from ie7/ie8.
  bool AddIE7Login(const IE7PasswordInfo& info);

  // Removes |info| from the list of imported passwords from ie7/ie8.
  bool RemoveIE7Login(const IE7PasswordInfo& info);

  // Return the ie7/ie8 login matching |info|.
  bool GetIE7Login(const IE7PasswordInfo& info, IE7PasswordInfo* result);
#endif

  // Updates remembered password form.
  bool UpdateLogin(const webkit_glue::PasswordForm& form);

  // Removes |form| from the list of remembered password forms.
  bool RemoveLogin(const webkit_glue::PasswordForm& form);

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsCreatedBetween(const base::Time delete_begin,
                                  const base::Time delete_end);

  // Loads a list of matching password forms into the specified vector |forms|.
  // The list will contain all possibly relevant entries to the observed |form|,
  // including blacklisted matches.
  bool GetLogins(const webkit_glue::PasswordForm& form,
                 std::vector<webkit_glue::PasswordForm*>* forms) const;

  // Loads the complete list of password forms into the specified vector |forms|
  // if include_blacklisted is true, otherwise only loads those which are
  // actually autofillable; i.e haven't been blacklisted by the user selecting
  // the 'Never for this site' button.
  bool GetAllLogins(std::vector<webkit_glue::PasswordForm*>* forms,
                    bool include_blacklisted) const;

  //////////////////////////////////////////////////////////////////////////////
  //
  // Autofill
  //
  //////////////////////////////////////////////////////////////////////////////

  // Records the form elements in |elements| in the database in the autofill
  // table.
  bool AddAutofillFormElements(
      const std::vector<webkit_glue::AutofillForm::Element>& elements);

  // Records a single form element in in the database in the autofill table.
  bool AddAutofillFormElement(
      const webkit_glue::AutofillForm::Element& element);

  // Retrieves a vector of all values which have been recorded in the autofill
  // table as the value in a form element with name |name| and which start with
  // |prefix|.  The comparison of the prefix is case insensitive.
  bool GetFormValuesForElementName(const std::wstring& name,
                                   const std::wstring& prefix,
                                   std::vector<std::wstring>* values,
                                   int limit) const;

  // Removes rows from autofill_dates if they were created on or after
  // |delete_begin| and strictly before |delete_end|.  Decrements the count of
  // the corresponding rows in the autofill table, and removes those rows if the
  // count goes to 0.
  bool RemoveFormElementsAddedBetween(const base::Time delete_begin,
                                      const base::Time delete_end);

  // Removes from autofill_dates rows with given pair_id where date_created lies
  // between delte_begin and delte_end.
  bool RemoveFormElementForTimeRange(int64 pair_id,
                                     const base::Time delete_begin,
                                     const base::Time delete_end,
                                     int* how_many);

  // Increments the count in the row corresponding to |pair_id| by |delta|.
  // Removes the row from the table if the count becomes 0.
  bool AddToCountOfFormElement(int64 pair_id, int delta);

  // Gets the pair_id and count entries from name and value specified in
  // |element|.  Sets *count to 0 if there is no such row in the table.
  bool GetIDAndCountOfFormElement(
      const webkit_glue::AutofillForm::Element& element,
      int64* pair_id,
      int* count) const;

  // Gets the count only given the pair_id.
  bool GetCountOfFormElement(int64 pair_id,
                             int* count) const;

  // Updates the count entry in the row corresponding to |pair_id| to |count|.
  bool SetCountOfFormElement(int64 pair_id, int count);

  // Adds a new row to the autofill table with name and value given in
  // |element|.  Sets *pair_id to the pair_id of the new row.
  bool InsertFormElement(
      const webkit_glue::AutofillForm::Element& element,
      int64* pair_id);

  // Adds a new row to the autofill_dates table.
  bool InsertPairIDAndDate(int64 pair_id, const base::Time date_created);

  // Removes row from the autofill tables given |pair_id|.
  bool RemoveFormElementForID(int64 pair_id);

  // Removes row from the autofill tables for the given |name| |value| pair.
  bool RemoveFormElement(const std::wstring& name, const std::wstring& value);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps
  //
  //////////////////////////////////////////////////////////////////////////////

  bool SetWebAppImage(const GURL& url, const SkBitmap& image);
  bool GetWebAppImages(const GURL& url, std::vector<SkBitmap>* images) const;

  bool SetWebAppHasAllImages(const GURL& url, bool has_all_images);
  bool GetWebAppHasAllImages(const GURL& url) const;

  bool RemoveWebApp(const GURL& url);

 private:
  FRIEND_TEST(WebDatabaseTest, Autofill);

  // Removes empty values for autofill that were incorrectly stored in the DB
  // (see bug http://crbug.com/6111).
  // TODO(jcampan): http://crbug.com/7564 remove when we think all users have
  //                run this code.
  bool ClearAutofillEmptyValueElements();

  bool InitKeywordsTable();
  bool InitLoginsTable();
  bool InitAutofillTable();
  bool InitAutofillDatesTable();
  bool InitWebAppIconsTable();
  bool InitWebAppsTable();

  void MigrateOldVersionsAsNeeded();

  sqlite3* db_;
  int transaction_nesting_;
  MetaTableHelper meta_table_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabase);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_H__
