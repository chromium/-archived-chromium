// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_URL_ELIDER_H__
#define CHROME_COMMON_GFX_URL_ELIDER_H__

#include <windows.h>

#include "base/basictypes.h"
#include "chrome/common/gfx/chrome_font.h"

class GURL;

namespace gfx {

// This function takes a GURL object and elides it. It returns a string
// which composed of parts from subdomain, domain, path, filename and query.
// A "..." is added automatically at the end if the elided string is bigger
// than the available pixel width. For available pixel width = 0, empty
// string is returned. |languages| is a comma separted list of ISO 639
// language codes and is used to determine what characters are understood
// by a user. It should come from |prefs::kAcceptLanguages|.
std::wstring ElideUrl(const GURL& url,
                      const ChromeFont& font,
                      int available_pixel_width,
                      const std::wstring& languages);

std::wstring ElideText(const std::wstring& text,
                       const ChromeFont& font,
                       int available_pixel_width);

} // namespace gfx.

#endif  // #ifndef CHROME_COMMON_GFX_URL_ELIDER_H__

