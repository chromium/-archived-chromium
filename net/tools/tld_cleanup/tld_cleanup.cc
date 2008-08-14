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

// This command-line program converts an effective-TLD data file in UTF-8 from
// the format provided by Mozilla to the format expected by Chrome.  Any errors
// or warnings are recorded in tld_cleanup.log.
//
// In particular, it
//  * Strips blank lines and comments, as well as notes for individual rules.
//  * Changes all line endings to LF.
//  * Strips a single leading and/or trailing dot from each rule, if present.
//  * Logs a warning if a rule contains '!' or '*.' other than at the beginning
//    of the rule.  (This also catches multiple ! or *. at the start of a rule.)
//  * Logs a warning if GURL reports a rule as invalid, but keeps the rule.
//  * Canonicalizes each rule's domain by converting it to a GURL and back.
//  * Adds explicit rules for true TLDs found in any rule.

#include <windows.h>
#include <set>
#include <string>

#include "base/at_exit.h"
#include "base/file_util.h"
#include "base/icu_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"

static const wchar_t* const kLogFileName = L"tld_cleanup.log";
typedef std::set<std::string> StringSet;

// Writes the list of domain rules contained in the 'rules' set to the
// 'outfile', with each rule terminated by a LF.  The file must already have
// been created with write access.
bool WriteRules(const StringSet& rules, HANDLE outfile) {
  std::string data;
  for (StringSet::const_iterator iter = rules.begin();
       iter != rules.end();
       ++iter) {
    data.append(*iter);
    data.append(1, '\n');
  }

  unsigned long written = 0;
  BOOL success = WriteFile(outfile,
                           data.data(),
                           static_cast<long>(data.size()),
                           &written,
                           NULL);
  return (success && written == static_cast<long>(data.size()));
}

// These result codes should be in increasing order of severity.
typedef enum {
  kSuccess,
  kWarning,
  kError,
} NormalizeResult;

// Adjusts the rule to a standard form: removes single extraneous dots and
// canonicalizes it using GURL. Returns kSuccess if the rule is interpreted as
// valid; logs a warning and returns kWarning if it is probably invalid; and
// logs an error and returns kError if the rule is (almost) certainly invalid.
NormalizeResult NormalizeRule(std::string* rule) {
  NormalizeResult result = kSuccess;

  // Strip single leading and trailing dots.
  if (rule->at(0) == '.')
    rule->erase(0, 1);
  if (rule->size() == 0) {
    LOG(WARNING) << "Ignoring empty rule";
    return kWarning;
  }
  if (rule->at(rule->size() - 1) == '.')
    rule->erase(rule->size() - 1, 1);
  if (rule->size() == 0) {
    LOG(WARNING) << "Ignoring empty rule";
    return kWarning;
  }

  // Allow single leading '*.' or '!', saved here so it's not canonicalized.
  bool wildcard = false;
  bool exception = false;
  size_t start_offset = 0;
  if (rule->at(0) == '!') {
    rule->erase(0, 1);
    exception = true;
  } else if (rule->find("*.") == 0) {
    rule->erase(0, 2);
    wildcard = true;
  }
  if (rule->size() == 0) {
    LOG(WARNING) << "Ignoring empty rule";
    return kWarning;
  }

  // Warn about additional '*.' or '!'.
  if (rule->find("*.", start_offset) != std::string::npos ||
      rule->find('!', start_offset) != std::string::npos) {
    LOG(WARNING) << "Keeping probably invalid rule: " << *rule;
    result = kWarning;
  }

  // Make a GURL and normalize it, then get the host back out.
  std::string url = "http://";
  url.append(*rule);
  GURL gurl(url);
  const std::string& spec = gurl.possibly_invalid_spec();
  url_parse::Component host = gurl.parsed_for_possibly_invalid_spec().host;
  if (host.len < 0) {
    LOG(ERROR) << "Ignoring rule that couldn't be normalized: " << *rule;
    return kError;
  }
  if (!gurl.is_valid()) {
    LOG(WARNING) << "Keeping rule that GURL says is invalid: " << *rule;
    result = kWarning;
  }
  rule->assign(spec.substr(host.begin, host.len));

  // Restore wildcard or exception marker.
  if (exception)
    rule->insert(0, 1, '!');
  else if (wildcard)
    rule->insert(0, "*.");

  return result;
}

