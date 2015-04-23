// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rules for parsing content-types were borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_util.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/string_util.h"

using std::string;

namespace net {

//-----------------------------------------------------------------------------

// Return the index of the closing quote of the string, if any.
static size_t FindStringEnd(const string& line, size_t start, char delim) {
  DCHECK(start < line.length() && line[start] == delim &&
         (delim == '"' || delim == '\''));

  const char set[] = { delim, '\\', '\0' };
  for (;;) {
    // start points to either the start quote or the last
    // escaped char (the char following a '\\')

    size_t end = line.find_first_of(set, start + 1);
    if (end == string::npos)
      return line.length();

    if (line[end] == '\\') {
      // Hit a backslash-escaped char.  Need to skip over it.
      start = end + 1;
      if (start == line.length())
        return start;

      // Go back to looking for the next escape or the string end
      continue;
    }

    return end;
  }

  NOTREACHED();
  return line.length();
}

//-----------------------------------------------------------------------------

// static
std::string HttpUtil::PathForRequest(const GURL& url) {
  DCHECK(url.is_valid() && (url.SchemeIs("http") || url.SchemeIs("https")));
  if (url.has_query())
    return url.path() + "?" + url.query();
  return url.path();
}

// static
std::string HttpUtil::SpecForRequest(const GURL& url) {
  DCHECK(url.is_valid() && (url.SchemeIs("http") || url.SchemeIs("https")));
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements).spec();
}

// static
size_t HttpUtil::FindDelimiter(const string& line, size_t search_start,
                               char delimiter) {
  do {
    // search_start points to the spot from which we should start looking
    // for the delimiter.
    const char delim_str[] = { delimiter, '"', '\'', '\0' };
    size_t cur_delim_pos = line.find_first_of(delim_str, search_start);
    if (cur_delim_pos == string::npos)
      return line.length();

    char ch = line[cur_delim_pos];
    if (ch == delimiter) {
      // Found delimiter
      return cur_delim_pos;
    }

    // We hit the start of a quoted string.  Look for its end.
    search_start = FindStringEnd(line, cur_delim_pos, ch);
    if (search_start == line.length())
      return search_start;

    ++search_start;

    // search_start now points to the first char after the end of the
    // string, so just go back to the top of the loop and look for
    // |delimiter| again.
  } while (true);

  NOTREACHED();
  return line.length();
}

// static
void HttpUtil::ParseContentType(const string& content_type_str,
                                string* mime_type, string* charset,
                                bool *had_charset) {
  // Trim leading and trailing whitespace from type.  We include '(' in
  // the trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  size_t type_val = content_type_str.find_first_not_of(HTTP_LWS);
  type_val = std::min(type_val, content_type_str.length());
  size_t type_end = content_type_str.find_first_of(HTTP_LWS ";(", type_val);
  if (string::npos == type_end)
    type_end = content_type_str.length();

  size_t charset_val = 0;
  size_t charset_end = 0;

  // Iterate over parameters
  bool type_has_charset = false;
  size_t param_start = content_type_str.find_first_of(';', type_end);
  if (param_start != string::npos) {
    // We have parameters.  Iterate over them.
    size_t cur_param_start = param_start + 1;
    do {
      size_t cur_param_end =
          FindDelimiter(content_type_str, cur_param_start, ';');

      size_t param_name_start = content_type_str.find_first_not_of(HTTP_LWS,
          cur_param_start);
      param_name_start = std::min(param_name_start, cur_param_end);

      static const char charset_str[] = "charset=";
      size_t charset_end_offset = std::min(param_name_start +
          sizeof(charset_str) - 1, cur_param_end);
      if (LowerCaseEqualsASCII(content_type_str.begin() + param_name_start,
          content_type_str.begin() + charset_end_offset, charset_str)) {
        charset_val = param_name_start + sizeof(charset_str) - 1;
        charset_end = cur_param_end;
        type_has_charset = true;
      }

      cur_param_start = cur_param_end + 1;
    } while (cur_param_start < content_type_str.length());
  }

  if (type_has_charset) {
    // Trim leading and trailing whitespace from charset_val.  We include
    // '(' in the trailing trim set to catch media-type comments, which are
    // not at all standard, but may occur in rare cases.
    charset_val = content_type_str.find_first_not_of(HTTP_LWS, charset_val);
    charset_val = std::min(charset_val, charset_end);
    char first_char = content_type_str[charset_val];
    if (first_char == '"' || first_char == '\'') {
      charset_end = FindStringEnd(content_type_str, charset_val, first_char);
      ++charset_val;
      DCHECK(charset_end >= charset_val);
    } else {
      charset_end = std::min(content_type_str.find_first_of(HTTP_LWS ";(",
                                                            charset_val),
                             charset_end);
    }
  }

  // if the server sent "*/*", it is meaningless, so do not store it.
  // also, if type_val is the same as mime_type, then just update the
  // charset.  however, if charset is empty and mime_type hasn't
  // changed, then don't wipe-out an existing charset.  We
  // also want to reject a mime-type if it does not include a slash.
  // some servers give junk after the charset parameter, which may
  // include a comma, so this check makes us a bit more tolerant.
  if (content_type_str.length() != 0 &&
      content_type_str != "*/*" &&
      content_type_str.find_first_of('/') != string::npos) {
    // Common case here is that mime_type is empty
    bool eq = !mime_type->empty() &&
        LowerCaseEqualsASCII(content_type_str.begin() + type_val,
                             content_type_str.begin() + type_end,
                             mime_type->data());
    if (!eq) {
      mime_type->assign(content_type_str.begin() + type_val,
                        content_type_str.begin() + type_end);
      StringToLowerASCII(mime_type);
    }
    if ((!eq && *had_charset) || type_has_charset) {
      *had_charset = true;
      charset->assign(content_type_str.begin() + charset_val,
                      content_type_str.begin() + charset_end);
      StringToLowerASCII(charset);
    }
  }
}

