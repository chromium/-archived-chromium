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

// A sqlite implementation of a cookie monster persistent store.

#ifndef CHROME_COMMON_NET_COOKIE_MONSTER_SQLITE_H__
#define CHROME_COMMON_NET_COOKIE_MONSTER_SQLITE_H__

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/meta_table_helper.h"
#include "net/base/cookie_monster.h"

struct sqlite3;

class SQLitePersistentCookieStore
    : public CookieMonster::PersistentCookieStore {
 public:
  SQLitePersistentCookieStore(const std::wstring& path,
                              MessageLoop* background_loop);
  ~SQLitePersistentCookieStore();

  virtual bool Load(std::vector<CookieMonster::KeyedCanonicalCookie>*);

  virtual void AddCookie(const std::string&,
                         const CookieMonster::CanonicalCookie&);
  virtual void DeleteCookie(const CookieMonster::CanonicalCookie&);

 private:
  class Backend;

  // Database upgrade statements.
  bool EnsureDatabaseVersion(sqlite3* db);
  bool UpdateSchemaToVersion2(sqlite3* db);

  std::wstring path_;
  scoped_refptr<Backend> backend_;

  // Background MessageLoop on which to access the backend_;
  MessageLoop* background_loop_;

  MetaTableHelper meta_table_;

  DISALLOW_EVIL_CONSTRUCTORS(SQLitePersistentCookieStore);
};

#endif  // CHROME_COMMON_NET_COOKIE_MONSTER_SQLITE_H__
