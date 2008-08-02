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

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/resource_bundle.h"
#include "chrome/plugin/npobject_util.h"
#include "googleurl/src/url_util.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit_glue {

bool GetExeDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool GetApplicationDirectory(std::wstring *path) {
  return PathService::Get(chrome::DIR_APP, path);
}

bool IsPluginRunningInRendererProcess() {
  return !IsPluginProcess();
}

std::wstring GetWebKitLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  CommandLine parsed_command_line;
  const std::wstring& lang =
      parsed_command_line.GetSwitchValue(switches::kLang);
  DCHECK(!lang.empty());
  return lang;
}

std::wstring GetLocalizedString(int message_id) {
  return ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
}

}  // namespace webkit_glue
