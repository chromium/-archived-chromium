// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "base/file_path.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

namespace net {

bool FileURLToFilePath(const GURL& url, FilePath* file_path) {
  *file_path = FilePath();
  std::wstring& file_path_str = const_cast<std::wstring&>(file_path->value());
  file_path_str.clear();

  if (!url.is_valid())
    return false;

  std::string path;
  std::string host = url.host();
  if (host.empty()) {
    // URL contains no host, the path is the filename. In this case, the path
    // will probably be preceeded with a slash, as in "/C:/foo.txt", so we
    // trim out that here.
    path = url.path();
    size_t first_non_slash = path.find_first_not_of("/\\");
    if (first_non_slash != std::string::npos && first_non_slash > 0)
      path.erase(0, first_non_slash);
  } else {
    // URL contains a host: this means it's UNC. We keep the preceeding slash
    // on the path.
    path = "\\\\";
    path.append(host);
    path.append(url.path());
  }

  if (path.empty())
    return false;
  std::replace(path.begin(), path.end(), '/', '\\');

  // GURL stores strings as percent-encoded UTF-8, this will undo if possible.
  path = UnescapeURLComponent(path,
      UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);

  if (!IsStringUTF8(path)) {
    // Not UTF-8, assume encoding is native codepage and we're done. We know we
    // are giving the conversion function a nonempty string, and it may fail if
    // the given string is not in the current encoding and give us an empty
    // string back. We detect this and report failure.
    file_path_str = base::SysNativeMBToWide(path);
    return !file_path_str.empty();
  }
  file_path_str.assign(UTF8ToWide(path));

  // We used to try too hard and see if |path| made up entirely of
  // the 1st 256 characters in the Unicode was a zero-extended UTF-16.
  // If so, we converted it to 'Latin-1' and checked if the result was UTF-8.
  // If the check passed, we converted the result to UTF-8.
  // Otherwise, we treated the result as the native OS encoding.
  // However, that led to http://crbug.com/4619 and http://crbug.com/14153
  return true;
}

}  // namespace net
