// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  // URLs referenced do not point to valid http/https resources, false is
  // returned. |parameter_filter| can be used if you want to filter out some
  // parameters out of the URL. For example when importing from another browser
  // we remove any parameter identifying that browser.  If set to NULL, the URL
  // is not modified.
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

