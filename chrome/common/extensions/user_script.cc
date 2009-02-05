// Copyright 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/user_script.h"

#include "base/string_util.h"

bool UserScript::MatchesUrl(const GURL& url) {
  for (std::vector<std::string>::iterator glob = globs_.begin();
       glob != globs_.end(); ++glob) {
    if (MatchPattern(url.spec(), *glob))
      return true;
  }

  for (std::vector<URLPattern>::iterator pattern = url_patterns_.begin();
       pattern != url_patterns_.end(); ++pattern) {
    if (pattern->MatchesUrl(url))
      return true;
  }

  return false;
}

void UserScript::Pickle(::Pickle* pickle) {
  pickle->WriteString(url_.spec());

  // Don't write path as we don't need that in the renderer.

  pickle->WriteSize(globs_.size());
  for (std::vector<std::string>::iterator glob = globs_.begin();
       glob != globs_.end(); ++glob) {
    pickle->WriteString(*glob);
  }

  pickle->WriteSize(url_patterns_.size());
  for (std::vector<URLPattern>::iterator pattern = url_patterns_.begin();
       pattern != url_patterns_.end(); ++pattern) {
    pickle->WriteString(pattern->GetAsString());
  }
}

void UserScript::Unpickle(const ::Pickle& pickle, void** iter) {
  std::string url_spec;
  CHECK(pickle.ReadString(iter, &url_spec));
  url_ = GURL(url_spec);

  size_t num_globs = 0;
  CHECK(pickle.ReadSize(iter, &num_globs));

  globs_.clear();
  for (size_t i = 0; i < num_globs; ++i) {
    std::string glob;
    CHECK(pickle.ReadString(iter, &glob));
    globs_.push_back(glob);
  }

  size_t num_patterns = 0;
  CHECK(pickle.ReadSize(iter, &num_patterns));

  url_patterns_.clear();
  for (size_t i = 0; i < num_patterns; ++i) {
    std::string pattern_str;
    URLPattern pattern;
    CHECK(pickle.ReadString(iter, &pattern_str));
    CHECK(pattern.Parse(pattern_str));
    url_patterns_.push_back(pattern);
  }
}
