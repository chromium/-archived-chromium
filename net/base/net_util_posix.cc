// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace net {

bool FileURLToFilePath(const GURL& url, FilePath* path) {
  *path = FilePath();
  std::string& file_path_str = const_cast<std::string&>(path->value());
  file_path_str.clear();

  if (!url.is_valid())
    return false;

  // Firefox seems to ignore the "host" of a file url if there is one. That is,
  // file://foo/bar.txt maps to /bar.txt.
  std::string old_path = url.path();

  if (old_path.empty())
    return false;

  // GURL stores strings as percent-encoded 8-bit, this will undo if possible.
  old_path = UnescapeURLComponent(old_path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  // Collapse multiple path slashes into a single path slash.
  std::string new_path;
  do {
    new_path = old_path;
    ReplaceSubstringsAfterOffset(&new_path, 0, "//", "/");
    old_path.swap(new_path);
  } while (new_path != old_path);

  file_path_str.assign(old_path);

  return !file_path_str.empty();
}

}  // namespace net
