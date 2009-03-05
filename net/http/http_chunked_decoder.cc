/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Derived from:
// mozilla/netwerk/protocol/http/src/nsHttpChunkedDecoder.cpp

#include "net/http/http_chunked_decoder.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"

namespace net {

HttpChunkedDecoder::HttpChunkedDecoder()
    : chunk_remaining_(0),
      chunk_terminator_remaining_(false),
      reached_last_chunk_(false),
      reached_eof_(false) {
}

int HttpChunkedDecoder::FilterBuf(char* buf, int buf_len) {
  int result = 0;

  while (buf_len) {
    if (chunk_remaining_) {
      int num = std::min(chunk_remaining_, buf_len);

      buf_len -= num;
      chunk_remaining_ -= num;

      result += num;
      buf += num;

      // After each chunk's data there should be a CRLF
      if (!chunk_remaining_)
        chunk_terminator_remaining_ = true;
      continue;
    } else if (reached_eof_) {
      break;  // Done!
    }

    int bytes_consumed = ScanForChunkRemaining(buf, buf_len);
    if (bytes_consumed < 0)
      return bytes_consumed; // Error

    buf_len -= bytes_consumed;
    if (buf_len)
      memmove(buf, buf + bytes_consumed, buf_len);
  }

  return result;
}

int HttpChunkedDecoder::ScanForChunkRemaining(const char* buf, int buf_len) {
  DCHECK(chunk_remaining_ == 0);
  DCHECK(buf_len > 0);

  int bytes_consumed = 0;

  size_t index_of_lf = StringPiece(buf, buf_len).find('\n');
  if (index_of_lf != StringPiece::npos) {
    buf_len = static_cast<int>(index_of_lf);
    if (buf_len && buf[buf_len - 1] == '\r')  // Eliminate a preceding CR.
      buf_len--;
    bytes_consumed = static_cast<int>(index_of_lf) + 1;

    // Make buf point to the full line buffer to parse.
    if (!line_buf_.empty()) {
      line_buf_.append(buf, buf_len);
      buf = line_buf_.data();
      buf_len = static_cast<int>(line_buf_.size());
    }

    if (reached_last_chunk_) {
      if (buf_len) {
        DLOG(INFO) << "ignoring http trailer";
      } else {
        reached_eof_ = true;
      }
    } else if (chunk_terminator_remaining_) {
       if (buf_len) {
         DLOG(ERROR) << "chunk data not terminated properly";
         return ERR_INVALID_CHUNKED_ENCODING;
       }
       chunk_terminator_remaining_ = false;
    } else if (buf_len) {
      // Ignore any chunk-extensions.
      size_t index_of_semicolon = StringPiece(buf, buf_len).find(';');
      if (index_of_semicolon != StringPiece::npos)
        buf_len = static_cast<int>(index_of_semicolon);

      if (!ParseChunkSize(buf, buf_len, &chunk_remaining_)) {
        DLOG(ERROR) << "Failed parsing HEX from: " <<
            std::string(buf, buf_len);
        return ERR_INVALID_CHUNKED_ENCODING;
      }

      if (chunk_remaining_ == 0)
        reached_last_chunk_ = true;
    } else {
      DLOG(ERROR) << "missing chunk-size";
      return ERR_INVALID_CHUNKED_ENCODING;
    }
    line_buf_.clear();
  } else {
    // Save the partial line; wait for more data.
    bytes_consumed = buf_len;

    // Ignore a trailing CR
    if (buf[buf_len - 1] == '\r')
      buf_len--;

    line_buf_.append(buf, buf_len);
  }
  return bytes_consumed;
}


// While the HTTP 1.1 specification defines chunk-size as 1*HEX
// some sites rely on more lenient parsing.
// http://www.yahoo.com/, for example, pads chunk-size with trailing spaces
// (0x20) to be 7 characters long, such as "819b   ".
//
// A comparison of browsers running on WindowsXP shows that
// they will parse the following inputs (egrep syntax):
//
// Let \X be the character class for a hex digit: [0-9a-fA-F]
//
//   RFC 2616: ^\X+$
//        IE7: ^\X+[^\X]*$
// Safari 3.1: ^[\t\r ]*\X+[\t ]*$
//  Firefox 3: ^[\t\f\v\r ]*[+]?(0x)?\X+[^\X]*$
// Opera 9.51: ^[\t\f\v ]*[+]?(0x)?\X+[^\X]*$
//
// Our strategy is to be as strict as possible, while not breaking
// known sites.
//
//         Us: ^\X+[ ]*$
bool HttpChunkedDecoder::ParseChunkSize(const char* start, int len, int* out) {
  DCHECK(len >= 0);

  // Strip trailing spaces
  while (len && start[len - 1] == ' ')
    len--;

  // Be more restrictive than HexStringToInt;
  // don't allow inputs with leading "-", "+", "0x", "0X"
  if (StringPiece(start, len).find_first_not_of("0123456789abcdefABCDEF")!=
      StringPiece::npos)
    return false;
  return HexStringToInt(std::string(start, len), out);
}

}  // namespace net
