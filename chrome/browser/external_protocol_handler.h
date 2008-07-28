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

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_HANDLER_H__
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_HANDLER_H__

#include "chrome/common/pref_service.h"

class GURL;
class MessageLoop;

class ExternalProtocolHandler {
 public:
  enum BlockState {
    DONT_BLOCK,
    BLOCK,
    UNKNOWN,
  };

  // Returns whether we should block a given scheme.
  static BlockState GetBlockState(const std::wstring& scheme);

  // Checks to see if the protocol is allowed, if it is whitelisted,
  // the application associated with the protocol is launched on the io thread,
  // if it is blacklisted, returns silently. Otherwise, an
  // ExternalProtocolDialog is created asking the user. If the user accepts,
  // LaunchUrlWithoutSecurityCheck is called on the io thread and the
  // application is launched.
  // Must run on the UI thread.
  static void LaunchUrl(const GURL& url, int render_process_host_id,
                        int tab_contents_id);

  // Register the ExcludedSchemes preference.
  static void RegisterPrefs(PrefService* prefs);

  // Starts a url using the external protocol handler with the help
  // of shellexecute. Should only be called if the protocol is whitelisted
  // (checked in LaunchUrl) or if the user explicitly allows it. (By selecting
  // "Launch Application" in an ExternalProtocolDialog.) It is assumed that the
  // url has already been escaped, which happens in LaunchUrl.
  // NOTE: You should Not call this function directly unless you are sure the
  // url you have has been checked against the blacklist, and has been escaped.
  // All calls to this function should originate in some way from LaunchUrl.
  // Must run on the IO thread.
  static void LaunchUrlWithoutSecurityCheck(const GURL& url);

  // Prepopulates the dictionary with known protocols to deny or allow, if
  // preferences for them do not already exist.
  static void PrepopulateDictionary(DictionaryValue* win_pref);
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_HANDLER_H__
