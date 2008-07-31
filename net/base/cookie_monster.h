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

// Brought to you by the letter D and the number 2.

#ifndef NET_BASE_COOKIE_MONSTER_H__
#define NET_BASE_COOKIE_MONSTER_H__

#include <string>
#include <vector>
#include <utility>
#include <map>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/time.h"

class GURL;

namespace net {

// The cookie monster is the system for storing and retrieving cookies. It has
// an in-memory list of all cookies, and synchronizes non-session cookies to an
// optional permanent storage that implements the PersistentCookieStore
// interface.
//
// This class IS thread-safe. Normally, it is only used on the I/O thread, but
// is also accessed directly through Automation for UI testing.
//
// TODO(deanm) Implement CookieMonster, the cookie database.
//  - Verify that our domain enforcement and non-dotted handling is correct
//  - Currently garbage collection is done on oldest CreationUTC, Mozilla
//    purges cookies on last access time, which would require adding and
//    keeping track of access times on a CanonicalCookie
class CookieMonster {
 public:
  class ParsedCookie;
  class CanonicalCookie;
  class PersistentCookieStore;

  // NOTE(deanm):
  // I benchmarked hash_multimap vs multimap.  We're going to be query-heavy
  // so it would seem like hashing would help.  However they were very
  // close, with multimap being a tiny bit faster.  I think this is because
  // our map is at max around 1000 entries, and the additional complexity
  // for the hashing might not overcome the O(log(1000)) for querying
  // a multimap.  Also, multimap is standard, another reason to use it.
  typedef std::multimap<std::string, CanonicalCookie*> CookieMap;
  typedef std::pair<CookieMap::iterator, CookieMap::iterator> CookieMapItPair;
  typedef std::pair<std::string, CanonicalCookie*> KeyedCanonicalCookie;
  typedef std::pair<std::string, CanonicalCookie> CookieListPair;
  typedef std::vector<CookieListPair> CookieList;

  enum CookieOptions {
    // Normal cookie behavior, decides which cookies to return based on
    // the URL and whether it's https, etc.  Never returns HttpOnly cookies
    NORMAL = 0,
    // Include HttpOnly cookies
    INCLUDE_HTTPONLY = 1,
  };

  CookieMonster();

  // The store passed in should not have had Init() called on it yet. This class
  // will take care of initializing it. The backing store is NOT owned by this
  // class, but it must remain valid for the duration of the cookie monster's
  // existence.
  CookieMonster(PersistentCookieStore* store);

  ~CookieMonster();

  // Parse the string with the cookie time (very forgivingly).
  static Time ParseCookieTime(const std::string& time_string);

  // Set a single cookie.  Expects a cookie line, like "a=1; domain=b.com".
  bool SetCookie(const GURL& url, const std::string& cookie_line);
  // Sets a single cookie with a specific creation date. To set a cookie with
  // a creation date of Now() use SetCookie() instead (it calls this function
  // internally).
  bool SetCookieWithCreationTime(const GURL& url,
                                 const std::string& cookie_line,
                                 const Time& creation_time);
  // Set a vector of response cookie values for the same URL.
  void SetCookies(const GURL& url, const std::vector<std::string>& cookies);

  // TODO what if the total size of all the cookies >4k, can we have a header
  // that big or do we need multiple Cookie: headers?
  // Simple interface, get a cookie string "a=b; c=d" for the given URL.
  // It will _not_ return httponly cookies, see GetCookiesWithOptions
  std::string GetCookies(const GURL& url);
  std::string GetCookiesWithOptions(const GURL& url, CookieOptions options);
  // Returns all the cookies, for use in management UI, etc.
  CookieList GetAllCookies();

  // Delete all of the cookies.
  int DeleteAll(bool sync_to_store);
  // Delete all of the cookies that have a creation_date greater than or equal
  // to |delete_begin| and less than |delete_end|
  int DeleteAllCreatedBetween(const Time& delete_begin,
                              const Time& delete_end,
                              bool sync_to_store);
  // Delete all of the cookies that have a creation_date more recent than the
  // one passed into the function via |delete_after|.
  int DeleteAllCreatedAfter(const Time& delete_begin, bool sync_to_store);

  // Delete one specific cookie.
  bool DeleteCookie(const std::string& domain,
                    const CanonicalCookie& cookie,
                    bool sync_to_store);

  // There are some unknowns about how to correctly handle file:// cookies,
  // and our implementation for this is not robust enough. This allows you
  // to enable support, but it should only be used for testing. Bug 1157243.
  static void EnableFileScheme();
  static bool enable_file_scheme_;

 private:
  // Called by all non-static functions to ensure that the cookies store has
  // been initialized. This is not done during creating so it doesn't block
  // the window showing.
  // Note: this method should always be called with lock_ held.
  void InitIfNecessary() {
    if (!initialized_) {
      if (store_)
        InitStore();
      initialized_ = true;
    }
  }

  // Initializes the backing store and reads existing cookies from it.
  // Should only be called by InitIfNecessary().
  void InitStore();

  void FindCookiesForHostAndDomain(const GURL& url,
                                   CookieOptions options,
                                   std::vector<CanonicalCookie*>* cookies);

  void FindCookiesForKey(const std::string& key,
                         const GURL& url,
                         CookieOptions options,
                         const Time& current,
                         std::vector<CanonicalCookie*>* cookies);

  int DeleteEquivalentCookies(const std::string& key,
                              const CanonicalCookie& ecc);

