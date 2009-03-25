// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_UTIL_H__
#define NET_BASE_NET_UTIL_H__

#include "build/build_config.h"

#include <string>

#ifdef OS_WIN
#include <windows.h>
#endif

#include "base/basictypes.h"

struct addrinfo;
class FilePath;
class GURL;

namespace base {
class Time;
}

namespace net {

// Given the full path to a file name, creates a file: URL. The returned URL
// may not be valid if the input is malformed.
GURL FilePathToFileURL(const FilePath& path);
// Deprecated temporary compatibility function.
GURL FilePathToFileURL(const std::wstring& file_path);

// Converts a file: URL back to a filename that can be passed to the OS. The
// file URL must be well-formed (GURL::is_valid() must return true); we don't
// handle degenerate cases here. Returns true on success, false if it isn't a
// valid file URL. On failure, *file_path will be empty.
bool FileURLToFilePath(const GURL& url, FilePath* file_path);
// Deprecated temporary compatibility function.
bool FileURLToFilePath(const GURL& url, std::wstring* file_path);

// Split an input of the form <host>[":"<port>] into its consitituent parts.
// Saves the result into |*host| and |*port|. If the input did not have
// the optional port, sets |*port| to -1.
// Returns true if the parsing was successful, false otherwise.
// The returned host is NOT canonicalized, and may be invalid.
bool GetHostAndPort(std::string::const_iterator host_and_port_begin,
                    std::string::const_iterator host_and_port_end,
                    std::string* host,
                    int* port);
bool GetHostAndPort(const std::string& host_and_port,
                    std::string* host,
                    int* port);

// Returns the string representation of an address, like "192.168.0.1".
// Returns empty string on failure.
std::string NetAddressToString(const struct addrinfo* net_address);

// Returns the hostname of the current system. Returns empty string on failure.
std::string GetHostName();

// Return the value of the HTTP response header with name 'name'.  'headers'
// should be in the format that URLRequest::GetResponseHeaders() returns.
// Returns the empty string if the header is not found.
std::wstring GetSpecificHeader(const std::wstring& headers,
                               const std::wstring& name);
std::string GetSpecificHeader(const std::string& headers,
                              const std::string& name);

// Return the value of the HTTP response header field's parameter named
// 'param_name'.  Returns the empty string if the parameter is not found or is
// improperly formatted.
std::wstring GetHeaderParamValue(const std::wstring& field,
                                 const std::wstring& param_name);
std::string GetHeaderParamValue(const std::string& field,
                                const std::string& param_name);

// Return the filename extracted from Content-Disposition header.  Only two
// formats are supported: a. %-escaped UTF-8 b. RFC 2047.
//
// A non-ASCII param value is just returned as it is (assuming a NativeMB
// encoding). When a param value is ASCII, but is not in one of two forms
// supported, it is returned as it is unless it's pretty close to two supported
// formats but not well-formed. In that case, an empty string is returned.
//
// In any case, a caller must check for the empty return value and resort to
// another means to get a filename (e.g. url).
//
// This function does not do any escaping and callers are responsible for
// escaping 'unsafe' characters (e.g. (back)slash, colon) as they see fit.
//
// TODO(jungshik): revisit this issue. At the moment, the only caller
// net_util::GetSuggestedFilename and it calls ReplaceIllegalCharacters.  The
// other caller is a unit test. Need to figure out expose this function only to
// net_util_unittest.
//
std::wstring GetFileNameFromCD(const std::string& header);

// Converts the given host name to unicode characters, APPENDING them to the
// the given output string. This can be called for any host name, if the
// input is not IDN or is invalid in some way, we'll just append the ASCII
// source to the output so it is still usable.
//
// The input should be the canonicalized ASCII host name from GURL. This
// function does NOT accept UTF-8! Its length must also be given (this is
// designed to work on the substring of the host out of a URL spec).
//
// |languages| is a comma separated list of ISO 639 language codes. It
// is used to determine whether a hostname is 'comprehensible' to a user
// who understands languages listed. |host| will be converted to a
// human-readable form (Unicode) ONLY when each component of |host| is
// regarded as 'comprehensible'. Scipt-mixing is not allowed except that
// Latin letters in the ASCII range can be mixed with a limited set of
// script-language pairs (currently Han, Kana and Hangul for zh,ja and ko).
// When |languages| is empty, even that mixing is not allowed.
void IDNToUnicode(const char* host,
                  int host_len,
                  const std::wstring& languages,
                  std::wstring* out);

// Canonicalizes |host| and returns it.  If |is_ip_address| is non-NULL, sets it
// to true if |host| is an IP address.
std::string CanonicalizeHost(const std::string& host, bool* is_ip_address);
std::string CanonicalizeHost(const std::wstring& host, bool* is_ip_address);

// Call these functions to get the html for a directory listing.
// They will pass non-7bit-ascii characters unescaped, allowing
// the browser to interpret the encoding (utf8, etc).
std::string GetDirectoryListingHeader(const std::string& title);
std::string GetDirectoryListingEntry(const std::string& name, bool is_dir,
                                     int64 size, const base::Time& modified);

// If text starts with "www." it is removed, otherwise text is returned
// unmodified.
std::wstring StripWWW(const std::wstring& text);

// Gets the filename from the raw Content-Disposition header (as read from the
// network).  Otherwise uses the last path component name or hostname from
// |url|.  Note: it's possible for the suggested filename to be empty (e.g.,
// file:/// or view-cache:).
std::wstring GetSuggestedFilename(const GURL& url,
                                  const std::string& content_disposition,
                                  const std::wstring& default_name);

// DEPRECATED: Please use the above version of this method.
std::wstring GetSuggestedFilename(const GURL& url,
                                  const std::wstring& content_disposition,
                                  const std::wstring& default_name);

// Checks the given port against a list of ports which are restricted by
// default.  Returns true if the port is allowed, false if it is restricted.
bool IsPortAllowedByDefault(int port);

// Checks the given port against a list of ports which are restricted by the
// FTP protocol.  Returns true if the port is allowed, false if it is
// restricted.
bool IsPortAllowedByFtp(int port);

// Set socket to non-blocking mode
int SetNonBlocking(int fd);

}  // namespace net

#endif  // NET_BASE_NET_UTIL_H__
