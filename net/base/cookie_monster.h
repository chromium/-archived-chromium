// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by the letter D and the number 2.

#ifndef NET_BASE_COOKIE_MONSTER_H_
#define NET_BASE_COOKIE_MONSTER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

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

  class CookieOptions {
   public:
    // Default is to exclude httponly, which means:
    // - reading operations will not return httponly cookies.
    // - writing operations will not write httponly cookies.
    CookieOptions() : exclude_httponly_(true) {}
    void set_exclude_httponly() { exclude_httponly_ = true; }
    void set_include_httponly() { exclude_httponly_ = false; }
    bool exclude_httponly() const { return exclude_httponly_; }
   private:
    bool exclude_httponly_;
  };

  CookieMonster();

  // The store passed in should not have had Init() called on it yet. This class
  // will take care of initializing it. The backing store is NOT owned by this
  // class, but it must remain valid for the duration of the cookie monster's
  // existence.
  CookieMonster(PersistentCookieStore* store);

#ifdef UNIT_TEST
  CookieMonster(int last_access_threshold_seconds)
      : initialized_(false),
        store_(NULL),
        last_access_threshold_(
            base::TimeDelta::FromSeconds(last_access_threshold_seconds)) {
  }
#endif

  ~CookieMonster();

  // Parse the string with the cookie time (very forgivingly).
  static base::Time ParseCookieTime(const std::string& time_string);

  // Set a single cookie.  Expects a cookie line, like "a=1; domain=b.com".
  bool SetCookie(const GURL& url, const std::string& cookie_line);
  bool SetCookieWithOptions(const GURL& url,
                            const std::string& cookie_line,
                            const CookieOptions& options);
  // Sets a single cookie with a specific creation date. To set a cookie with
  // a creation date of Now() use SetCookie() instead (it calls this function
  // internally).
  bool SetCookieWithCreationTime(const GURL& url,
                                 const std::string& cookie_line,
                                 const base::Time& creation_time);
  bool SetCookieWithCreationTimeWithOptions(
                                 const GURL& url,
                                 const std::string& cookie_line,
                                 const base::Time& creation_time,
                                 const CookieOptions& options);
  // Set a vector of response cookie values for the same URL.
  void SetCookies(const GURL& url, const std::vector<std::string>& cookies);
  void SetCookiesWithOptions(const GURL& url,
                             const std::vector<std::string>& cookies,
                             const CookieOptions& options);

  // TODO what if the total size of all the cookies >4k, can we have a header
  // that big or do we need multiple Cookie: headers?
  // Simple interface, get a cookie string "a=b; c=d" for the given URL.
  // It will _not_ return httponly cookies, see CookieOptions.
  std::string GetCookies(const GURL& url);
  std::string GetCookiesWithOptions(const GURL& url,
                                    const CookieOptions& options);
  // Returns all the cookies, for use in management UI, etc.  This does not mark
  // the cookies as having been accessed.
  CookieList GetAllCookies();

  // Delete all of the cookies.
  int DeleteAll(bool sync_to_store);
  // Delete all of the cookies that have a creation_date greater than or equal
  // to |delete_begin| and less than |delete_end|
  int DeleteAllCreatedBetween(const base::Time& delete_begin,
                              const base::Time& delete_end,
                              bool sync_to_store);
  // Delete all of the cookies that have a creation_date more recent than the
  // one passed into the function via |delete_after|.
  int DeleteAllCreatedAfter(const base::Time& delete_begin, bool sync_to_store);

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
                                   const CookieOptions& options,
                                   std::vector<CanonicalCookie*>* cookies);

  void FindCookiesForKey(const std::string& key,
                         const GURL& url,
                         const CookieOptions& options,
                         const base::Time& current,
                         std::vector<CanonicalCookie*>* cookies);

  // Delete any cookies that are equivalent to |ecc| (same path, key, etc).
  // If |skip_httponly| is true, httponly cookies will not be deleted.  The
  // return value with be true if |skip_httponly| skipped an httponly cookie.
  // NOTE: There should never be more than a single matching equivalent cookie.
  bool DeleteAnyEquivalentCookie(const std::string& key,
                                 const CanonicalCookie& ecc,
                                 bool skip_httponly);

  void InternalInsertCookie(const std::string& key,
                            CanonicalCookie* cc,
                            bool sync_to_store);

  void InternalUpdateCookieAccessTime(CanonicalCookie* cc);

  void InternalDeleteCookie(CookieMap::iterator it, bool sync_to_store);

  // If the number of cookies for host |key|, or globally, are over preset
  // maximums, garbage collects, first for the host and then globally, as
  // described by GarbageCollectRange().  The limits can be found as constants
  // at the top of the function body.
  //
  // Returns the number of cookies deleted (useful for debugging).
  int GarbageCollect(const base::Time& current, const std::string& key);

  // Deletes all expired cookies in |itpair|;
  // then, if the number of remaining cookies is greater than |num_max|,
  // collects the least recently accessed cookies until
  // (|num_max| - |num_purge|) cookies remain.
  //
  // Returns the number of cookies deleted.
  int GarbageCollectRange(const base::Time& current,
                          const CookieMapItPair& itpair,
                          size_t num_max,
                          size_t num_purge);

  // Helper for GarbageCollectRange(); can be called directly as well.  Deletes
  // all expired cookies in |itpair|.  If |cookie_its| is non-NULL, it is
  // populated with all the non-expired cookies from |itpair|.
  //
  // Returns the number of cookies deleted.
  int GarbageCollectExpired(const base::Time& current,
                            const CookieMapItPair& itpair,
                            std::vector<CookieMap::iterator>* cookie_its);

  CookieMap cookies_;

  // Indicates whether the cookie store has been initialized. This happens
  // lazily in InitStoreIfNecessary().
  bool initialized_;

  PersistentCookieStore* store_;

  // The resolution of our time isn't enough, so we do something
  // ugly and increment when we've seen the same time twice.
  base::Time CurrentTime();
  base::Time last_time_seen_;

  // Minimum delay after updating a cookie's LastAccessDate before we will
  // update it again.
  const base::TimeDelta last_access_threshold_;

  // Lock for thread-safety
  Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(CookieMonster);
};

