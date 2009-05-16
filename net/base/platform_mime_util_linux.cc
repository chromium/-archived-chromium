// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/mime_util.h"
#include "net/base/platform_mime_util.h"

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* result) const {
  FilePath::StringType dummy_path = "foo." + ext;
  std::string out = mime_util::GetFileMimeType(dummy_path);
  // GetFileMimeType likes to return application/octet-stream
  // for everything it doesn't know - ignore that.
  if (out == "application/octet-stream" || !out.length())
    return false;
  *result = out;
  return true;
}

bool PlatformMimeUtil::GetPreferredExtensionForMimeType(
    const std::string& mime_type, FilePath::StringType* ext) const {
  // Unlike GetPlatformMimeTypeFromExtension, this method doesn't have a
  // default list that it uses, but for now we are also returning false since
  // this doesn't really matter as much under Linux.
  //
  // If we wanted to do this properly, we would read the mime.cache file which
  // has a section where they assign a glob (*.gif) to a mimetype
  // (image/gif). We look up the "heaviest" glob for a certain mime type and
  // then then try to chop off "*.".

  NOTIMPLEMENTED();
  return false;
}

}  // namespace net
