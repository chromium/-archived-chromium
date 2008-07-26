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

#ifndef CHROME_BROWSER_TEMPLATE_URL_PARSER_H__
#define CHROME_BROWSER_TEMPLATE_URL_PARSER_H__

#include <string>

#include "base/basictypes.h"

class TemplateURL;

// TemplateURLParser, as the name implies, handling reading of TemplateURLs
// from OpenSearch description documents.
class TemplateURLParser {
 public:
  class ParameterFilter {
   public:
    // Invoked for each parameter of the template URL while parsing.  If this
    // methods returns false, the parameter is not included.
     virtual bool KeepParameter(const std::string& key,
                                const std::string& value) = 0;
  };
  // Decodes the chunk of data representing a TemplateURL. If data does
  // not describe a valid TemplateURL false is returned. Additionally, if the
  // URLs referenced do not point to valid http resources, false is returned.
  // |parameter_filter| can be used if you want to filter out some parameters
  // out of the URL. For example when importing from another browser we remove
  // any parameter identifying that browser.  If set to NULL, the URL is not
  // modified.
  //
  // NOTE: This does not clear all values of the supplied TemplateURL; it's
  // expected callers will supply a new TemplateURL to this method.
  static bool Parse(const unsigned char* data,
                    size_t length,
                    ParameterFilter* parameter_filter,
                    TemplateURL* url);

 private:
  // No one should create one of these.
  TemplateURLParser();

  DISALLOW_EVIL_CONSTRUCTORS(TemplateURLParser);
};

#endif // CHROME_BROWSER_TEMPLATE_URL_PARSER_H__
