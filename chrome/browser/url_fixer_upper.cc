// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <windows.h>

#include "chrome/browser/url_fixer_upper.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/common/gfx/text_elider.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_file.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

using namespace std;

// does some basic fixes for input that we want to test for file-ness
static void PrepareStringForFileOps(const wstring& text, wstring* output) {
  TrimWhitespace(text, TRIM_ALL, output);
  replace(output->begin(), output->end(), '/', '\\');
}

// Tries to create a full path from |text|.  If the result is valid and the
// file exists, returns true and sets |full_path| to the result.  Otherwise,
// returns false and leaves |full_path| unchanged.
static bool ValidPathForFile(const wstring& text, wstring* full_path) {
  wchar_t file_path[MAX_PATH];
  if (!_wfullpath(file_path, text.c_str(), MAX_PATH))
    return false;

  if (!file_util::PathExists(file_path))
    return false;

  full_path->assign(file_path);
  return true;
}

// Tries to create a file: URL from |text| if it looks like a filename, even if
// it doesn't resolve as a valid path or to an existing file.  Returns true
// with a (possibly invalid) file: URL in |fixed_up_url| for input beginning
// with a drive specifier or "\\".  Returns false in other cases (including
// file: URLs: these don't look like filenames), leaving fixed_up_url
// unchanged.
static wstring FixupPath(const wstring& text) {
  DCHECK(text.length() >= 2);

  wstring filename;
  PrepareStringForFileOps(text, &filename);

  if (filename[1] == '|')
    filename[1] = ':';

  // Here, we know the input looks like a file.
  GURL file_url = net::FilePathToFileURL(filename);
  if (file_url.is_valid())
    return gfx::ElideUrl(file_url, ChromeFont(), 0, std::wstring());

  // Invalid file URL, just return the input.
  return text;
}

// Checks |domain| to see if a valid TLD is already present.  If not, appends
// |desired_tld| to the domain, and prepends "www." unless it's already present.
// Then modifies |fixed_up_url| to reflect the changes.
static void AddDesiredTLD(const wstring& desired_tld,
                          wstring* domain) {
  if (desired_tld.empty() || domain->empty())
    return;

  // Check the TLD.  If the return value is positive, we already have a TLD, so
  // abort; if the return value is wstring::npos, there's no valid host (e.g. if
  // the user pasted in garbage for which HistoryURLProvider is trying to
  // suggest an exact match), so adding a TLD makes no sense.  The only useful
  // case is where the return value is 0 (there's a valid host with no known
  // TLD).  We disallow unknown registries here so users can input "mail.yahoo"
  // and hit ctrl-enter to get "www.mail.yahoo.com".
  const size_t registry_length =
      net::RegistryControlledDomainService::GetRegistryLength(*domain, false);
  if (registry_length != 0)
    return;

  // Add the suffix at the end of the domain.
  const size_t domain_length(domain->length());
  DCHECK(domain_length > 0);
  DCHECK(desired_tld[0] != '.');
  if ((*domain)[domain_length - 1] != '.')
    domain->push_back('.');
  domain->append(desired_tld);

  // Now, if the domain begins with "www.", stop.
  const wstring prefix(L"www.");
  if (domain->compare(0, prefix.length(), prefix) != 0) {
    // Otherwise, add www. to the beginning of the URL.
    domain->insert(0, prefix);
  }
}

static inline void FixupUsername(const wstring& text,
                                 const url_parse::Component& part,
                                 wstring* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the username at the moment.
  url->append(text, part.begin, part.len);
  // Do not append the trailing '@' because we might need to include the user's
  // password.  FixupURL itself will append the '@' for us.
}

static inline void FixupPassword(const wstring& text,
                                 const url_parse::Component& part,
                                 wstring* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the password at the moment.
  url->append(L":");
  url->append(text, part.begin, part.len);
}

static void FixupHost(const wstring& text,
                      const url_parse::Component& part,
                      bool has_scheme,
                      const wstring& desired_tld,
                      wstring* url) {
  if (!part.is_valid())
    return;

  // Make domain valid.
  // Strip all leading dots and all but one trailing dot, unless the user only
  // typed dots, in which case their input is totally invalid and we should just
  // leave it unchanged.
  wstring domain(text, part.begin, part.len);
  const size_t first_nondot(domain.find_first_not_of('.'));
  if (first_nondot != wstring::npos) {
    domain.erase(0, first_nondot);
    size_t last_nondot(domain.find_last_not_of('.'));
    DCHECK(last_nondot != wstring::npos);
    last_nondot += 2; // Point at second period in ending string
    if (last_nondot < domain.length())
      domain.erase(last_nondot);
  }

  // Add any user-specified TLD, if applicable.
  AddDesiredTLD(desired_tld, &domain);

  url->append(domain);
}

