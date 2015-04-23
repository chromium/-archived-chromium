// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist.h"

#include <algorithm>
#include <string>

namespace {

bool matches(std::string pattern, std::string url) {
  return url.find(pattern) != std::string::npos;
}

}

// Value is not important, here just that the object has an address.
const void* const Blacklist::kRequestDataKey = 0;

bool Blacklist::Entry::MatchType(const std::string& type) const {
  return std::find(types_->begin(), types_->end(), type) != types_->end();
}

bool Blacklist::Entry::IsBlocked(const GURL& url) const {
  return (attributes_ & kBlockAll) ||
    ((attributes_ & kBlockUnsecure) && !url.SchemeIsSecure());
}

Blacklist::Entry::Entry(const std::string& pattern, unsigned int attributes)
    : pattern_(pattern), attributes_(attributes) {}

void Blacklist::Entry::AddType(const std::string& type) {
  types_->push_back(type);
}

Blacklist::Blacklist(const FilePath& file) {
  // TODO(idanan): Do something here.
}

Blacklist::~Blacklist() {
  for (std::vector<Entry*>::iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i)
    delete *i;
}

// Returns a pointer to the Blacklist-owned entry which matches the given
// URL. If no matching Entry is found, returns null.
const Blacklist::Entry* Blacklist::findMatch(const GURL& url) const {
  for (std::vector<Entry*>::const_iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i)
    if (matches((*i)->pattern(), url.spec()))
      return *i;
  return 0;
}

std::string Blacklist::StripCookies(const std::string& header) {
  // TODO(idanan): Implement this.
  return header;
}

std::string Blacklist::StripCookieExpiry(const std::string& cookie) {
  std::string::size_type start = cookie.find("; expires=");
  if (start != std::string::npos) {
    std::string::size_type finish = cookie.find(";", start+1);
    std::string session_cookie(cookie, 0, start);
    if (finish != std::string::npos)
      session_cookie.append(cookie.substr(finish));
    return session_cookie;
  }
  return cookie;
}
