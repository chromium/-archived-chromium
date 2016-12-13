// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_cache.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace {

// Helper to find the containing directory of path. In RFC 2617 this is what
// they call the "last symbolic element in the absolute path".
// Examples:
//   "/foo/bar.txt" --> "/foo/"
//   "/foo/" --> "/foo/"
std::string GetParentDirectory(const std::string& path) {
  std::string::size_type last_slash = path.rfind("/");
  if (last_slash == std::string::npos) {
    // No slash (absolute paths always start with slash, so this must be
    // the proxy case which uses empty string).
    DCHECK(path.empty());
    return path;
  }
  return path.substr(0, last_slash + 1);
}

// Debug helper to check that |path| arguments are properly formed.
// (should be absolute path, or empty string).
void CheckPathIsValid(const std::string& path) {
  DCHECK(path.empty() || path[0] == '/');
}

// Return true if |path| is a subpath of |container|. In other words, is
// |container| an ancestor of |path|?
bool IsEnclosingPath(const std::string& container, const std::string& path) {
  DCHECK(container.empty() || *(container.end() - 1) == '/');
  return (container.empty() && path.empty()) ||
         (!container.empty() && StartsWithASCII(path, container, true));
}

// Debug helper to check that |origin| arguments are properly formed.
void CheckOriginIsValid(const GURL& origin) {
  DCHECK(origin.is_valid());
  DCHECK(origin.SchemeIs("http") || origin.SchemeIs("https"));
  DCHECK(origin.GetOrigin() == origin);
}

// Functor used by remove_if.
struct IsEnclosedBy {
  IsEnclosedBy(const std::string& path) : path(path) { }
  bool operator() (const std::string& x) {
    return IsEnclosingPath(path, x);
  }
  const std::string& path;
};

} // namespace

namespace net {

// Performance: O(n), where n is the number of realm entries.
HttpAuthCache::Entry* HttpAuthCache::LookupByRealm(const GURL& origin,
                                                   const std::string& realm) {
  CheckOriginIsValid(origin);

  // Linear scan through the realm entries.
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin() == origin && it->realm() == realm)
      return &(*it);
  }
  return NULL; // No realm entry found.
}

// Performance: O(n*m), where n is the number of realm entries, m is the number
// of path entries per realm. Both n amd m are expected to be small; m is
// kept small because AddPath() only keeps the shallowest entry.
HttpAuthCache::Entry* HttpAuthCache::LookupByPath(const GURL& origin,
                                                  const std::string& path) {
  CheckOriginIsValid(origin);
  CheckPathIsValid(path);

  // RFC 2617 section 2:
  // A client SHOULD assume that all paths at or deeper than the depth of
  // the last symbolic element in the path field of the Request-URI also are
  // within the protection space ...
  std::string parent_dir = GetParentDirectory(path);

  // Linear scan through the realm entries.
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin() == origin && it->HasEnclosingPath(parent_dir))
      return &(*it);
  }
  return NULL; // No entry found.
}

HttpAuthCache::Entry* HttpAuthCache::Add(const GURL& origin,
                                         HttpAuthHandler* handler,
                                         const std::wstring& username,
                                         const std::wstring& password,
                                         const std::string& path) {
  CheckOriginIsValid(origin);
  CheckPathIsValid(path);

  // Check for existing entry (we will re-use it if present).
  HttpAuthCache::Entry* entry = LookupByRealm(origin, handler->realm());

  if (!entry) {
    // Failsafe to prevent unbounded memory growth of the cache.
    if (entries_.size() >= kMaxNumRealmEntries) {
      LOG(WARNING) << "Num auth cache entries reached limit -- evicting";
      entries_.pop_back();
    }

    entries_.push_front(Entry());
    entry = &entries_.front();
    entry->origin_ = origin;
  }

  entry->username_ = username;
  entry->password_ = password;
  entry->handler_ = handler;
  entry->AddPath(path);

  return entry;
}

void HttpAuthCache::Entry::AddPath(const std::string& path) {
  std::string parent_dir = GetParentDirectory(path);
  if (!HasEnclosingPath(parent_dir)) {
    // Remove any entries that have been subsumed by the new entry.
    paths_.remove_if(IsEnclosedBy(parent_dir));

    // Failsafe to prevent unbounded memory growth of the cache.
    if (paths_.size() >= kMaxNumPathsPerRealmEntry) {
      LOG(WARNING) << "Num path entries for " << origin()
                   << " has grown too large -- evicting";
      paths_.pop_back();
    }

    // Add new path.
    paths_.push_front(parent_dir);
  }
}

bool HttpAuthCache::Entry::HasEnclosingPath(const std::string& dir) {
  DCHECK(GetParentDirectory(dir) == dir);
  for (PathList::const_iterator it = paths_.begin(); it != paths_.end();
       ++it) {
    if (IsEnclosingPath(*it, dir))
      return true;
  }
  return false;
}

bool HttpAuthCache::Remove(const GURL& origin,
                           const std::string& realm,
                           const std::wstring& username,
                           const std::wstring& password) {
  for (EntryList::iterator it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin() == origin && it->realm() == realm) {
      if (username == it->username() && password == it->password()) {
        entries_.erase(it);
        return true;
      }
      return false;
    }
  }
  return false;
}

} // namespace net
