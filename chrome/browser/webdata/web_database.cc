// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/gfx/png_decoder.h"
#include "base/gfx/png_encoder.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/scoped_vector.h"
#include "webkit/glue/password_form.h"

#if defined(OS_POSIX)
// TODO(port): get rid of this include. It's used just to provide declarations
// and stub definitions for classes we encouter during the porting effort.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

// TODO(port): Get rid of this section and finish porting.
#if defined(OS_WIN)
// Encryptor is the *wrong* way of doing things; we need to turn it into a
// bottleneck to use the platform methods (e.g. Keychain on the Mac). That's
// going to take a massive change in its API...
#include "chrome/browser/password_manager/encryptor.h"
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Schema
//
// keywords                 Most of the columns mirror that of a field in
//                          TemplateURL. See TemplateURL for more details.
//   id
//   short_name
//   keyword
//   favicon_url
//   url
//   show_in_default_list
//   safe_for_autoreplace
//   originating_url
//   date_created           This column was added after we allowed keywords.
//                          Keywords created before we started tracking
//                          creation date have a value of 0 for this.
//   usage_count
//   input_encodings        Semicolon separated list of supported input
//                          encodings, may be empty.
//   suggest_url
//   prepopulate_id         See TemplateURL::prepoulate_id.
//   autogenerate_keyword
//
// logins
//   origin_url
//   action_url
//   username_element
//   username_value
//   password_element
//   password_value
//   submit_element
//   signon_realm        The authority (scheme, host, port).
//   ssl_valid           SSL status of page containing the form at first
//                       impression.
//   preferred           MRU bit.
//   date_created        This column was added after logins support. "Legacy"
//                       entries have a value of 0.
//   blacklisted_by_user Tracks whether or not the user opted to 'never
//                       remember'
//                       passwords for this site.
//
// autofill
//   name                The name of the input as specified in the html.
//   value               The literal contents of the text field.
//   value_lower         The contents of the text field made lower_case.
//   pair_id             An ID number unique to the row in the table.
//   count               How many times the user has entered the string |value|
//                       in a field of name |name|.
//
// autofill_dates        This table associates a row to each separate time the
//                       user submits a form containing a certain name/value
//                       pair.  The |pair_id| should match the |pair_id| field
//                       in the appropriate row of the autofill table.
//   pair_id
//   date_created
//
// web_app_icons
//   url         URL of the web app.
//   width       Width of the image.
//   height      Height of the image.
//   image       PNG encoded image data.
//
// web_apps
//   url                 URL of the web app.
//   has_all_images      Do we have all the images?
//
////////////////////////////////////////////////////////////////////////////////

using base::Time;

// Current version number.
static const int kCurrentVersionNumber = 22;
static const int kCompatibleVersionNumber = 21;

// Keys used in the meta table.
static const char* kDefaultSearchProviderKey = "Default Search Provider ID";
static const char* kBuiltinKeywordVersion = "Builtin Keyword Version";

std::string JoinStrings(const std::string& separator,
                        const std::vector<std::string>& strings) {
  if (strings.empty())
    return std::string();
  std::vector<std::string>::const_iterator i(strings.begin());
  std::string result(*i);
  while (++i != strings.end())
    result += separator + *i;
  return result;
}

WebDatabase::WebDatabase() : db_(NULL), transaction_nesting_(0) {
}

WebDatabase::~WebDatabase() {
  if (db_) {
    DCHECK(transaction_nesting_ == 0) <<
        "Forgot to close the transaction on shutdown";
    sqlite3_close(db_);
    db_ = NULL;
  }
}

void WebDatabase::BeginTransaction() {
  DCHECK(db_);
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to begin transaction";
  }
  transaction_nesting_++;
}

void WebDatabase::CommitTransaction() {
  DCHECK(db_);
  DCHECK(transaction_nesting_ > 0) << "Committing too many transaction";
  transaction_nesting_--;
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "COMMIT", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to commit transaction";
  }
}

