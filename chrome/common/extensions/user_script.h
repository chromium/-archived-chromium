// Copyright 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_
#define CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_

#include <vector>
#include <string>

#include "base/file_path.h"
#include "base/pickle.h"
#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

// Represents a user script, either a standalone one, or one that is part of an
// extension.
class UserScript {
 public:
  UserScript(){}

  // The URL to retrieve the content of this script at.
  const GURL& url() const { return url_; }
  void set_url(const GURL& url) { url_ = url; }

  // The path to find the script at.
  const FilePath& path() const { return path_; }
  void set_path(const FilePath& path) { path_ = path; }

  // The globs, if any, that determine which pages this script runs against.
  // These are only used with "standalone" Greasemonkey-like user scripts.
  const std::vector<std::string>& globs() const { return globs_; }
  void add_glob(const std::string& glob) { globs_.push_back(glob); }
  void clear_globs() { globs_.clear(); }

  // The URLPatterns, if any, that determine which pages this script runs
  // against.
  const std::vector<URLPattern>& url_patterns() const { return url_patterns_; }
  void add_url_pattern(const URLPattern& pattern) {
    url_patterns_.push_back(pattern);
  }
  void clear_url_patterns() { url_patterns_.clear(); }

  // Returns true if the script should be applied to the specified URL, false
  // otherwise.
  bool MatchesUrl(const GURL& url);

  // Serialize the script into a pickle.
  void Pickle(::Pickle* pickle);

  // Deserialize the script from a pickle. Note that this always succeeds
  // because presumably we were the one that pickled it, and we did it
  // correctly.
  void Unpickle(const ::Pickle& pickle, void** iter);

 private:
  // The URL to the content of the script.
  GURL url_;

  // The path to the content of the script.
  FilePath path_;

  // Greasemonkey-style globs that determine pages to inject the script into.
  // These are only used with standalone scripts.
  std::vector<std::string> globs_;

  // URLPatterns that determine pages to inject the script into. These are
  // only used with scripts that are part of extensions.
  std::vector<URLPattern> url_patterns_;
};

typedef std::vector<UserScript> UserScriptList;

#endif  // CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_
