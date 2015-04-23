// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_SANDBOX_SUPPORT_H_
#define CHROME_RENDERER_RENDERER_SANDBOX_SUPPORT_H_

#include <stdint.h>

#include <string>

namespace renderer_sandbox_support {

// Return a font family which provides glyphs for the Unicode code points
// specified by |utf16|
//   utf16: a native-endian UTF16 string
//   num_utf16: the number of 16-bit words in |utf16|
//
// Returns: the font family or an empty string if the request could not be
// satisfied.
std::string getFontFamilyForCharacters(const uint16_t* utf16, size_t num_utf16);

};  // namespace render_sandbox_support

#endif  // CHROME_RENDERER_RENDER_SANDBOX_SUPPORT_H_
