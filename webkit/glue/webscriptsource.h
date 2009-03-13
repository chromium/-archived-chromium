// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBSCRIPT_SOURCE_H_
#define WEBKIT_GLUE_WEBSCRIPT_SOURCE_H_

namespace webkit_glue {

// Describes some script that can be executed within a frame.
// NOTE: start_line is 1-based (like the corresponding object in WebCore).
// TODO(aa): Allow clients to specify external data intead of strings to avoid
// copies.
struct WebScriptSource {
  WebScriptSource(const std::string& source)
      : source(source), start_line(1) {}
  WebScriptSource(const std::string& source, const GURL& url)
      : source(source), url(url), start_line(1) {}
  WebScriptSource(const std::string& source, const GURL& url, int start_line)
      : source(source), url(url), start_line(start_line) {}

  std::string source;
  GURL url;
  int start_line;
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBSCRIPT_SOURCE_H_
