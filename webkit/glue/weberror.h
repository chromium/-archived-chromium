// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBERROR_H__
#define WEBKIT_GLUE_WEBERROR_H__

class GURL;

class WebError {
 public:
  // The network error code.
  virtual int GetErrorCode() const = 0;

  // The URL that failed.
  virtual const GURL& GetFailedURL() const = 0;
};

#endif  // WEBKIT_GLUE_WEBERROR_H__
