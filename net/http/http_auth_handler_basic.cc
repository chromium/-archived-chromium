// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_basic.h"

#include "base/string_util.h"
#include "net/http/http_auth.h"
#include "net/base/base64.h"

namespace net {

bool HttpAuthHandlerBasic::Init(std::string::const_iterator challenge_begin,
                                std::string::const_iterator challenge_end) {
  scheme_ = "basic";
  score_ = 1;
  properties_ = 0;

  // Verify the challenge's auth-scheme.
  HttpAuth::ChallengeTokenizer challenge_tok(challenge_begin, challenge_end);
  if (!challenge_tok.valid() ||
      !LowerCaseEqualsASCII(challenge_tok.scheme(), "basic"))
    return false;

  // Extract the realm.
  while (challenge_tok.GetNext()) {
    if (LowerCaseEqualsASCII(challenge_tok.name(), "realm"))
      realm_ = challenge_tok.unquoted_value();
  }

  return challenge_tok.valid() && !realm_.empty();
}

std::string HttpAuthHandlerBasic::GenerateCredentials(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo*,
    const ProxyInfo*) {
  // TODO(eroman): is this the right encoding of username/password?
  std::string base64_username_password;
  if (!Base64Encode(WideToUTF8(username) + ":" + WideToUTF8(password),
        &base64_username_password))
    return std::string(); // FAIL
  return std::string("Basic ") + base64_username_password;
}

}  // namespace net
