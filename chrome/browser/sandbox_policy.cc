// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sandbox_policy.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/win_util.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace {

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the renderer.
const wchar_t* const kTroublesomeDlls[] = {
  L"adialhk.dll",                 // Kaspersky Internet Security.
  L"acpiz.dll",                   // Unknown.
  L"avgrsstx.dll",                // AVG 8.
  L"btkeyind.dll",                // Widcomm Bluetooth.
  L"cmcsyshk.dll",                // CMC Internet Security.
  L"dockshellhook.dll",           // Stardock Objectdock.
  L"GoogleDesktopNetwork3.DLL",   // Google Desktop Search v5.
  L"fwhook.dll",                  // PC Tools Firewall Plus.
  L"hookprocesscreation.dll",     // Blumentals Program protector.
  L"hookterminateapis.dll",       // Blumentals and Cyberprinter.
  L"hookprintapis.dll",           // Cyberprinter.
  L"imon.dll",                    // NOD32 Antivirus.
  L"ioloHL.dll",                  // Iolo (System Mechanic).
  L"kloehk.dll",                  // Kaspersky Internet Security.
  L"lawenforcer.dll",             // Spyware-Browser AntiSpyware (Spybro).
  L"libdivx.dll",                 // DivX.
  L"lvprcinj01.dll",              // Logitech QuickCam.
  L"madchook.dll",                // Madshi (generic hooking library).
  L"mdnsnsp.dll",                 // Bonjour.
  L"moonsysh.dll",                // Moon Secure Antivirus.
  L"npdivx32.dll",                // DivX.
  L"npggNT.des",                  // GameGuard 2008.
  L"npggNT.dll",                  // GameGuard (older).
  L"oawatch.dll",                 // Online Armor.
  L"pavhook.dll",                 // Panda Internet Security.
  L"pavshook.dll",                // Panda Antivirus.
  L"pctavhook.dll",               // PC Tools Antivirus.
  L"prntrack.dll",                // Pharos Systems.
  L"radhslib.dll",                // Radiant Naomi Internet Filter.
  L"radprlib.dll",                // Radiant Naomi Internet Filter.
  L"rlhook.dll",                  // Trustware Bufferzone.
  L"r3hook.dll",                  // Kaspersky Internet Security.
  L"sahook.dll",                  // McAfee Site Advisor.
  L"sbrige.dll",                  // Unknown.
  L"sc2hook.dll",                 // Supercopier 2.
  L"sguard.dll",                  // Iolo (System Guard).
  L"smumhook.dll",                // Spyware Doctor version 5 and above.
  L"ssldivx.dll",                 // DivX.
  L"syncor11.dll",                // SynthCore Midi interface.
  L"systools.dll",                // Panda Antivirus.
  L"tfwah.dll",                   // Threatfire (PC tools).
  L"wblind.dll",                  // Stardock Object desktop.
  L"wbhelp.dll",                  // Stardock Object desktop.
  L"winstylerthemehelper.dll"     // Tuneup utilities 2006.
};

}  // namespace

PluginPolicyCategory GetPolicyCategoryForPlugin(
    const FilePath& dll,
    const std::wstring& clsid,
    const std::wstring& list) {
  std::wstring filename = dll.BaseName().value();
  std::wstring plugin_dll = StringToLowerASCII(filename);
  std::wstring trusted_plugins = StringToLowerASCII(list);
  std::wstring activex_clsid = StringToLowerASCII(clsid);

  size_t pos = 0;
  size_t end_item = 0;
  while (end_item != std::wstring::npos) {
    end_item = list.find(L",", pos);

    size_t size_item = (end_item == std::wstring::npos) ? end_item :
                                                          end_item - pos;
    std::wstring item = list.substr(pos, size_item);
    if (!item.empty()) {
      if (item == activex_clsid || item == plugin_dll)
        return PLUGIN_GROUP_TRUSTED;
    }

    pos = end_item + 1;
  }

  return PLUGIN_GROUP_UNTRUSTED;
}

// Adds the policy rules for the path and path\* with the semantic |access|.
// We need to add the wildcard rules to also apply the rule to the subfiles
// and subfolders.
bool AddDirectoryAndChildren(int path, const wchar_t* sub_dir,
                             sandbox::TargetPolicy::Semantics access,
                             sandbox::TargetPolicy* policy) {
  std::wstring directory;
  if (!PathService::Get(path, &directory))
    return false;

  if (sub_dir)
    file_util::AppendToPath(&directory, sub_dir);

  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES, access,
                           directory.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  file_util::AppendToPath(&directory, L"*");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES, access,
                           directory.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  return true;
}

// Adds the policy rules for the path and path\* with the semantic |access|.
// We need to add the wildcard rules to also apply the rule to the subkeys.
bool AddKeyAndSubkeys(std::wstring key,
                      sandbox::TargetPolicy::Semantics access,
                      sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY, access,
                           key.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  key += L"\\*";
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_REGISTRY, access,
                           key.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  return true;
}

// Eviction of injected DLLs is done by the sandbox so that the injected module
// does not get a chance to execute any code.
bool AddDllEvictionPolicy(sandbox::TargetPolicy* policy) {
  for (int ix = 0; ix != arraysize(kTroublesomeDlls); ++ix) {
    // To minimize the list we only add an unload policy if the dll is also
    // loaded in this process. All the injected dlls of interest do this.
    if (::GetModuleHandleW(kTroublesomeDlls[ix])) {
      LOG(WARNING) << "dll to unload found: " << kTroublesomeDlls[ix];
      if (sandbox::SBOX_ALL_OK != policy->AddDllToUnload(kTroublesomeDlls[ix]))
        return false;
    }
  }

  return true;
}