bool WebDatabase::Init(const std::wstring& db_name) {
  // Open the database, using the narrow version of open so that
  // the DB is in UTF-8.
  if (sqlite3_open(WideToUTF8(db_name).c_str(), &db_) != SQLITE_OK) {
    LOG(WARNING) << "Unable to open the web database.";
    return false;
  }

  // We don't store that much data in the tables so use a small page size.
  // This provides a large benefit for empty tables (which is very likely with
  // the tables we create).
  sqlite3_exec(db_, "PRAGMA page_size=2048", NULL, NULL, NULL);

  // We shouldn't have much data and what access we currently have is quite
  // infrequent. So we go with a small cache size.
  sqlite3_exec(db_, "PRAGMA cache_size=32", NULL, NULL, NULL);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  // Initialize various tables
  SQLTransaction transaction(db_);
  transaction.Begin();

  // Version check.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db_))
    return false;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Web database is too new.";
    return false;
  }

  // Initialize the tables.
  if (!InitKeywordsTable() || !InitLoginsTable() || !InitWebAppIconsTable() ||
      !InitWebAppsTable() || !InitAutofillTable() ||
      !InitAutofillDatesTable()) {
    LOG(WARNING) << "Unable to initialize the web database.";
    return false;
  }

  // If the file on disk is an older database version, bring it up to date.
  MigrateOldVersionsAsNeeded();

  return (transaction.Commit() == SQLITE_OK);
}

