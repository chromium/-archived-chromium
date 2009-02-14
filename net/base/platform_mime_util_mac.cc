// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreServices/CoreServices.h>
#include <string>

#include "base/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "net/base/platform_mime_util.h"

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* result) const {
  std::string ext_nodot = ext;
  if (ext_nodot.length() >= 1 && ext_nodot[0] == L'.')
    ext_nodot.erase(ext_nodot.begin());
  scoped_cftyperef<CFStringRef> ext_ref(base::SysUTF8ToCFStringRef(ext_nodot));
  if (!ext_ref)
    return false;
  scoped_cftyperef<CFStringRef> uti(
      UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                            ext_ref,
                                            NULL));
  if (!uti)
    return false;
  scoped_cftyperef<CFStringRef> mime_ref(
      UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType));
  if (!mime_ref)
    return false;

  *result = base::SysCFStringRefToUTF8(mime_ref);
  return true;
}

bool PlatformMimeUtil::GetPreferredExtensionForMimeType(
    const std::string& mime_type, FilePath::StringType* ext) const {
  scoped_cftyperef<CFStringRef> mime_ref(base::SysUTF8ToCFStringRef(mime_type));
  if (!mime_ref)
    return false;
  scoped_cftyperef<CFStringRef> uti(
      UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType,
                                            mime_ref,
                                            NULL));
  if (!uti)
    return false;
  scoped_cftyperef<CFStringRef> ext_ref(
      UTTypeCopyPreferredTagWithClass(uti, kUTTagClassFilenameExtension));
  if (!ext_ref)
    return false;

  ext_ref.reset(CFStringCreateWithFormat(kCFAllocatorDefault,
                                         NULL,
                                         CFSTR(".%@"),
                                         ext_ref.get()));

  *ext = base::SysCFStringRefToUTF8(ext_ref);
  return true;
}

}  // namespace net
