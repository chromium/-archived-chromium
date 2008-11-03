// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GREASEMONKEY_SLAVE_H__
#define CHROME_RENDERER_GREASEMONKEY_SLAVE_H__

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/glue/webframe.h"


// Parsed representation of a Greasemonkey script.
class GreasemonkeyScript {
 public:
  GreasemonkeyScript(const StringPiece& script_url)
    : url_(script_url) {}

  // Gets the script body that should be injected into matching content.
  const StringPiece& GetBody() const {
    return body_;
  }

  // Gets a URL where this script can be found.
  const StringPiece& GetURL() const {
    return url_;
  }

  // Parses the text content of a user script file.
  void Parse(const StringPiece& script_text);

  // Returns true if the script should be applied to the specified URL, false
  // otherwise.
  bool MatchesUrl(const GURL& url);

 private:
  FRIEND_TEST(GreasemonkeySlaveTest, EscapeGlob);
  FRIEND_TEST(GreasemonkeySlaveTest, Parse1);
  FRIEND_TEST(GreasemonkeySlaveTest, Parse2);
  FRIEND_TEST(GreasemonkeySlaveTest, Parse3);

  // Helper function to convert the Greasemonkey glob format to the patterns
  // used internally to test URLs.
  static std::string EscapeGlob(const std::string& glob);

  // Parses the metadata block from the script.
  void ParseMetadata(const StringPiece& script_text);

  // Adds an include pattern that will be checked to determine whether to
  // include a script on a given page.
  void AddInclude(const std::string &glob_pattern);

  // The body of the script, which will be injected into content pages. This
  // references shared_memory_, and is valid until that memory is either
  // deleted or Unmap()'d.
  StringPiece body_;

  // The url of the file the script came from. This references shared_memory_,
  // and is valid until that memory is either deleted or Unmap()'d.
  StringPiece url_;

  // List of patterns to test URLs against for this script. These patterns have
  // been escaped for use with MatchPattern() in string_utils ('?' and '\' are
  // escaped).
  std::vector<std::string> include_patterns_;
};


// Manages installed GreasemonkeyScripts for a render process.
class GreasemonkeySlave {
 public:
  GreasemonkeySlave();

  // Update the parsed scripts from shared memory.
  bool UpdateScripts(SharedMemoryHandle shared_memory);

  // Inject the appropriate scripts into a frame based on its URL.
  // TODO(aa): Extract a GreasemonkeyFrame interface out of this to improve
  // testability.
  bool InjectScripts(WebFrame* frame);

 private:
  // Shared memory containing raw script data.
  scoped_ptr<SharedMemory> shared_memory_;

  // Parsed script data.
  std::vector<GreasemonkeyScript> scripts_;

  DISALLOW_COPY_AND_ASSIGN(GreasemonkeySlave);
};

#endif // CHROME_RENDERER_GREASEMONKEY_SLAVE_H__
