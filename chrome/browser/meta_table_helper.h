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

#ifndef CHROME_BROWSER_META_TABLE_HELPER_H__
#define CHROME_BROWSER_META_TABLE_HELPER_H__

#include <string>

#include "base/basictypes.h"

struct sqlite3;
class SQLStatement;

// MetaTableHelper maintains arbitrary key/value pairs in a table, as well
// as version information. MetaTableHelper is used by both WebDatabase and
// HistoryDatabase to maintain version information.
//
// To use a MetalTableHelper you must invoke the init method specifying the
// database to use.
class MetaTableHelper {
 public:
  // Creates a new MetaTableHelper. After construction you must invoke
  // Init with the appropriate database.
  MetaTableHelper();
  ~MetaTableHelper();

  // Initializes the MetaTableHelper, creating the meta table if necessary. For
  // new tables, it will initialize the version number to |version| and will
  // set the optional |is_new| out var to true.
  //
  // The name of the database in the sqlite connection (for tables named with
  // the "db_name.table_name" scheme is given in |db_name|. If empty, it is
  // assumed there is no database name.
  bool Init(const std::string& db_name, int version, sqlite3* db);

  // Version number. This should be the version number of the creator of the
  // file. GetVersionNumber will return 0 if there is no version number.
  // See also *CompatibleVersionNumber.
  void SetVersionNumber(int version);
  int GetVersionNumber();

  // The compatible version number is the lowest version that this file format
  // is readable by. If an addition or other non-critical change is made to the
  // file in such a way that it could be read or written non-catastrophically
  // by an older version, this number tells us which version that is.
  //
  // Any version newer than this should be able to interpret the file. Any
  // version older than this should not touch the file or else it might
  // corrupt it.
  //
  // GetCompatibleVersionNumber will return 0 if there is none.
  void SetCompatibleVersionNumber(int version);
  int GetCompatibleVersionNumber();

  // Arbitrary key/value pair with a wstring value.
  bool SetValue(const std::string& key, const std::wstring& value);
  bool GetValue(const std::string& key, std::wstring* value);

  // Arbitrary key/value pair with an int value.
  bool SetValue(const std::string& key, int value);
  bool GetValue(const std::string& key, int* value);

  // Arbitrary key/value pair with an int64 value.
  bool SetValue(const std::string& key, int64 value);
  bool GetValue(const std::string& key, int64* value);

 private:
  // Conveniences to prepare the two types of statements used by
  // MetaTableHelper.
  bool PrepareSetStatement(SQLStatement* statement, const std::string& key);
  bool PrepareGetStatement(SQLStatement* statement, const std::string& key);

  sqlite3* db_;

  // Name of the database within the connection, if there is one. When empty,
  // there is no special database name.
  std::string db_name_;

  DISALLOW_EVIL_CONSTRUCTORS(MetaTableHelper);
};

#endif  // CHROME_BROWSER_META_TABLE_HELPER_H__
