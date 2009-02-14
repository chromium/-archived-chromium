// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/base/platform_mime_util.h"

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* result) const {
  // The correct thing to do is to interact somehow with the freedesktop shared
  // mime info cache. Since Linux (and other traditional *IX systems) don't use
  // file extensions; they use mime types and have multiple ways of detecting
  // that; some types can be guessed from globs (*.gif), but there's a whole
  // bunch that use a magic byte test.
  //
  // Since this method only is called from inside mime_util where there is
  // already a hard coded table of mime types, we just return false because it
  // doesn't matter.

  return false;
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

  return false;
}

}  // namespace net
