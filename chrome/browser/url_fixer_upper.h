// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CHROME_BROWSER_URL_FIXER_UPPER_H__
#define CHROME_BROWSER_URL_FIXER_UPPER_H__

#include <string>

#include "googleurl/src/url_parse.h"

// This object is designed to convert various types of input into URLs that we
// know are valid. For example, user typing in the URL bar or command line
// options. This is NOT the place for converting between different types of
// URLs or parsing them, see net_util.h for that.
namespace URLFixerUpper {

  // Segments the given text string into parts of a URL.  This is most useful
  // for schemes such as http, https, and ftp where |SegmentURL| will find many
  // segments.  Currently does not segment "file" schemes.
  std::wstring SegmentURL(const std::wstring& text, url_parse::Parsed* parts);

  // Converts |text| to a fixed-up URL and returns it. Attempts to make
  // some "smart" adjustments to obviously-invalid input where possible.
  // |text| may be an absolute path to a file, which will get converted to a
  // "file:" URL.
  //
  // The result will be a "more" valid URL than the input. It may still not
  // be valid, convert to a GURL for that.
  //
  // If |desired_tld| is non-empty, it represents the TLD the user wishes to
  // append in the case of an incomplete domain.  We check that this is not a
  // file path and there does not appear to be a valid TLD already, then append
  // |desired_tld| to the domain and prepend "www." (unless it, or a scheme,
  // are already present.)  This TLD should not have a leading '.' (use "com"
  // instead of ".com").
  std::wstring FixupURL(const std::wstring& text,
                        const std::wstring& desired_tld);

  // Converts |text| to a fixed-up URL, allowing it to be a relative path on
  // the local filesystem.  Begin searching in |base_dir|; if empty, use the
  // current working directory.  If this resolves to a file on disk, convert it
  // to a "file:" URL in |fixed_up_url|; otherwise, fall back to the behavior
  // of FixupURL().
  //
  // For "regular" input, even if it is possibly a file with a full path, you
  // should use FixupURL() directly.  This function should only be used when
  // relative path handling is desired, as for command line processing.
  std::wstring FixupRelativeFile(const std::wstring& base_dir,
                                 const std::wstring& text);
};

#endif  // #ifndef CHROME_BROWSER_URL_FIXER_UPPER_H__