// static
// Parse the Range header according to RFC 2616 14.35.1
// ranges-specifier = byte-ranges-specifier
// byte-ranges-specifier = bytes-unit "=" byte-range-set
// byte-range-set  = 1#( byte-range-spec | suffix-byte-range-spec )
// byte-range-spec = first-byte-pos "-" [last-byte-pos]
// first-byte-pos  = 1*DIGIT
// last-byte-pos   = 1*DIGIT
bool HttpUtil::ParseRanges(const std::string& headers,
                           std::vector<HttpByteRange>* ranges) {
  std::string ranges_specifier;
  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  while (it.GetNext()) {
    // Look for "Range" header.
    if (!LowerCaseEqualsASCII(it.name(), "range"))
      continue;
    ranges_specifier = it.values();
    // We just care about the first "Range" header, so break here.
    break;
  }

  if (ranges_specifier.empty())
    return false;

  size_t equal_char_offset = ranges_specifier.find('=');
  if (equal_char_offset == std::string::npos)
    return false;

  // Try to extract bytes-unit part.
  std::string::const_iterator bytes_unit_begin = ranges_specifier.begin();
  std::string::const_iterator bytes_unit_end = bytes_unit_begin +
                                               equal_char_offset;
  std::string::const_iterator byte_range_set_begin = bytes_unit_end + 1;
  std::string::const_iterator byte_range_set_end = ranges_specifier.end();

  TrimLWS(&bytes_unit_begin, &bytes_unit_end);
  // "bytes" unit identifier is not found.
  if (!LowerCaseEqualsASCII(bytes_unit_begin, bytes_unit_end, "bytes"))
    return false;

  ValuesIterator byte_range_set_iterator(byte_range_set_begin,
                                         byte_range_set_end, ',');
  while (byte_range_set_iterator.GetNext()) {
    size_t minus_char_offset = byte_range_set_iterator.value().find('-');
    // If '-' character is not found, reports failure.
    if (minus_char_offset == std::string::npos)
      return false;

    std::string::const_iterator first_byte_pos_begin =
        byte_range_set_iterator.value_begin();
    std::string::const_iterator first_byte_pos_end =
        first_byte_pos_begin +  minus_char_offset;
    TrimLWS(&first_byte_pos_begin, &first_byte_pos_end);
    std::string first_byte_pos(first_byte_pos_begin, first_byte_pos_end);

    HttpByteRange range;
    // Try to obtain first-byte-pos.
    if (!first_byte_pos.empty()) {
        int64 first_byte_position = -1;
        if (!StringToInt64(first_byte_pos, &first_byte_position))
          return false;
        range.set_first_byte_position(first_byte_position);
      }

    std::string::const_iterator last_byte_pos_begin =
        byte_range_set_iterator.value_begin() + minus_char_offset + 1;
    std::string::const_iterator last_byte_pos_end =
        byte_range_set_iterator.value_end();
    TrimLWS(&last_byte_pos_begin, &last_byte_pos_end);
    std::string last_byte_pos(last_byte_pos_begin, last_byte_pos_end);

    // We have last-byte-pos or suffix-byte-range-spec in this case.
    if (!last_byte_pos.empty()) {
      int64 last_byte_position;
      if (!StringToInt64(last_byte_pos, &last_byte_position))
        return false;
      if (range.HasFirstBytePosition())
        range.set_last_byte_position(last_byte_position);
      else
        range.set_suffix_length(last_byte_position);
    } else if (!range.HasFirstBytePosition()) {
      return false;
    }

    // Do a final check on the HttpByteRange object.
    if (!range.IsValid())
      return false;
    ranges->push_back(range);
  }
  return ranges->size() > 0;
}