bool AddPolicyForGearsInRenderer(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;

  // TODO(mpcomplete): need to restrict access to database files only.  This
  // is just temporary for debugging purposes.
  std::wstring plugin_data;
  if (!PathService::Get(chrome::DIR_USER_DATA, &plugin_data))
    return false;
  if (!win_util::ConvertToLongPath(plugin_data, &plugin_data))
    return false;

  file_util::AppendToPath(&plugin_data, L"*");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           plugin_data.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  std::wstring temppath;
  if (!file_util::GetTempDir(&temppath))
    return false;
  file_util::AppendToPath(&temppath, L"*");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           temppath.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  return true;
}

bool AddGenericPolicy(sandbox::TargetPolicy* policy) {
  sandbox::ResultCode result;

  // Add the policy for the pipes
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                           sandbox::TargetPolicy::FILES_ALLOW_ANY,
                           L"\\??\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

#ifdef IPC_MESSAGE_LOG_ENABLED
  // Add the policy for the IPC logging events.
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                           sandbox::TargetPolicy::EVENTS_ALLOW_ANY,
                           IPC::Logging::GetEventName(true).c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_SYNC,
                           sandbox::TargetPolicy::EVENTS_ALLOW_ANY,
                           IPC::Logging::GetEventName(false).c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif

  // Add the policy for debug message only in debug
#ifndef NDEBUG
  std::wstring debug_message;
  if (!PathService::Get(chrome::DIR_APP, &debug_message))
    return false;
  if (!win_util::ConvertToLongPath(debug_message, &debug_message))
    return false;
  file_util::AppendToPath(&debug_message, L"debug_message.exe");
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_PROCESS,
                           sandbox::TargetPolicy::PROCESS_MIN_EXEC,
                           debug_message.c_str());
  if (result != sandbox::SBOX_ALL_OK)
    return false;
#endif  // NDEBUG

  return true;
}

bool ApplyPolicyForTrustedPlugin(sandbox::TargetPolicy* policy) {
  policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);
  policy->SetTokenLevel(sandbox::USER_UNPROTECTED, sandbox::USER_UNPROTECTED);
  return true;
}

bool ApplyPolicyForUntrustedPlugin(sandbox::TargetPolicy* policy) {
  policy->SetJobLevel(sandbox::JOB_UNPROTECTED, 0);

  sandbox::TokenLevel initial_token = sandbox::USER_UNPROTECTED;
  if (win_util::GetWinVersion() > win_util::WINVERSION_XP) {
    // On 2003/Vista the initial token has to be restricted if the main token
    // is restricted.
    initial_token = sandbox::USER_RESTRICTED_SAME_ACCESS;
  }
  policy->SetTokenLevel(initial_token, sandbox::USER_LIMITED);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

  if (!AddDirectoryAndChildren(base::DIR_TEMP, NULL,
                               sandbox::TargetPolicy::FILES_ALLOW_ANY, policy))
    return false;

  if (!AddDirectoryAndChildren(base::DIR_IE_INTERNET_CACHE, NULL,
                               sandbox::TargetPolicy::FILES_ALLOW_ANY, policy))
    return false;


  if (!AddDirectoryAndChildren(base::DIR_APP_DATA, NULL,
                               sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                               policy))
    return false;

  if (!AddDirectoryAndChildren(base::DIR_APP_DATA, L"Macromedia",
                               sandbox::TargetPolicy::FILES_ALLOW_ANY,
                               policy))
    return false;

  if (!AddDirectoryAndChildren(base::DIR_LOCAL_APP_DATA, NULL,
                               sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                               policy))
    return false;

  if (!AddKeyAndSubkeys(L"HKEY_CURRENT_USER\\SOFTWARE\\MACROMEDIA",
                        sandbox::TargetPolicy::REG_ALLOW_ANY,
                        policy))
    return false;

  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
    if (!AddKeyAndSubkeys(L"HKEY_CURRENT_USER\\SOFTWARE\\AppDataLow",
                          sandbox::TargetPolicy::REG_ALLOW_ANY,
                          policy))
      return false;

    if (!AddDirectoryAndChildren(base::DIR_LOCAL_APP_DATA_LOW, NULL,
                                 sandbox::TargetPolicy::FILES_ALLOW_ANY,
                                 policy))
      return false;
  }

  return true;
}

bool AddPolicyForPlugin(const FilePath &plugin_dll,
                        const std::string &activex_clsid,
                        const std::wstring &trusted_plugins,
                        sandbox::TargetPolicy* policy) {
  // Add the policy for the pipes.
  sandbox::ResultCode result = sandbox::SBOX_ALL_OK;
  result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
                           sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                           L"\\\\.\\pipe\\chrome.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  std::wstring clsid = UTF8ToWide(activex_clsid);

  PluginPolicyCategory policy_category =
      GetPolicyCategoryForPlugin(plugin_dll, clsid, trusted_plugins);

  switch (policy_category) {
    case PLUGIN_GROUP_TRUSTED:
      return ApplyPolicyForTrustedPlugin(policy);
    case PLUGIN_GROUP_UNTRUSTED:
      return ApplyPolicyForUntrustedPlugin(policy);
    default:
      NOTREACHED();
      break;
  }

  return false;
}
