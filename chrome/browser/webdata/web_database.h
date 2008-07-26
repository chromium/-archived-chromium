// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_WEBDATA_WEB_DATABASE_H__
#define CHROME_BROWSER_WEBDATA_WEB_DATABASE_H__

#include <string>
#include <vector>
#include <hash_map>

#include "base/basictypes.h"
#include "chrome/browser/meta_table_helper.h"
#include "chrome/browser/template_url.h"
#include "chrome/common/sqlite_utils.h"
#include "skia/include/SkBitmap.h"

class Time;
struct PasswordForm;
struct IE7PasswordInfo;

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
  bool GetKeywords(std::vector<TemplateURL*>* urls);

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
  bool AddLogin(const PasswordForm& form);

  // Adds |info| to the list of imported passwords from ie7/ie8.
  bool AddIE7Login(const IE7PasswordInfo& info);

  // Updates remembered password form.
  bool UpdateLogin(const PasswordForm& form);

  // Removes |form| from the list of remembered password forms.
  bool RemoveLogin(const PasswordForm& form);

  // Removes |info| from the list of imported passwords from ie7/ie8.
  bool RemoveIE7Login(const IE7PasswordInfo& info);

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction.
  bool RemoveLoginsCreatedBetween(const Time delete_begin,
                                  const Time delete_end);

  // Loads a list of matching password forms into the specified vector |forms|.
  // The list will contain all possibly relevant entries to the observed |form|,
  // including blacklisted matches.
  bool GetLogins(const PasswordForm& form, std::vector<PasswordForm*>* forms);

  // Return the ie7/ie8 login matching |info|.
  bool GetIE7Login(const IE7PasswordInfo& info, IE7PasswordInfo* result);

  // Loads the complete list of password forms into the specified vector |forms|
  // if include_blacklisted is true, otherwise only loads those which are
  // actually autofillable; i.e haven't been blacklisted by the user selecting
  // the 'Never for this site' button.
  bool GetAllLogins(std::vector<PasswordForm*>* forms,
                    bool include_blacklisted);

  //////////////////////////////////////////////////////////////////////////////
  //
  // Web Apps
  //
  //////////////////////////////////////////////////////////////////////////////

  bool SetWebAppImage(const GURL& url, const SkBitmap& image);
  bool GetWebAppImages(const GURL& url, std::vector<SkBitmap>* images);

  bool SetWebAppHasAllImages(const GURL& url, bool has_all_images);
  bool GetWebAppHasAllImages(const GURL& url);

  bool RemoveWebApp(const GURL& url);

 private:
  friend class WebDatabaseTest;

  bool InitKeywordsTable();
  bool InitLoginsTable();
  bool InitWebAppIconsTable();
  bool InitWebAppsTable();

  sqlite3* db_;
  int transaction_nesting_;
  MetaTableHelper meta_table_;

  DISALLOW_EVIL_CONSTRUCTORS(WebDatabase);
};

#endif  // CHROME_BROWSER_WEBDATA_WEB_DATABASE_H__
