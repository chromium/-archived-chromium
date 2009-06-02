// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/force_tls_state.h"

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace net {

ForceTLSState::ForceTLSState() {
}

void ForceTLSState::DidReceiveHeader(const GURL& url,
                                     const std::string& value) {
  // TODO(abarth): Actually parse |value| once the spec settles down.
  EnableHost(url.host());
}

void ForceTLSState::EnableHost(const std::string& host) {
  // TODO(abarth): Canonicalize host.
  AutoLock lock(lock_);
  enabled_hosts_.insert(host);
}

bool ForceTLSState::IsEnabledForHost(const std::string& host) {
  // TODO(abarth): Canonicalize host.
  AutoLock lock(lock_);
  return enabled_hosts_.find(host) != enabled_hosts_.end();
}

// "X-Force-TLS" ":" "max-age" "=" delta-seconds *1INCLUDESUBDOMAINS
// INCLUDESUBDOMAINS = [ " includeSubDomains" ]
bool ForceTLSState::ParseHeader(const std::string& value,
                                int* max_age,
                                bool* include_subdomains) {
  DCHECK(max_age);
  DCHECK(include_subdomains);

  int max_age_candidate;

  enum ParserState {
    START,
    AFTER_MAX_AGE_LABEL,
    AFTER_MAX_AGE_EQUALS,
    AFTER_MAX_AGE,
    AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER,
    AFTER_INCLUDE_SUBDOMAINS,
  } state = START;

  StringTokenizer tokenizer(value, " =");
  tokenizer.set_options(StringTokenizer::RETURN_DELIMS);
  while (tokenizer.GetNext()) {
    DCHECK(!tokenizer.token_is_delim() || tokenizer.token().length() == 1);
    DCHECK(tokenizer.token_is_delim() || *tokenizer.token_begin() != ' ');
    switch (state) {
      case START:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (!LowerCaseEqualsASCII(tokenizer.token(), "max-age"))
          return false;
        state = AFTER_MAX_AGE_LABEL;
        break;

      case AFTER_MAX_AGE_LABEL:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (*tokenizer.token_begin() != '=')
          return false;
        DCHECK(tokenizer.token().length() ==  1);
        state = AFTER_MAX_AGE_EQUALS;
        break;

      case AFTER_MAX_AGE_EQUALS:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (!StringToInt(tokenizer.token(), &max_age_candidate))
          return false;
        if (max_age_candidate < 0)
          return false;
        state = AFTER_MAX_AGE;
        break;

      case AFTER_MAX_AGE:
        if (*tokenizer.token_begin() != ' ')
          return false;
        state = AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER;
        break;

      case AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER:
        if (*tokenizer.token_begin() == ' ')
          continue;
        if (!LowerCaseEqualsASCII(tokenizer.token(), "includesubdomains"))
          return false;
        state = AFTER_INCLUDE_SUBDOMAINS;
        break;

      case AFTER_INCLUDE_SUBDOMAINS:
        if (*tokenizer.token_begin() != ' ')
          return false;
        break;

      default:
        NOTREACHED();
    }
  }

  // We've consumed all the input.  Let's see what state we ended up in.
  switch (state) {
    case START:
    case AFTER_MAX_AGE_LABEL:
    case AFTER_MAX_AGE_EQUALS:
      return false;
    case AFTER_MAX_AGE:
    case AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER:
      *max_age = max_age_candidate;
      *include_subdomains = false;
      return true;
    case AFTER_INCLUDE_SUBDOMAINS:
      *max_age = max_age_candidate;
      *include_subdomains = true;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

}  // namespace
