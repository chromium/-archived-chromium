// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SANDBOX_POLICY_H_
#define CHROME_BROWSER_SANDBOX_POLICY_H_

#include <string>

#include "base/file_path.h"
#include "sandbox/src/sandbox.h"

// Adds the generic policy rules to a sandbox TargetPolicy.
bool AddGenericPolicy(sandbox::TargetPolicy* policy);

// Adds policy rules for unloaded the known dlls that cause chrome to crash.
bool AddDllEvictionPolicy(sandbox::TargetPolicy* policy);

// Adds the custom policy rules for a given plugin. If dll is activex-shim,
// then clsid is the clsid of ActiveX control. Otherwise clsid is ignored.
// |trusted_plugins| contains the comma separate list of plugins that should
// not be sandboxed. The plugin in the list can be either the plugin dll name
// of the class id if it's an ActiveX.
bool AddPolicyForPlugin(const FilePath &plugin_dll,
                        const std::string &activex_clsid,
                        const std::wstring &trusted_plugins,
                        sandbox::TargetPolicy* policy);

enum PluginPolicyCategory {
  PLUGIN_GROUP_TRUSTED,
  PLUGIN_GROUP_UNTRUSTED,
};

// Returns the policy category for the plugin dll.
PluginPolicyCategory GetPolicyCategoryForPlugin(
    const FilePath& plugin_dll,
    const std::wstring& activex_clsid,
    const std::wstring& trusted_plugins);

// Creates a sandbox without any restriction.
bool ApplyPolicyForTrustedPlugin(sandbox::TargetPolicy* policy);

// Creates a sandbox with the plugin running in a restricted environment.
// Only the "Users" and "Everyone" groups are enabled in the token. The User SID
// is disabled.
bool ApplyPolicyForUntrustedPlugin(sandbox::TargetPolicy* policy);

#endif  // CHROME_BROWSER_SANDBOX_POLICY_H_