  void InternalInsertCookie(const std::string& key,
                            CanonicalCookie* cc,
                            bool sync_to_store);

  void InternalDeleteCookie(CookieMap::iterator it, bool sync_to_store);

  // Enforce cookie maximum limits, purging expired and old cookies if needed
  int GarbageCollect(const Time& current, const std::string& key);
  int GarbageCollectRange(const Time& current,
                          const CookieMapItPair& itpair,
                          size_t num_max,
                          size_t num_purge);

  CookieMap cookies_;

  // Indicates whether the cookie store has been initialized. This happens
  // lazily in InitStoreIfNecessary().
  bool initialized_;

  PersistentCookieStore* store_;

  // The resolution of our time isn't enough, so we do something
  // ugly and increment when we've seen the same time twice.
  Time CurrentTime();
  Time last_time_seen_;

  // Lock for thread-safety
  Lock lock_;

  DISALLOW_EVIL_CONSTRUCTORS(CookieMonster);
};

class CookieMonster::ParsedCookie {
 public:
  typedef std::pair<std::string, std::string> TokenValuePair;
  typedef std::vector<TokenValuePair> PairList;

  // The maximum length of a cookie string we will try to parse
  static const int kMaxCookieSize = 4096;
  // The maximum number of Token/Value pairs.  Shouldn't have more than 8.
  static const int kMaxPairs = 16;

  // Construct from a cookie string like "BLAH=1; path=/; domain=.google.com"
  ParsedCookie(const std::string& cookie_line);
  ~ParsedCookie() { }

  // You should not call any other methods on the class if !IsValid
  bool IsValid() const { return is_valid_; }

  const std::string& Name() const { return pairs_[0].first; }
  const std::string& Token() const { return Name(); }
  const std::string& Value() const { return pairs_[0].second; }

  bool HasPath() const { return path_index_ != 0; }
  const std::string& Path() const { return pairs_[path_index_].second; }
  bool HasDomain() const { return domain_index_ != 0; }
  const std::string& Domain() const { return pairs_[domain_index_].second; }
  bool HasExpires() const { return expires_index_ != 0; }
  const std::string& Expires() const { return pairs_[expires_index_].second; }
  bool HasMaxAge() const { return maxage_index_ != 0; }
  const std::string& MaxAge() const { return pairs_[maxage_index_].second; }
  bool IsSecure() const { return secure_index_ != 0; }
  bool IsHttpOnly() const { return httponly_index_ != 0; }

  // For debugging only!
  std::string DebugString() const;

 private:
  void ParseTokenValuePairs(const std::string& cookie_line);
  void SetupAttributes();

  PairList pairs_;
  bool is_valid_;
  // These will default to 0, but that should never be valid since the
  // 0th index is the user supplied token/value, not an attribute.
  // We're really never going to have more than like 8 attributes, so we
  // could fit these into 3 bits each if we're worried about size...
  size_t path_index_;
  size_t domain_index_;
  size_t expires_index_;
  size_t maxage_index_;
  size_t secure_index_;
  size_t httponly_index_;

  DISALLOW_EVIL_CONSTRUCTORS(CookieMonster::ParsedCookie);
};


class CookieMonster::CanonicalCookie {
 public:
  CanonicalCookie(const std::string& name, const std::string& value,
                  const std::string& path, bool secure,
                  bool httponly, const Time& creation,
                  bool has_expires, const Time& expires)
      : name_(name),
        value_(value),
        path_(path),
        secure_(secure),
        httponly_(httponly),
        creation_date_(creation),
        has_expires_(has_expires),
        expiry_date_(expires) {
  }

  // Supports the default copy constructor.

  const std::string& Name() const { return name_; }
  const std::string& Value() const { return value_; }
  const std::string& Path() const { return path_; }
  const Time& CreationDate() const { return creation_date_; }
  bool DoesExpire() const { return has_expires_; }
  bool IsPersistent() const { return DoesExpire(); }
  const Time& ExpiryDate() const { return expiry_date_; }
  bool IsSecure() const { return secure_; }
  bool IsHttpOnly() const { return httponly_; }

  bool IsExpired(const Time& current) {
    return has_expires_ && current >= expiry_date_;
  }

  // Are the cookies considered equivalent in the eyes of the RFC.
  // This says that the domain and path should string match identically.
  bool IsEquivalent(const CanonicalCookie& ecc) const {
    // It seems like it would make sense to take secure and httponly into
    // account, but the RFC doesn't specify this.
    return name_ == ecc.Name() && path_ == ecc.Path();
  }

  bool IsOnPath(const std::string& url_path) const;

  std::string DebugString() const;
 private:
  std::string name_;
  std::string value_;
  std::string path_;
  Time creation_date_;
  bool has_expires_;
  Time expiry_date_;
  bool secure_;
  bool httponly_;
};

class CookieMonster::PersistentCookieStore {
 public:
  virtual ~PersistentCookieStore() { }

  // Initializes the store and retrieves the existing cookies. This will be
  // called only once at startup.
  virtual bool Load(std::vector<CookieMonster::KeyedCanonicalCookie>*) = 0;

  virtual void AddCookie(const std::string&, const CanonicalCookie&) = 0;
  virtual void DeleteCookie(const CanonicalCookie&) = 0;

 protected:
  PersistentCookieStore() { }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(CookieMonster::PersistentCookieStore);
};

}  // namespace net

#endif  // NET_BASE_COOKIE_MONSTER_H__
