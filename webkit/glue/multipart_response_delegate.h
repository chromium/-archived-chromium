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
//
// A delegate class of ResourceHandleInternal (resource_handle_win) that
// handles multipart/x-mixed-replace data.  We special case
// multipart/x-mixed-replace because WebCore expects a separate
// didReceiveResponse for each new message part.
//
// Most of the logic and edge case handling are based on the Mozilla's
// implementation in netwerk/streamconv/converters/nsMultiMixedConv.cpp.
// This seems like a derivative work, so here's the original license:

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <string>

#include "config.h"
#pragma warning(push, 0)
#include "ResourceResponse.h"
#pragma warning(pop)

namespace WebCore {
  class ResourceHandle;
  class ResourceHandleClient;
}

class MultipartResponseDelegate {
  friend class MultipartResponseTest_Functions_Test;  // For unittests.

 public:
  MultipartResponseDelegate(WebCore::ResourceHandleClient* client,
                            WebCore::ResourceHandle* job,
                            const WebCore::ResourceResponse& response,
                            const std::string& boundary);

  // Passed through from ResourceHandleInternal
  void OnReceivedData(const char* data, int data_len);
  void OnCompletedRequest();

 private:
  // Pointers back to our owning object so we can make callbacks as we parse
  // pieces of data.
  WebCore::ResourceHandleClient* client_;
  WebCore::ResourceHandle* job_;

  // The original resource response for this request.  We use this as a
  // starting point for each parts response.
  WebCore::ResourceResponse original_response_;

  // Checks to see if data[pos] character is a line break; handles crlf, lflf,
  // lf, or cr. Returns the number of characters to skip over (0, 1 or 2).
  int PushOverLine(const std::string& data, size_t pos);

  // Tries to parse http headers from the start of data_.  Returns true if it
  // succeeds and sends a didReceiveResponse to m_client.  Returns false if
  // the header is incomplete (in which case we just wait for more data).
  bool ParseHeaders();

  // Find the next boundary in data_.  Returns std::string::npos if there's no
  // full token.
  size_t FindBoundary();

  // A temporary buffer to hold data between reads for multipart data that
  // gets split in the middle of a header.
  std::string data_;

  // Multipart boundary token
  std::string boundary_;

  // true until we get our first on received data call
  bool first_received_data_;

  // true if we're truncated in the middle of a header
  bool processing_headers_;

  // true when we're done sending information.  At that point, we stop
  // processing AddData requests.
  bool stop_sending_;
};