class CookieMonster::ParsedCookie {
 public:
  typedef std::pair<std::string, std::string> TokenValuePair;
  typedef std::vector<TokenValuePair> PairList;

  // The maximum length of a cookie string we will try to parse
  static const size_t kMaxCookieSize = 4096;
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

  // Return the number of attributes, for example, returning 2 for:
  //   "BLAH=hah; path=/; domain=.google.com"
  size_t NumberOfAttributes() const { return pairs_.size() - 1; }

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

  DISALLOW_COPY_AND_ASSIGN(ParsedCookie);
};


class CookieMonster::CanonicalCookie {
 public:
  CanonicalCookie(const std::string& name,
                  const std::string& value,
                  const std::string& path,
                  bool secure,
                  bool httponly,
                  const base::Time& creation,
                  const base::Time& last_access,
                  bool has_expires,
                  const base::Time& expires)
      : name_(name),
        value_(value),
        path_(path),
        creation_date_(creation),
        last_access_date_(last_access),
        expiry_date_(expires),
        has_expires_(has_expires),
        secure_(secure),
        httponly_(httponly) {
  }

#if defined(_MSC_VER) && _CPPLIB_VER == 505
  // On Visual Studio 2008 Service Pack 1, std::vector<> do an early
  // optimization in a way that requires the availability of a default
  // constructor. It is because it sees std::pair<> as "swappable", so creates a
  // dummy to swap with, which requires an empty constructor for any entry in
  // the std::pair.
  CanonicalCookie() { }
#endif

  // Supports the default copy constructor.

  const std::string& Name() const { return name_; }
  const std::string& Value() const { return value_; }
  const std::string& Path() const { return path_; }
  const base::Time& CreationDate() const { return creation_date_; }
  const base::Time& LastAccessDate() const { return last_access_date_; }
  bool DoesExpire() const { return has_expires_; }
  bool IsPersistent() const { return DoesExpire(); }
  const base::Time& ExpiryDate() const { return expiry_date_; }
  bool IsSecure() const { return secure_; }
  bool IsHttpOnly() const { return httponly_; }

  bool IsExpired(const base::Time& current) {
    return has_expires_ && current >= expiry_date_;
  }

  // Are the cookies considered equivalent in the eyes of the RFC.
  // This says that the domain and path should string match identically.
  bool IsEquivalent(const CanonicalCookie& ecc) const {
    // It seems like it would make sense to take secure and httponly into
    // account, but the RFC doesn't specify this.
    return name_ == ecc.Name() && path_ == ecc.Path();
  }

  void SetLastAccessDate(const base::Time& date) {
    last_access_date_ = date;
  }

  bool IsOnPath(const std::string& url_path) const;

  std::string DebugString() const;
 private:
  std::string name_;
  std::string value_;
  std::string path_;
  base::Time creation_date_;
  base::Time last_access_date_;
  base::Time expiry_date_;
  bool has_expires_;
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
  virtual void UpdateCookieAccessTime(const CanonicalCookie&) = 0;
  virtual void DeleteCookie(const CanonicalCookie&) = 0;

 protected:
  PersistentCookieStore() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentCookieStore);
};

}  // namespace net

#endif  // NET_BASE_COOKIE_MONSTER_H_