// static
bool HttpUtil::HasHeader(const std::string& headers, const char* name) {
  size_t name_len = strlen(name);
  string::const_iterator it =
      std::search(headers.begin(),
                  headers.end(),
                  name,
                  name + name_len,
                  CaseInsensitiveCompareASCII<char>());
  if (it == headers.end())
    return false;

  // ensure match is prefixed by newline
  if (it != headers.begin() && it[-1] != '\n')
    return false;

  // ensure match is suffixed by colon
  if (it + name_len >= headers.end() || it[name_len] != ':')
    return false;

  return true;
}

// static
std::string HttpUtil::StripHeaders(const std::string& headers,
                                   const char* const headers_to_remove[],
                                   size_t headers_to_remove_len) {
  std::string stripped_headers;
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  while (it.GetNext()) {
    bool should_remove = false;
    for (size_t i = 0; i < headers_to_remove_len; ++i) {
      if (LowerCaseEqualsASCII(it.name_begin(), it.name_end(),
                               headers_to_remove[i])) {
        should_remove = true;
        break;
      }
    }
    if (!should_remove) {
      // Assume that name and values are on the same line.
      stripped_headers.append(it.name_begin(), it.values_end());
      stripped_headers.append("\r\n");
    }
  }
  return stripped_headers;
}

// static
bool HttpUtil::IsNonCoalescingHeader(string::const_iterator name_begin,
                                     string::const_iterator name_end) {
  // NOTE: "set-cookie2" headers do not support expires attributes, so we don't
  // have to list them here.
  const char* kNonCoalescingHeaders[] = {
    "date",
    "expires",
    "last-modified",
    "location",  // See bug 1050541 for details
    "retry-after",
    "set-cookie",
    // The format of auth-challenges mixes both space separated tokens and
    // comma separated properties, so coalescing on comma won't work.
    "www-authenticate",
    "proxy-authenticate"
  };
  for (size_t i = 0; i < arraysize(kNonCoalescingHeaders); ++i) {
    if (LowerCaseEqualsASCII(name_begin, name_end, kNonCoalescingHeaders[i]))
      return true;
  }
  return false;
}

bool HttpUtil::IsLWS(char c) {
  return strchr(HTTP_LWS, c) != NULL;
}

void HttpUtil::TrimLWS(string::const_iterator* begin,
                       string::const_iterator* end) {
  // leading whitespace
  while (*begin < *end && IsLWS((*begin)[0]))
    ++(*begin);

  // trailing whitespace
  while (*begin < *end && IsLWS((*end)[-1]))
    --(*end);
}

// static
bool HttpUtil::IsQuote(char c) {
  // Single quote mark isn't actually part of quoted-text production,
  // but apparently some servers rely on this.
  return c == '"' || c == '\'';
}

// static
std::string HttpUtil::Unquote(std::string::const_iterator begin,
                              std::string::const_iterator end) {
  // Empty string
  if (begin == end)
    return std::string();

  // Nothing to unquote.
  if (!IsQuote(*begin))
    return std::string(begin, end);

  // No terminal quote mark.
  if (end - begin < 2 || *begin != *(end - 1))
    return std::string(begin, end);

  // Strip quotemarks
  ++begin;
  --end;

  // Unescape quoted-pair (defined in RFC 2616 section 2.2)
  std::string unescaped;
  bool prev_escape = false;
  for (; begin != end; ++begin) {
    char c = *begin;
    if (c == '\\' && !prev_escape) {
      prev_escape = true;
      continue;
    }
    prev_escape = false;
    unescaped.push_back(c);
  }
  return unescaped;
}