// Loads the file described by 'in_filename', converts it to the desired format
// (see the file comments above), and saves it into 'out_filename'.  Returns
// the most severe of the result codes encountered when normalizing the rules.
NormalizeResult NormalizeFile(const std::wstring& in_filename,
                              const std::wstring& out_filename) {
  std::string data;
  if (!file_util::ReadFileToString(in_filename, &data)) {
    fwprintf(stderr, L"Unable to read file %ls\n", in_filename.c_str());
    // We return success since we've already reported the error.
    return kSuccess;
  }

  HANDLE outfile(CreateFile(out_filename.c_str(),
                            GENERIC_WRITE,
                            0,
                            NULL,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL));
  if (outfile == INVALID_HANDLE_VALUE) {
    fwprintf(stderr, L"Unable to write file %ls\n", out_filename.c_str());
    // We return success since we've already reported the error.
    return kSuccess;
  }

  // We do a lot of string assignment during parsing, but simplicity is more
  // important than performance here.
  std::string rule;
  NormalizeResult result = kSuccess;
  size_t line_start = 0;
  size_t line_end = 0;
  StringSet rules;
  while (line_start < data.size()) {
    // Skip comments.
    if (line_start + 1 < data.size() &&
        data[line_start] == '/' &&
        data[line_start + 1] == '/') {
      line_end = data.find_first_of("\r\n", line_start);
      if (line_end == std::string::npos)
        line_end = data.size();
    } else {
      // Truncate at first whitespace.
      line_end = data.find_first_of("\r\n \t", line_start);
      if (line_end == std::string::npos)
        line_end = data.size();
      rule.assign(data.data(), line_start, line_end - line_start);

      NormalizeResult new_result = NormalizeRule(&rule);
      if (new_result != kError) {
        rules.insert(rule);
        // Add true TLD for multi-level rules.
        size_t tld_start = rule.find_last_of('.');
        if (tld_start != std::string::npos && tld_start + 1 < rule.size())
          rules.insert(rule.substr(tld_start + 1));
      }
      result = std::max(result, new_result);
    }

    // Find beginning of next non-empty line.
    line_start = data.find_first_of("\r\n", line_end);
    if (line_start == std::string::npos)
      line_start = data.size();
    line_start = data.find_first_not_of("\r\n", line_start);
    if (line_start == std::string::npos)
      line_start = data.size();
  }

  if (!WriteRules(rules, outfile)) {
    LOG(ERROR) << "Error(s) writing " << out_filename;
    result = kError;
  }

  return result;
}

int main(int argc, const char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Normalizes and verifies UTF-8 TLD data files\n");
    fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
    return 1;
  }

  // Manages the destruction of singletons.
  base::AtExitManager exit_manager;

  // Only use OutputDebugString in debug mode.
#ifdef NDEBUG
  logging::LoggingDestination destination = logging::LOG_ONLY_TO_FILE;
#else
  logging::LoggingDestination destination =
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
#endif

  std::wstring log_filename;
  PathService::Get(base::DIR_EXE, &log_filename);
  file_util::AppendToPath(&log_filename, kLogFileName);
  logging::InitLogging(log_filename.c_str(),
                       destination,
                       logging::LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE);

  icu_util::Initialize();

  NormalizeResult result = NormalizeFile(UTF8ToWide(argv[1]),
                                         UTF8ToWide(argv[2]));
  if (result != kSuccess) {
    fwprintf(stderr, L"Errors or warnings processing file.  See log in %ls.",
             kLogFileName);
  }

  if (result == kError)
    return 1;
  return 0;
}