// Looks for a port number, including initial colon, at port_start.  If
// something invalid (which cannot be fixed up) is found, like ":foo" or
// ":7:7", returns false.  Otherwise, removes any extra colons
// ("::1337" -> ":1337", ":/" -> "/") and returns true.
static void FixupPort(const wstring& text,
                      const url_parse::Component& part,
                      wstring* url) {
  if (!part.is_valid())
    return;

  // Look for non-digit in port and strip if found.
  wstring port(text, part.begin, part.len);
  for (wstring::iterator i = port.begin(); i != port.end(); ) {
    if (IsAsciiDigit(*i))
      ++i;
    else
      i = port.erase(i);
  }

  if (port.empty())
    return; // Nothing to append.

  url->append(L":");
  url->append(port);
}

static inline void FixupPath(const wstring& text,
                             const url_parse::Component& part,
                             wstring* url) {
  if (!part.is_valid() || part.len == 0) {
    // We should always have a path.
    url->append(L"/");
    return;
  }

  // Append the path as is.
  url->append(text, part.begin, part.len);
}

static inline void FixupQuery(const wstring& text,
                              const url_parse::Component& part,
                              wstring* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the query at the moment.
  url->append(L"?");
  url->append(text, part.begin, part.len);
}

static inline void FixupRef(const wstring& text,
                            const url_parse::Component& part,
                            wstring* url) {
  if (!part.is_valid())
    return;

  // We don't fix up the ref at the moment.
  url->append(L"#");
  url->append(text, part.begin, part.len);
}

static void OffsetComponent(int offset, url_parse::Component* part) {
  DCHECK(part);

  if (part->is_valid()) {
    // Offset the location of this component.
    part->begin += offset;

    // This part might not have existed in the original text.
    if (part->begin < 0)
      part->reset();
  }
}

static bool HasPort(const std::wstring& original_text,
                    const url_parse::Component& scheme_component,
                    const std::wstring& scheme) {
  // Find the range between the ":" and the "/".
  size_t port_start = scheme_component.end() + 1;
  size_t port_end = port_start;
  while ((port_end < original_text.length()) &&
         !url_parse::IsAuthorityTerminator(original_text[port_end]))
    ++port_end;
  if (port_end == port_start)
    return false;

  // Scan the range to see if it is entirely digits.
  for (size_t i = port_start; i < port_end; ++i) {
    if (!IsAsciiDigit(original_text[i]))
      return false;
  }

  return true;
}

wstring URLFixerUpper::SegmentURL(const wstring& text,
                                  url_parse::Parsed* parts) {
  // Initialize the result.
  *parts = url_parse::Parsed();

  wstring trimmed;
  TrimWhitespace(text, TRIM_ALL, &trimmed);
  if (trimmed.empty())
    return wstring(); // Nothing to segment.

  int trimmed_length = static_cast<int>(trimmed.length());
  if (url_parse::DoesBeginWindowsDriveSpec(trimmed.data(), 0, trimmed_length)
      || url_parse::DoesBeginUNCPath(trimmed.data(), 0, trimmed_length, false))
    return L"file";

  // Otherwise, we need to look at things carefully.
  wstring scheme;
  if (url_parse::ExtractScheme(text.data(),
                               static_cast<int>(text.length()),
                               &parts->scheme)) {
    // We were able to extract a scheme.  Remember what we have, but we may
    // decide to change our minds later.
    scheme.assign(text.substr(parts->scheme.begin, parts->scheme.len));

    if (parts->scheme.is_valid() &&
        // Valid schemes are ASCII-only.
        (!IsStringASCII(scheme) ||
        // We need to fix up the segmentation for "www.example.com:/".  For this
        // case, we guess that schemes with a "." are not actually schemes.
        (scheme.find(L".") != wstring::npos) ||
        // We need to fix up the segmentation for "www:123/".  For this case, we
        // will add an HTTP scheme later and make the URL parser happy.
        // TODO(pkasting): Maybe we should try to use GURL's parser for this?
        HasPort(text, parts->scheme, scheme)))
      parts->scheme.reset();
  }

  // When we couldn't find a scheme in the input, we need to pick one.  Normally
  // we choose http, but if the URL starts with "ftp.", we match other browsers
  // and choose ftp.
  if (!parts->scheme.is_valid())
    scheme.assign(StartsWith(text, L"ftp.", false) ? L"ftp" : L"http");

  // Cannonicalize the scheme.
  StringToLowerASCII(&scheme);

  // Not segmenting file schemes or nonstandard schemes.
  if ((scheme == L"file") ||
      !url_util::IsStandard(scheme.c_str(), static_cast<int>(scheme.length()),
          url_parse::Component(0, static_cast<int>(scheme.length()))))
    return scheme;

  if (parts->scheme.is_valid()) {
    // Have the GURL parser do the heavy lifting for us.
    url_parse::ParseStandardURL(text.data(), static_cast<int>(text.length()),
                                parts);
    return scheme;
  }

  // We need to add a scheme in order for ParseStandardURL to be happy.
  // Find the first non-whitespace character.
  wstring::const_iterator first_nonwhite = text.begin();
  while ((first_nonwhite != text.end()) && IsWhitespace(*first_nonwhite))
    ++first_nonwhite;

  // Construct the text to parse by inserting the scheme.
  wstring inserted_text(scheme);
  inserted_text.append(L"://");
  wstring text_to_parse(text.begin(), first_nonwhite);
  text_to_parse.append(inserted_text);
  text_to_parse.append(first_nonwhite, text.end());

  // Have the GURL parser do the heavy lifting for us.
  url_parse::ParseStandardURL(text_to_parse.data(),
                              static_cast<int>(text_to_parse.length()),
                              parts);

  // Offset the results of the parse to match the original text.
  const int offset = -static_cast<int>(inserted_text.length());
  OffsetComponent(offset, &parts->scheme);
  OffsetComponent(offset, &parts->username);
  OffsetComponent(offset, &parts->password);
  OffsetComponent(offset, &parts->host);
  OffsetComponent(offset, &parts->port);
  OffsetComponent(offset, &parts->path);
  OffsetComponent(offset, &parts->query);
  OffsetComponent(offset, &parts->ref);

  return scheme;
}

