// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace net {

bool FileURLToFilePath(const GURL& url, std::wstring* file_path) {
  file_path->clear();

  if (!url.is_valid())
    return false;

  // Firefox seems to ignore the "host" of a file url if there is one. That is,
  // file://foo/bar.txt maps to /bar.txt.
  std::string path = url.path();

  if (path.empty())
    return false;

  // GURL stores strings as percent-encoded 8-bit, this will undo if possible.
  path = UnescapeURLComponent(path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  // Collapse multiple path slashes into a single path slash.
  std::string new_path;
  do {
    new_path = path;
    ReplaceSubstringsAfterOffset(&new_path, 0, "//", "/");
    path.swap(new_path);
  } while (new_path != path);

  // TODO(tc): This should actually be 8-bit to wide.  We may lose data if the
  // string isn't UTF-8.
  file_path->assign(UTF8ToWide(path));

  return !file_path->empty();
}

}  // namespace net

