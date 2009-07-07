// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTED_PAGES_SOURCE_H_
#define PRINTING_PRINTED_PAGES_SOURCE_H_

#include <string>

class GURL;
class MessageLoop;

namespace printing {

class PrintedDocument;

// Source of printed pages.
class PrintedPagesSource {
 public:
  // Returns the document title.
  virtual std::wstring RenderSourceName() = 0;

  // Returns the URL's source of the document if applicable.
  virtual GURL RenderSourceUrl() = 0;
};

}  // namespace printing

#endif  // PRINTING_PRINTED_PAGES_SOURCE_H_
