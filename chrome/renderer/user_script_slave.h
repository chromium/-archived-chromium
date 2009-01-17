// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_SLAVE_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_SLAVE_H_

#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/glue/webframe.h"


// Parsed representation of a user script.
class UserScript {
 public:
  UserScript(const StringPiece& script_url)
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
  FRIEND_TEST(UserScriptSlaveTest, EscapeGlob);
  FRIEND_TEST(UserScriptSlaveTest, Parse1);
  FRIEND_TEST(UserScriptSlaveTest, Parse2);
  FRIEND_TEST(UserScriptSlaveTest, Parse3);

  // Helper function to convert the user script glob format to the patterns
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


// Manages installed UserScripts for a render process.
class UserScriptSlave {
 public:
  UserScriptSlave();

  // Update the parsed scripts from shared memory.
  bool UpdateScripts(base::SharedMemoryHandle shared_memory);

  // Inject the appropriate scripts into a frame based on its URL.
  // TODO(aa): Extract a UserScriptFrame interface out of this to improve
  // testability.
  bool InjectScripts(WebFrame* frame);

 private:
  // Shared memory containing raw script data.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // Parsed script data.
  std::vector<UserScript> scripts_;

  // Greasemonkey API source that is injected with the scripts.
  StringPiece api_js_;

  // The line number of the first line of the user script among all of the
  // injected javascript.  This is used to make reported errors correspond with
  // the proper line in the user script.
  int user_script_start_line_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSlave);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_SLAVE_H_
