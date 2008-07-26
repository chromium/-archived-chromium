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
#include "net/base/net_errors.h"

namespace net {

HttpChunkedDecoder::HttpChunkedDecoder()
    : chunk_remaining_(0),
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
      continue;
    } else if (reached_eof_) {
      break;  // Done!
    }

    // Returns bytes consumed or an error code.
    int rv = ScanForChunkRemaining(buf, buf_len);
    if (rv < 0)
      return rv;

    buf_len -= rv;
    if (buf_len)
      memmove(buf, buf + rv, buf_len);
  }

  return result;
}

int HttpChunkedDecoder::ScanForChunkRemaining(char* buf, int buf_len) {
  DCHECK(chunk_remaining_ == 0);
  DCHECK(buf_len > 0);

  int result = 0;

  char *p = static_cast<char*>(memchr(buf, '\n', buf_len));
  if (p) {
    *p = 0;
    if ((p > buf) && (*(p - 1) == '\r'))  // Eliminate a preceding CR.
      *(p - 1) = 0;
    result = static_cast<int>(p - buf) + 1;

    // Make buf point to the full line buffer to parse.
    if (!line_buf_.empty()) {
      line_buf_.append(buf);
      buf = const_cast<char*>(line_buf_.data());
    }

    if (reached_last_chunk_) {
      if (*buf) {
        DLOG(INFO) << "ignoring http trailer";
      } else {
        reached_eof_ = true;
      }
    } else if (*buf) {
      // Ignore any chunk-extensions.
      if ((p = strchr(buf, ';')) != NULL)
        *p = 0;

      if (!sscanf_s(buf, "%x", &chunk_remaining_)) {
        DLOG(ERROR) << "sscanf failed parsing HEX from: " << buf;
        return ERR_FAILED;
      }

      if (chunk_remaining_ == 0)
        reached_last_chunk_ = true;
    }
    line_buf_.clear();
  } else {
    // Save the partial line; wait for more data.
    result = buf_len;

    // Ignore a trailing CR
    if (buf[buf_len - 1] == '\r')
      buf_len--;

    line_buf_.append(buf, buf_len);
  }
  return result;
}

}  // namespace net
