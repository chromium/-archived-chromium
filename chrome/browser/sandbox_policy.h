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

#ifndef CHROME_BROWSER_SANDBOX_POLICY_H__
#define CHROME_BROWSER_SANDBOX_POLICY_H__

#include <string>
#include "sandbox/src/sandbox.h"
#include "webkit/activex_shim/activex_shared.h"

// Adds the generic policy rules to a sandbox TargetPolicy.
bool AddGenericPolicy(sandbox::TargetPolicy* policy);

// Adds the custom policy rules for a given plugin. If dll is activex-shim,
// then clsid is the clsid of ActiveX control. Otherwise clsid is ignored.
// |trusted_plugins| contains the comma separate list of plugins that should
// not be sandboxed. The plugin in the list can be either the plugin dll name
// of the class id if it's an ActiveX.
bool AddPolicyForPlugin(const std::wstring &plugin_dll,
                        const std::string &activex_clsid,
                        const std::wstring &trusted_plugins,
                        sandbox::TargetPolicy* policy);

enum PluginPolicyCategory {
  PLUGIN_GROUP_TRUSTED,
  PLUGIN_GROUP_UNTRUSTED,
};

// Returns the policy category for the plugin dll.
PluginPolicyCategory GetPolicyCategoryForPlugin(
    const std::wstring& plugin_dll,
    const std::wstring& activex_clsid,
    const std::wstring& trusted_plugins);

// Creates a sandbox without any restriction.
bool ApplyPolicyForTrustedPlugin(sandbox::TargetPolicy* policy);

// Creates a sandbox with the plugin running in a restricted environment.
// Only the "Users" and "Everyone" groups are enabled in the token. The User SID
// is disabled.
bool ApplyPolicyForUntrustedPlugin(sandbox::TargetPolicy* policy);

#endif  // CHROME_BROWSER_SANDBOX_POLICY_H__