bool WebDatabase::SetWebAppImage(const GURL& url,
                                 const SkBitmap& image) {
  SQLStatement s;
  if (s.prepare(db_,
                "INSERT OR REPLACE INTO web_app_icons "
                "(url, width, height, image) VALUES (?, ?, ?, ?)")
      != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::vector<unsigned char> image_data;
  PNGEncoder::EncodeBGRASkBitmap(image, false, &image_data);

  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.bind_int(1, image.width());
  s.bind_int(2, image.height());
  s.bind_blob(3, &image_data.front(), static_cast<int>(image_data.size()));
  return s.step() == SQLITE_DONE;
}

bool WebDatabase::GetWebAppImages(const GURL& url,
                                  std::vector<SkBitmap>* images) const {
  SQLStatement s;
  if (s.prepare(db_, "SELECT image FROM web_app_icons WHERE url=?") !=
      SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  while (s.step() == SQLITE_ROW) {
    SkBitmap image;
    std::vector<unsigned char> image_data;
    s.column_blob_as_vector(0, &image_data);
    if (PNGDecoder::Decode(&image_data, &image)) {
      images->push_back(image);
    } else {
      // Should only have valid image data in the db.
      NOTREACHED();
    }
  }
  return true;
}

bool WebDatabase::SetWebAppHasAllImages(const GURL& url,
                                        bool has_all_images) {
  SQLStatement s;
  if (s.prepare(db_, "INSERT OR REPLACE INTO web_apps (url, has_all_images) "
                     "VALUES (?, ?)") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.bind_int(1, has_all_images ? 1 : 0);
  return (s.step() == SQLITE_DONE);
}

bool WebDatabase::GetWebAppHasAllImages(const GURL& url) const {
  SQLStatement s;
  if (s.prepare(db_, "SELECT has_all_images FROM web_apps "
                     "WHERE url=?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return (s.step() == SQLITE_ROW && s.column_int(0) == 1);
}

bool WebDatabase::RemoveWebApp(const GURL& url) {
  SQLStatement delete_s;
  if (delete_s.prepare(db_, "DELETE FROM web_app_icons WHERE url = ?") !=
      SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  if (delete_s.step() != SQLITE_DONE)
    return false;

  SQLStatement delete_s2;
  if (delete_s2.prepare(db_, "DELETE FROM web_apps WHERE url = ?") !=
      SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s2.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return (delete_s2.step() == SQLITE_DONE);
}

bool WebDatabase::InitKeywordsTable() {
  if (!DoesSqliteTableExist(db_, "keywords")) {
    if (sqlite3_exec(db_, "CREATE TABLE keywords ("
                     "id INTEGER PRIMARY KEY,"
                     "short_name VARCHAR NOT NULL,"
                     "keyword VARCHAR NOT NULL,"
                     "favicon_url VARCHAR NOT NULL,"
                     "url VARCHAR NOT NULL,"
                     "show_in_default_list INTEGER,"
                     "safe_for_autoreplace INTEGER,"
                     "originating_url VARCHAR,"
                     "date_created INTEGER DEFAULT 0,"
                     "usage_count INTEGER DEFAULT 0,"
                     "input_encodings VARCHAR,"
                     "suggest_url VARCHAR,"
                     "prepopulate_id INTEGER DEFAULT 0,"
                     "autogenerate_keyword INTEGER DEFAULT 0)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitLoginsTable() {
  if (!DoesSqliteTableExist(db_, "logins")) {
    // First time
    if (sqlite3_exec(db_, "CREATE TABLE logins ("
                     "origin_url VARCHAR NOT NULL, "
                     "action_url VARCHAR, "
                     "username_element VARCHAR, "
                     "username_value VARCHAR, "
                     "password_element VARCHAR, "
                     "password_value BLOB, "
                     "submit_element VARCHAR, "
                     "signon_realm VARCHAR NOT NULL,"
                     "ssl_valid INTEGER NOT NULL,"
                     "preferred INTEGER NOT NULL,"
                     "date_created INTEGER NOT NULL,"
                     "blacklisted_by_user INTEGER NOT NULL,"
                     "scheme INTEGER NOT NULL,"
                     "UNIQUE "
                     "(origin_url, username_element, "
                     "username_value, password_element, "
                     "submit_element, signon_realm))",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_, "CREATE INDEX logins_signon ON "
                     "logins (signon_realm)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }

  if (!DoesSqliteTableExist(db_, "ie7_logins")) {
    // First time
    if (sqlite3_exec(db_, "CREATE TABLE ie7_logins ("
                     "url_hash VARCHAR NOT NULL, "
                     "password_value BLOB, "
                     "date_created INTEGER NOT NULL,"
                     "UNIQUE "
                     "(url_hash))",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_, "CREATE INDEX ie7_logins_hash ON "
                     "ie7_logins (url_hash)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillTable() {
  if (!DoesSqliteTableExist(db_, "autofill")) {
    if (sqlite3_exec(db_,
                     "CREATE TABLE autofill ("
                     "name VARCHAR, "
                     "value VARCHAR, "
                     "value_lower VARCHAR, "
                     "pair_id INTEGER PRIMARY KEY, "
                     "count INTEGER DEFAULT 1)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_,
                     "CREATE INDEX autofill_name ON "
                     "autofill (name)",
                     NULL, NULL, NULL) != SQLITE_OK) {
       NOTREACHED();
       return false;
    }
    if (sqlite3_exec(db_,
                     "CREATE INDEX autofill_name_value_lower ON "
                     "autofill (name, value_lower)",
                     NULL, NULL, NULL) != SQLITE_OK) {
       NOTREACHED();
       return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillDatesTable() {
  if (!DoesSqliteTableExist(db_, "autofill_dates")) {
    if (sqlite3_exec(db_,
                     "CREATE TABLE autofill_dates ( "
                     "pair_id INTEGER DEFAULT 0, "
                     "date_created INTEGER DEFAULT 0)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_,
                     "CREATE INDEX autofill_dates_pair_id ON "
                     "autofill_dates (pair_id)",
                     NULL, NULL, NULL) != SQLITE_OK) {
       NOTREACHED();
       return false;
    }
  }
  return true;
}

bool WebDatabase::InitWebAppIconsTable() {
  if (!DoesSqliteTableExist(db_, "web_app_icons")) {
    if (sqlite3_exec(db_, "CREATE TABLE web_app_icons ("
                     "url LONGVARCHAR,"
                     "width int,"
                     "height int,"
                     "image BLOB, UNIQUE (url, width, height))",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitWebAppsTable() {
  if (!DoesSqliteTableExist(db_, "web_apps")) {
    if (sqlite3_exec(db_, "CREATE TABLE web_apps ("
                     "url LONGVARCHAR UNIQUE,"
                     "has_all_images INTEGER NOT NULL)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_, "CREATE INDEX web_apps_url_index ON "
                     "web_apps (url)", NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

static void BindURLToStatement(const TemplateURL& url, SQLStatement* s) {
  s->bind_wstring(0, url.short_name());
  s->bind_wstring(1, url.keyword());
  GURL favicon_url = url.GetFavIconURL();
  if (!favicon_url.is_valid()) {
    s->bind_string(2, "");
  } else {
    s->bind_string(2, history::HistoryDatabase::GURLToDatabaseURL(
                       url.GetFavIconURL()));
  }
  if (url.url())
    s->bind_wstring(3, url.url()->url());
  else
    s->bind_wstring(3, std::wstring());
  s->bind_int(4, url.safe_for_autoreplace() ? 1 : 0);
  if (!url.originating_url().is_valid()) {
    s->bind_string(5, std::string());
  } else {
    s->bind_string(5, history::HistoryDatabase::GURLToDatabaseURL(
        url.originating_url()));
  }
  s->bind_int64(6, url.date_created().ToTimeT());
  s->bind_int(7, url.usage_count());
  s->bind_string(8, JoinStrings(";", url.input_encodings()));
  s->bind_int(9, url.show_in_default_list() ? 1 : 0);
  if (url.suggestions_url())
    s->bind_wstring(10, url.suggestions_url()->url());
  else
    s->bind_wstring(10, std::wstring());
  s->bind_int(11, url.prepopulate_id());
  s->bind_int(12, url.autogenerate_keyword() ? 1 : 0);
}

bool WebDatabase::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  SQLStatement s;
  if (s.prepare(db_,
                "INSERT INTO keywords "
                "(short_name, keyword, favicon_url, url, safe_for_autoreplace, "
                "originating_url, date_created, usage_count, input_encodings, "
                "show_in_default_list, suggest_url, prepopulate_id, "
                "autogenerate_keyword, id) VALUES "
                "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.bind_int64(13, url.id());
  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveKeyword(TemplateURL::IDType id) {
  DCHECK(id);
  SQLStatement s;
  if (s.prepare(db_,
                "DELETE FROM keywords WHERE id = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_int64(0, id);
  return s.step() == SQLITE_DONE;
}

bool WebDatabase::GetKeywords(std::vector<TemplateURL*>* urls) const {
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT id, short_name, keyword, favicon_url, url, "
                "safe_for_autoreplace, originating_url, date_created, "
                "usage_count, input_encodings, show_in_default_list, "
                "suggest_url, prepopulate_id, autogenerate_keyword "
                "FROM keywords ORDER BY id ASC") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  int result;
  while ((result = s.step()) == SQLITE_ROW) {
    TemplateURL* template_url = new TemplateURL();
    std::wstring tmp;
    template_url->set_id(s.column_int64(0));

    s.column_wstring(1, &tmp);
    DCHECK(!tmp.empty());
    template_url->set_short_name(tmp);

    s.column_wstring(2, &tmp);
    template_url->set_keyword(tmp);

    s.column_wstring(3, &tmp);
    if (!tmp.empty())
      template_url->SetFavIconURL(GURL(WideToUTF8(tmp)));

    s.column_wstring(4, &tmp);
    template_url->SetURL(tmp, 0, 0);

    template_url->set_safe_for_autoreplace(s.column_int(5) == 1);

    s.column_wstring(6, &tmp);
    if (!tmp.empty())
      template_url->set_originating_url(GURL(WideToUTF8(tmp)));

    template_url->set_date_created(Time::FromTimeT(s.column_int64(7)));

    template_url->set_usage_count(s.column_int(8));

    std::vector<std::string> encodings;
    SplitString(s.column_string(9), ';', &encodings);
    template_url->set_input_encodings(encodings);

    template_url->set_show_in_default_list(s.column_int(10) == 1);

    s.column_wstring(11, &tmp);
    template_url->SetSuggestionsURL(tmp, 0, 0);

    template_url->set_prepopulate_id(s.column_int(12));

    template_url->set_autogenerate_keyword(s.column_int(13) == 1);

    urls->push_back(template_url);
  }
  return result == SQLITE_DONE;
}

bool WebDatabase::UpdateKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  SQLStatement s;
  if (s.prepare(db_,
                "UPDATE keywords "
                "SET short_name=?, keyword=?, favicon_url=?, url=?, "
                "safe_for_autoreplace=?, originating_url=?, date_created=?, "
                "usage_count=?, input_encodings=?, show_in_default_list=?, "
                "suggest_url=?, prepopulate_id=?, autogenerate_keyword=? "
                "WHERE id=?")
                != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.bind_int64(13, url.id());
  return s.step() == SQLITE_DONE;
}

bool WebDatabase::SetDefaultSearchProviderID(int64 id) {
  return meta_table_.SetValue(kDefaultSearchProviderKey, id);
}

int64 WebDatabase::GetDefaulSearchProviderID() {
  int64 value = 0;
  meta_table_.GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool WebDatabase::SetBuitinKeywordVersion(int version) {
  return meta_table_.SetValue(kBuiltinKeywordVersion, version);
}

int WebDatabase::GetBuitinKeywordVersion() {
  int version = 0;
  meta_table_.GetValue(kBuiltinKeywordVersion, &version);
  return version;
}

bool WebDatabase::AddLogin(const PasswordForm& form) {
  SQLStatement s;
  std::string encrypted_password;
  if (s.prepare(db_,
                "INSERT OR REPLACE INTO logins "
                "(origin_url, action_url, username_element, username_value, "
                " password_element, password_value, submit_element, "
                " signon_realm, ssl_valid, preferred, date_created, "
                " blacklisted_by_user, scheme) "
                "VALUES "
                "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.origin.spec());
  s.bind_string(1, form.action.spec());
  s.bind_wstring(2, form.username_element);
  s.bind_wstring(3, form.username_value);
  s.bind_wstring(4, form.password_element);
  Encryptor::EncryptString16(WideToUTF16Hack(form.password_value),
                             &encrypted_password);
  s.bind_blob(5, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.bind_wstring(6, form.submit_element);
  s.bind_string(7, form.signon_realm);
  s.bind_int(8, form.ssl_valid);
  s.bind_int(9, form.preferred);
  s.bind_int64(10, form.date_created.ToTimeT());
  s.bind_int(11, form.blacklisted_by_user);
  s.bind_int(12, form.scheme);
  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::UpdateLogin(const PasswordForm& form) {
  SQLStatement s;
  std::string encrypted_password;
  if (s.prepare(db_, "UPDATE logins SET "
                "action_url = ?, "
                "password_value = ?, "
                "ssl_valid = ?, "
                "preferred = ? "
                "WHERE origin_url = ? AND "
                "username_element = ? AND "
                "username_value = ? AND "
                "password_element = ? AND "
                "signon_realm = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.action.spec());
  Encryptor::EncryptString16(WideToUTF16Hack(form.password_value),
                             &encrypted_password);
  s.bind_blob(1, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.bind_int(2, form.ssl_valid);
  s.bind_int(3, form.preferred);
  s.bind_string(4, form.origin.spec());
  s.bind_wstring(5, form.username_element);
  s.bind_wstring(6, form.username_value);
  s.bind_wstring(7, form.password_element);
  s.bind_string(8, form.signon_realm);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLogin(const PasswordForm& form) {
  SQLStatement s;
  // Remove a login by UNIQUE-constrained fields.
  if (s.prepare(db_,
                "DELETE FROM logins WHERE "
                "origin_url = ? AND "
                "username_element = ? AND "
                "username_value = ? AND "
                "password_element = ? AND "
                "submit_element = ? AND "
                "signon_realm = ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, form.origin.spec());
  s.bind_wstring(1, form.username_element);
  s.bind_wstring(2, form.username_value);
  s.bind_wstring(3, form.password_element);
  s.bind_wstring(4, form.submit_element);
  s.bind_string(5, form.signon_realm);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLoginsCreatedBetween(const Time delete_begin,
                                             const Time delete_end) {
  SQLStatement s1;
  if (s1.prepare(db_,
                "DELETE FROM logins WHERE "
                "date_created >= ? AND date_created < ?") != SQLITE_OK) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s1.bind_int64(0, delete_begin.ToTimeT());
  s1.bind_int64(1,
                delete_end.is_null() ?
                    std::numeric_limits<int64>::max() :
                    delete_end.ToTimeT());

  SQLStatement s2;
  if (s2.prepare(db_,
               "DELETE FROM ie7_logins WHERE "
               "date_created >= ? AND date_created < ?") != SQLITE_OK) {
    NOTREACHED() << "Statement 2 prepare failed";
    return false;
  }
  s2.bind_int64(0, delete_begin.ToTimeT());
  s2.bind_int64(1,
                delete_end.is_null() ?
                    std::numeric_limits<int64>::max() :
                    delete_end.ToTimeT());

  return s1.step() == SQLITE_DONE && s2.step() == SQLITE_DONE;
}

static void InitPasswordFormFromStatement(PasswordForm* form,
                                          SQLStatement* s) {
  std::string encrypted_password;
  std::string tmp;
  string16 decrypted_password;
  s->column_string(0, &tmp);
  form->origin = GURL(tmp);
  s->column_string(1, &tmp);
  form->action = GURL(tmp);
  s->column_wstring(2, &form->username_element);
  s->column_wstring(3, &form->username_value);
  s->column_wstring(4, &form->password_element);
  s->column_blob_as_string(5, &encrypted_password);
  Encryptor::DecryptString16(encrypted_password, &decrypted_password);
  form->password_value = UTF16ToWideHack(decrypted_password);
  s->column_wstring(6, &form->submit_element);
  s->column_string(7, &tmp);
  form->signon_realm = tmp;
  form->ssl_valid = (s->column_int(8) > 0);
  form->preferred = (s->column_int(9) > 0);
  form->date_created = Time::FromTimeT(s->column_int64(10));
  form->blacklisted_by_user = (s->column_int(11) > 0);
  int scheme_int = s->column_int(12);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
}

bool WebDatabase::GetLogins(const PasswordForm& form,
                            std::vector<PasswordForm*>* forms) const {
  DCHECK(forms);
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT origin_url, action_url, "
                "username_element, username_value, "
                "password_element, password_value, "
                "submit_element, signon_realm, "
                "ssl_valid, preferred, "
                "date_created, blacklisted_by_user, scheme FROM logins "
                "WHERE signon_realm == ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.signon_realm);

  int result;
  while ((result = s.step()) == SQLITE_ROW) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return result == SQLITE_DONE;
}

bool WebDatabase::GetAllLogins(std::vector<PasswordForm*>* forms,
                               bool include_blacklisted) const {
  DCHECK(forms);
  SQLStatement s;
  std::string stmt = "SELECT origin_url, action_url, "
                     "username_element, username_value, "
                     "password_element, password_value, "
                     "submit_element, signon_realm, ssl_valid, preferred, "
                     "date_created, blacklisted_by_user, scheme FROM logins ";
  if (!include_blacklisted)
    stmt.append("WHERE blacklisted_by_user == 0 ");
  stmt.append("ORDER BY origin_url");

  if (s.prepare(db_, stmt.c_str()) != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  int result;
  while ((result = s.step()) == SQLITE_ROW) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return result == SQLITE_DONE;
}

bool WebDatabase::AddAutofillFormElements(
    const std::vector<AutofillForm::Element>& elements) {
  bool result = true;
  for (std::vector<AutofillForm::Element>::const_iterator
       itr = elements.begin();
       itr != elements.end();
       itr++) {
    result = result && AddAutofillFormElement(*itr);
  }
  return result;
}

bool WebDatabase::ClearAutofillEmptyValueElements() {
  SQLStatement s;

  if (s.prepare(db_, "SELECT pair_id FROM autofill "
                     "WHERE TRIM(value)= \"\"") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::set<int64> ids;
  int result;
  while ((result = s.step()) == SQLITE_ROW)
    ids.insert(s.column_int64(0));

  bool success = true;
  for (std::set<int64>::const_iterator iter = ids.begin(); iter != ids.end();
       ++iter) {
    if (!RemoveFormElementForID(*iter))
      success = false;
  }

  return success;
}

bool WebDatabase::GetIDAndCountOfFormElement(
    const AutofillForm::Element& element, int64* pair_id, int* count) const {
  SQLStatement s;

  if (s.prepare(db_, "SELECT pair_id, count FROM autofill "
                     " WHERE name = ? AND value = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_wstring(0, element.name);
  s.bind_wstring(1, element.value);

  int result;

  *count = 0;

  if ((result = s.step()) == SQLITE_ROW) {
    *pair_id = s.column_int64(0);
    *count = s.column_int(1);
  }

  return true;
}

bool WebDatabase::GetCountOfFormElement(int64 pair_id, int* count) const {
  SQLStatement s;

  if (s.prepare(db_, "SELECT count FROM autofill "
                     " WHERE pair_id = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_int64(0, pair_id);

  int result;
  if ((result = s.step()) == SQLITE_ROW) {
    *count = s.column_int(0);
  } else {
    return false;
  }

  return true;
}

bool WebDatabase::InsertFormElement(const AutofillForm::Element& element,
                                    int64* pair_id) {
  SQLStatement s;

  if (s.prepare(db_, "INSERT INTO autofill "
                     "(name, value, value_lower) "
                     "VALUES (?, ?, ?)")
                     != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_wstring(0, element.name);
  s.bind_wstring(1, element.value);
  s.bind_wstring(2, l10n_util::ToLower(element.value));

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }

  *pair_id = sqlite3_last_insert_rowid(db_);

  return true;
}

bool WebDatabase::InsertPairIDAndDate(int64 pair_id,
                                      const Time date_created) {
  SQLStatement s;

  if (s.prepare(db_,
                "INSERT INTO autofill_dates "
                "(pair_id, date_created) VALUES (?, ?)")
                    != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_int64(0, pair_id);
  s.bind_int64(1, date_created.ToTimeT());

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebDatabase::SetCountOfFormElement(int64 pair_id, int count) {
  SQLStatement s;

  if (s.prepare(db_,
                "UPDATE autofill SET count = ? "
                "WHERE pair_id = ?")
                    != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_int(0, count);
  s.bind_int64(1, pair_id);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebDatabase::AddAutofillFormElement(const AutofillForm::Element& element) {
  SQLStatement s;
  int count = 0;
  int64 pair_id;

  if (!GetIDAndCountOfFormElement(element, &pair_id, &count))
    return false;

  if (count == 0 && !InsertFormElement(element, &pair_id))
    return false;

  return SetCountOfFormElement(pair_id, count + 1) &&
      InsertPairIDAndDate(pair_id, Time::Now());
}

bool WebDatabase::GetFormValuesForElementName(const std::wstring& name,
    const std::wstring& prefix,
    std::vector<std::wstring>* values,
    int limit) const {
  DCHECK(values);
  SQLStatement s;

  if (prefix.empty()) {
    if (s.prepare(db_, "SELECT value FROM autofill "
                       "WHERE name = ? "
                       "ORDER BY count DESC "
                       "LIMIT ?") != SQLITE_OK) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.bind_wstring(0, name);
    s.bind_int(1, limit);
  } else {
    std::wstring prefix_lower = l10n_util::ToLower(prefix);
    std::wstring next_prefix = prefix_lower;
    next_prefix[next_prefix.length() - 1]++;

    if (s.prepare(db_, "SELECT value FROM autofill "
                       "WHERE name = ? AND "
                       "value_lower >= ? AND "
                       "value_lower < ? "
                       "ORDER BY count DESC "
                       "LIMIT ?") != SQLITE_OK) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.bind_wstring(0, name);
    s.bind_wstring(1, prefix_lower);
    s.bind_wstring(2, next_prefix);
    s.bind_int(3, limit);
  }

  values->clear();
  int result;
  while ((result = s.step()) == SQLITE_ROW)
    values->push_back(s.column_wstring(0));

  return result == SQLITE_DONE;
}

bool WebDatabase::RemoveFormElementsAddedBetween(const Time delete_begin,
                                                 const Time delete_end) {
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT DISTINCT pair_id FROM autofill_dates WHERE "
                "date_created >= ? AND date_created < ?") != SQLITE_OK) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.bind_int64(0, delete_begin.ToTimeT());
  s.bind_int64(1,
               delete_end.is_null() ?
                   std::numeric_limits<int64>::max() :
                   delete_end.ToTimeT());

  std::vector<int64> pair_ids;
  int result;
  while ((result = s.step()) == SQLITE_ROW)
    pair_ids.push_back(s.column_int64(0));

  if (result != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }

  for (std::vector<int64>::iterator itr = pair_ids.begin();
       itr != pair_ids.end();
       itr++) {
    int how_many = 0;
    if (!RemoveFormElementForTimeRange(*itr, delete_begin, delete_end,
                                       &how_many)) {
      return false;
    }
    if (!AddToCountOfFormElement(*itr, -how_many))
      return false;
  }

  return true;
}

bool WebDatabase::RemoveFormElementForTimeRange(int64 pair_id,
                                                const Time delete_begin,
                                                const Time delete_end,
                                                int* how_many) {
  SQLStatement s;
  if (s.prepare(db_,
                "DELETE FROM autofill_dates WHERE pair_id = ? AND "
                "date_created >= ? AND date_created < ?") != SQLITE_OK) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.bind_int64(0, pair_id);
  s.bind_int64(1, delete_begin.is_null() ? 0 : delete_begin.ToTimeT());
  s.bind_int64(2, delete_end.is_null() ? std::numeric_limits<int64>::max() :
                                         delete_end.ToTimeT());

  bool result = (s.step() == SQLITE_DONE);
  if (how_many)
    *how_many = sqlite3_changes(db_);

  return result;
}

bool WebDatabase::RemoveFormElement(const std::wstring& name,
                                    const std::wstring& value) {
  // Find the id for that pair.
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT pair_id FROM autofill WHERE  name = ? AND value= ?") !=
                SQLITE_OK) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.bind_wstring(0, name);
  s.bind_wstring(1, value);

  int result = s.step();
  if (result != SQLITE_ROW)
    return false;

  return RemoveFormElementForID(s.column_int64(0));
}

bool WebDatabase::AddToCountOfFormElement(int64 pair_id, int delta) {
  int count = 0;

  if (!GetCountOfFormElement(pair_id, &count))
    return false;

  if (count + delta == 0) {
    if (!RemoveFormElementForID(pair_id))
      return false;
  } else {
    if (!SetCountOfFormElement(pair_id, count + delta))
      return false;
  }
  return true;
}

bool WebDatabase::RemoveFormElementForID(int64 pair_id) {
  SQLStatement s;
  if (s.prepare(db_,
                "DELETE FROM autofill WHERE pair_id = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_int64(0, pair_id);
  if (s.step() != SQLITE_DONE)
    return false;

  return RemoveFormElementForTimeRange(pair_id, Time(), Time(), NULL);
}

void WebDatabase::MigrateOldVersionsAsNeeded() {
  // Migrate if necessary.
  int current_version = meta_table_.GetVersionNumber();
  switch (current_version) {
    // Versions 1 - 19 are unhandled.  Version numbers greater than
    // kCurrentVersionNumber should have already been weeded out by the caller.
    default:
      // When the version is too old, we just try to continue anyway.  There
      // should not be a released product that makes a database too old for us
      // to handle.
      LOG(WARNING) << "Web database version " << current_version <<
          " is too old to handle.";
      return;

    case 20:
      // Add the autogenerate_keyword column.
      if (sqlite3_exec(db_,
                       "ALTER TABLE keywords ADD COLUMN autogenerate_keyword "
                       "INTEGER DEFAULT 0", NULL, NULL, NULL) != SQLITE_OK) {
        NOTREACHED();
        LOG(WARNING) << "Unable to update web database to version 21.";
        return;
      }
      meta_table_.SetVersionNumber(21);
      meta_table_.SetCompatibleVersionNumber(
          std::min(21, kCompatibleVersionNumber));
      // FALL THROUGH

    case 21:
      if (!ClearAutofillEmptyValueElements()) {
        NOTREACHED() << "Failed to clean-up autofill DB.";
      }
      meta_table_.SetVersionNumber(22);
      // No change in the compatibility version number.

      // FALL THROUGH

    // Add successive versions here.  Each should set the version number and
    // compatible version number as appropriate, then fall through to the next
    // case.

    case kCurrentVersionNumber:
      // No migration needed.
      return;
  }
}
