// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "net/http/http_auth_handler.h"

namespace net {

class NTLMAuthModule;

// Code for handling HTTP NTLM authentication.
class HttpAuthHandlerNTLM : public HttpAuthHandler {
 public:
  // A function that generates n random bytes in the output buffer.
  typedef void (*GenerateRandomProc)(uint8* output, size_t n);

  // A function that returns the local host name as a null-terminated string
  // in the output buffer. Returns an empty string if the local host name is
  // not available.
  // TODO(wtc): return a std::string instead.
  typedef void (*HostNameProc)(char* name, size_t namelen);

  HttpAuthHandlerNTLM();

  virtual ~HttpAuthHandlerNTLM();

  virtual bool NeedsIdentity();

  virtual std::string GenerateCredentials(const std::wstring& username,
                                          const std::wstring& password,
                                          const HttpRequestInfo* request,
                                          const ProxyInfo* proxy);

  // For unit tests to override the GenerateRandom and GetHostName functions.
  static void SetGenerateRandomProc(GenerateRandomProc proc);
  static void SetHostNameProc(HostNameProc proc);

 protected:
  virtual bool Init(std::string::const_iterator challenge_begin,
                    std::string::const_iterator challenge_end) {
    return ParseChallenge(challenge_begin, challenge_end);
  }

 private:
  // Parse the challenge, saving the results into this instance.
  // Returns true on success.
  bool ParseChallenge(std::string::const_iterator challenge_begin,
                      std::string::const_iterator challenge_end);

  // The actual implementation of NTLM.
  //
  // TODO(wtc): This artificial separation of the NTLM auth module from the
  // NTLM auth handler comes from the Mozilla code.  It is due to an
  // architecture constraint of Mozilla's (all crypto code must reside in the
  // "PSM" component), so that the NTLM code, which does crypto, must be
  // separated from the "netwerk" component.  Our source tree doesn't have
  // this constraint, so we may want to merge NTLMAuthModule into this class.
  scoped_ptr<NTLMAuthModule> ntlm_module_;

  // The base64-encoded string following "NTLM" in the "WWW-Authenticate" or
  // "Proxy-Authenticate" response header.
  std::string auth_data_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_NTLM_H_
