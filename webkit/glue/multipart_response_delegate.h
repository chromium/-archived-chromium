// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A delegate class of WebURLLoaderImpl that handles multipart/x-mixed-replace
// data.  We special case multipart/x-mixed-replace because WebCore expects a
// separate didReceiveResponse for each new message part.
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

#ifndef WEBKIT_GLUE_MULTIPART_RESPONSE_DELEGATE_H_
#define WEBKIT_GLUE_MULTIPART_RESPONSE_DELEGATE_H_

#include <string>

#include "webkit/api/public/WebURLResponse.h"

namespace WebKit {
class WebURLLoader;
class WebURLLoaderClient;
}

namespace webkit_glue {

// Used by unit tests to access private members.
class MultipartResponseDelegateTester;

class MultipartResponseDelegate {
 public:
  MultipartResponseDelegate(WebKit::WebURLLoaderClient* client,
                            WebKit::WebURLLoader* loader,
                            const WebKit::WebURLResponse& response,
                            const std::string& boundary);

  // Passed through from ResourceHandleInternal
  void OnReceivedData(const char* data, int data_len);
  void OnCompletedRequest();

  // Returns the multi part boundary string from the Content-type header
  // in the response.
  // Returns true on success.
  static bool ReadMultipartBoundary(const WebKit::WebURLResponse& response,
                                    std::string* multipart_boundary);

  // Returns the lower and higher content ranges from an individual multipart
  // in a multipart response.
  // Returns true on success.
  static bool ReadContentRanges(const WebKit::WebURLResponse& response,
                                int* content_range_lower_bound,
                                int* content_range_upper_bound);

 private:
  friend class MultipartResponseDelegateTester;  // For unittests.

  // Pointers to the client and associated loader so we can make callbacks as
  // we parse pieces of data.
  WebKit::WebURLLoaderClient* client_;
  WebKit::WebURLLoader* loader_;

  // The original resource response for this request.  We use this as a
  // starting point for each parts response.
  WebKit::WebURLResponse original_response_;

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

}  // namespace webkit_glue

#endif
