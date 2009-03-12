// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLRESPONSE_H_
#define WEBKIT_GLUE_WEBURLRESPONSE_H_

#include <string>

#include "base/basictypes.h"

class GURL;

class WebResponse {
 public:
  // Get the URL.
  virtual GURL GetURL() const = 0;

  // Get the http status code.
  virtual int GetHttpStatusCode() const = 0;

  // Returns the mime type of the response.
  virtual std::string GetMimeType() const = 0;

  // Returns an opaque value containing the state of the SSL connection that
  // the resource was loaded on, or an empty string if no SSL connection was
  // used.
  virtual std::string GetSecurityInfo() const = 0;

  // Returns whether the content of this resource was filtered (usually for
  // security reasons).
  virtual bool IsContentFiltered() const = 0;

  virtual ~WebResponse() {}
};

#endif  // #ifndef WEBKIT_GLUE_WEBURLRESPONSE_H_