// static
std::string HttpUtil::Unquote(const std::string& str) {
  return Unquote(str.begin(), str.end());
}

// static
std::string HttpUtil::Quote(const std::string& str) {
  std::string escaped;
  escaped.reserve(2 + str.size());

  std::string::const_iterator begin = str.begin();
  std::string::const_iterator end = str.end();

  // Esape any backslashes or quotemarks within the string, and
  // then surround with quotes.
  escaped.push_back('"');
  for (; begin != end; ++begin) {
    char c = *begin;
    if (c == '"' || c == '\\')
      escaped.push_back('\\');
    escaped.push_back(c);
  }
  escaped.push_back('"');
  return escaped;
}

// Find the "http" substring in a status line. This allows for
// some slop at the start. If the "http" string could not be found
// then returns -1.
// static
int HttpUtil::LocateStartOfStatusLine(const char* buf, int buf_len) {
  const int slop = 4;
  const int http_len = 4;

  if (buf_len >= http_len) {
    int i_max = std::min(buf_len - http_len, slop);
    for (int i = 0; i <= i_max; ++i) {
      if (LowerCaseEqualsASCII(buf + i, buf + i + http_len, "http"))
        return i;
    }
  }
  return -1; // Not found
}

int HttpUtil::LocateEndOfHeaders(const char* buf, int buf_len, int i) {
  bool was_lf = false;
  char last_c = '\0';
  for (; i < buf_len; ++i) {
    char c = buf[i];
    if (c == '\n') {
      if (was_lf)
        return i + 1;
      was_lf = true;
    } else if (c != '\r' || last_c != '\n') {
      was_lf = false;
    }
    last_c = c;
  }
  return -1;
}

// In order for a line to be continuable, it must specify a
// non-blank header-name. Line continuations are specifically for
// header values -- do not allow headers names to span lines.
static bool IsLineSegmentContinuable(const char* begin, const char* end) {
  if (begin == end)
    return false;

  const char* colon = std::find(begin, end, ':');
  if (colon == end)
    return false;

  const char* name_begin = begin;
  const char* name_end = colon;

  // Name can't be empty.
  if (name_begin == name_end)
    return false;

  // Can't start with LWS (this would imply the segment is a continuation)
  if (HttpUtil::IsLWS(*name_begin))
    return false;

  return true;
}

// Helper used by AssembleRawHeaders, to find the end of the status line.
static const char* FindStatusLineEnd(const char* begin, const char* end) {
  size_t i = StringPiece(begin, end - begin).find_first_of("\r\n");
  if (i == StringPiece::npos)
    return end;
  return begin + i;
}

// Helper used by AssembleRawHeaders, to skip past leading LWS.
static const char* FindFirstNonLWS(const char* begin, const char* end) {
  for (const char* cur = begin; cur != end; ++cur) {
    if (!HttpUtil::IsLWS(*cur))
      return cur;
  }
  return end; // Not found.
}

std::string HttpUtil::AssembleRawHeaders(const char* input_begin,
                                         int input_len) {
  std::string raw_headers;
  raw_headers.reserve(input_len);

  const char* input_end = input_begin + input_len;

  // Skip any leading slop, since the consumers of this output
  // (HttpResponseHeaders) don't deal with it.
  int status_begin_offset = LocateStartOfStatusLine(input_begin, input_len);
  if (status_begin_offset != -1)
    input_begin += status_begin_offset;

  // Copy the status line.
  const char* status_line_end = FindStatusLineEnd(input_begin, input_end);
  raw_headers.append(input_begin, status_line_end);

  // After the status line, every subsequent line is a header line segment.
  // Should a segment start with LWS, it is a continuation of the previous
  // line's field-value.

  // TODO(ericroman): is this too permissive? (delimits on [\r\n]+)
  CStringTokenizer lines(status_line_end, input_end, "\r\n");

  // This variable is true when the previous line was continuable.
  bool prev_line_continuable = false;

  while (lines.GetNext()) {
    const char* line_begin = lines.token_begin();
    const char* line_end = lines.token_end();

    if (prev_line_continuable && IsLWS(*line_begin)) {
      // Join continuation; reduce the leading LWS to a single SP.
      raw_headers.push_back(' ');
      raw_headers.append(FindFirstNonLWS(line_begin, line_end), line_end);
    } else {
      // Terminate the previous line.
      raw_headers.push_back('\0');

      // Copy the raw data to output.
      raw_headers.append(line_begin, line_end);

      // Check if the current line can be continued.
      prev_line_continuable = IsLineSegmentContinuable(line_begin, line_end);
    }
  }

  raw_headers.append("\0\0", 2);
  return raw_headers;
}