std::wstring URLFixerUpper::FixupURL(const wstring& text,
                                     const wstring& desired_tld) {
  wstring trimmed;
  TrimWhitespace(text, TRIM_ALL, &trimmed);
  if (trimmed.empty())
    return wstring(); // Nothing here.

  // Segment the URL.
  url_parse::Parsed parts;
  wstring scheme(SegmentURL(trimmed, &parts));

  // We handle the file scheme separately.
  if (scheme == L"file")
    return (parts.scheme.is_valid() ? text : FixupPath(text));

  // For some schemes whose layouts we understand, we rebuild it.
  if (url_util::IsStandard(scheme.c_str(), static_cast<int>(scheme.length()),
          url_parse::Component(0, static_cast<int>(scheme.length())))) {
    wstring url(scheme);
    url.append(L"://");

    // We need to check whether the |username| is valid because it is our
    // responsibility to append the '@' to delineate the user information from
    // the host portion of the URL.
    if (parts.username.is_valid()) {
      FixupUsername(trimmed, parts.username, &url);
      FixupPassword(trimmed, parts.password, &url);
      url.append(L"@");
    }

    FixupHost(trimmed, parts.host, parts.scheme.is_valid(), desired_tld, &url);
    FixupPort(trimmed, parts.port, &url);
    FixupPath(trimmed, parts.path, &url);
    FixupQuery(trimmed, parts.query, &url);
    FixupRef(trimmed, parts.ref, &url);

    return url;
  }

  // In the worst-case, we insert a scheme if the URL lacks one.
  if (!parts.scheme.is_valid()) {
    wstring fixed_scheme(scheme);
    fixed_scheme.append(L"://");
    trimmed.insert(0, fixed_scheme);
  }

  return trimmed;
}

// The rules are different here than for regular fixup, since we need to handle
// input like "hello.html" and know to look in the current directory.  Regular
// fixup will look for cues that it is actually a file path before trying to
// figure out what file it is.  If our logic doesn't work, we will fall back on
// regular fixup.
wstring URLFixerUpper::FixupRelativeFile(const wstring& base_dir,
                                         const wstring& text) {
  wchar_t old_cur_directory[MAX_PATH];
  if (!base_dir.empty()) {
    // save the old current directory before we move to the new one
    // TODO: in the future, we may want to handle paths longer than MAX_PATH
    GetCurrentDirectory(MAX_PATH, old_cur_directory);
    SetCurrentDirectory(base_dir.c_str());
  }

  // allow funny input with extra whitespace and the wrong kind of slashes
  wstring trimmed;
  PrepareStringForFileOps(text, &trimmed);

  bool is_file = true;
  wstring full_path;
  if (!ValidPathForFile(trimmed, &full_path)) {
    // Not a path as entered, try unescaping it in case the user has
    // escaped things. We need to go through 8-bit since the escaped values
    // only represent 8-bit values.
    std::wstring unescaped = UTF8ToWide(UnescapeURLComponent(
        WideToUTF8(trimmed),
        UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS));
    if (!ValidPathForFile(unescaped, &full_path))
      is_file = false;
  }

  // Put back the current directory if we saved it.
  if (!base_dir.empty())
    SetCurrentDirectory(old_cur_directory);

  if (is_file) {
    GURL file_url = net::FilePathToFileURL(full_path);
    if (file_url.is_valid())
      return gfx::ElideUrl(file_url, ChromeFont(), 0, std::wstring());
    // Invalid files fall through to regular processing.
  }

  // Fall back on regular fixup for this input.
  return FixupURL(text, L"");
}