// TODO(jungshik): 1. If the list is 'fr-CA,fr-FR,en,de', we have to add
// 'fr' after 'fr-CA' with the same q-value as 'fr-CA' because
// web servers, in general, do not fall back to 'fr' and may end up picking
// 'en' which has a lower preference than 'fr-CA' and 'fr-FR'.
// 2. This function assumes that the input is a comma separated list
// without any whitespace. As long as it comes from the preference and
// a user does not manually edit the preference file, it's the case. Still,
// we may have to make it more robust.
std::string HttpUtil::GenerateAcceptLanguageHeader(
    const std::string& raw_language_list) {
  // We use integers for qvalue and qvalue decrement that are 10 times
  // larger than actual values to avoid a problem with comparing
  // two floating point numbers.
  const unsigned int kQvalueDecrement10 = 2;
  unsigned int qvalue10 = 10;
  StringTokenizer t(raw_language_list, ",");
  std::string lang_list_with_q;
  while (t.GetNext()) {
    std::string language = t.token();
    if (qvalue10 == 10) {
      // q=1.0 is implicit.
      lang_list_with_q = language;
    } else {
      DCHECK_LT(qvalue10, 10U);
      StringAppendF(&lang_list_with_q, ",%s;q=0.%d", language.c_str(),
                    qvalue10);
    }
    // It does not make sense to have 'q=0'.
    if (qvalue10 > kQvalueDecrement10)
      qvalue10 -= kQvalueDecrement10;
  }
  return lang_list_with_q;
}

std::string HttpUtil::GenerateAcceptCharsetHeader(const std::string& charset) {
  std::string charset_with_q = charset;
  if (LowerCaseEqualsASCII(charset, "utf-8")) {
    charset_with_q += ",*;q=0.5";
  } else {
    charset_with_q += ",utf-8;q=0.7,*;q=0.3";
  }
  return charset_with_q;
}

// BNF from section 4.2 of RFC 2616:
//
//   message-header = field-name ":" [ field-value ]
//   field-name     = token
//   field-value    = *( field-content | LWS )
//   field-content  = <the OCTETs making up the field-value
//                     and consisting of either *TEXT or combinations
//                     of token, separators, and quoted-string>
//

HttpUtil::HeadersIterator::HeadersIterator(string::const_iterator headers_begin,
                                           string::const_iterator headers_end,
                                           const std::string& line_delimiter)
    : lines_(headers_begin, headers_end, line_delimiter) {
}

bool HttpUtil::HeadersIterator::GetNext() {
  while (lines_.GetNext()) {
    name_begin_ = lines_.token_begin();
    values_end_ = lines_.token_end();

    string::const_iterator colon = find(name_begin_, values_end_, ':');
    if (colon == values_end_)
      continue;  // skip malformed header

    name_end_ = colon;

    // If the name starts with LWS, it is an invalid line.
    // Leading LWS implies a line continuation, and these should have
    // already been joined by AssembleRawHeaders().
    if (name_begin_ == name_end_ || IsLWS(*name_begin_))
      continue;

    TrimLWS(&name_begin_, &name_end_);
    if (name_begin_ == name_end_)
      continue;  // skip malformed header

    values_begin_ = colon + 1;
    TrimLWS(&values_begin_, &values_end_);

    // if we got a header name, then we are done.
    return true;
  }
  return false;
}

HttpUtil::ValuesIterator::ValuesIterator(
    string::const_iterator values_begin,
    string::const_iterator values_end,
    char delimiter)
    : values_(values_begin, values_end, string(1, delimiter)) {
  values_.set_quote_chars("\'\"");
}

bool HttpUtil::ValuesIterator::GetNext() {
  while (values_.GetNext()) {
    value_begin_ = values_.token_begin();
    value_end_ = values_.token_end();
    TrimLWS(&value_begin_, &value_end_);

    // bypass empty values.
    if (value_begin_ != value_end_)
      return true;
  }
  return false;
}

}  // namespace net
