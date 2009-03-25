// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <atlbase.h>
#include <windows.h>
#include <msi.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/wmi_util.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/util_constants.h"

#include "installer_util_strings.h"

namespace {
// Substitute the locale parameter in uninstall URL with whatever
// Google Update tells us is the locale. In case we fail to find
// the locale, we use US English.
std::wstring GetUninstallSurveyUrl() {
  std::wstring kSurveyUrl = L"http://www.google.com/support/chrome/bin/"
                            L"request.py?hl=$1&contact_type=uninstall";

  std::wstring language;
  if (!GoogleUpdateSettings::GetLanguage(&language))
    language = L"en-US";  // Default to US English.

  return ReplaceStringPlaceholders(kSurveyUrl.c_str(), language.c_str(), NULL);
}
}

bool GoogleChromeDistribution::BuildUninstallMetricsString(
    DictionaryValue* uninstall_metrics_dict, std::wstring* metrics) {
  DCHECK(NULL != metrics);
  bool has_values = false;

  DictionaryValue::key_iterator iter(uninstall_metrics_dict->begin_keys());
  for (; iter != uninstall_metrics_dict->end_keys(); ++iter) {
    has_values = true;
    metrics->append(L"&");
    metrics->append(*iter);
    metrics->append(L"=");

    std::wstring value;
    uninstall_metrics_dict->GetString(*iter, &value);
    metrics->append(value);
  }

  return has_values;
}

bool GoogleChromeDistribution::ExtractUninstallMetricsFromFile(
    const std::wstring& file_path, std::wstring* uninstall_metrics_string) {

  JSONFileValueSerializer json_serializer(file_path);

  std::string json_error_string;
  scoped_ptr<Value> root(json_serializer.Deserialize(NULL));

  // Preferences should always have a dictionary root.
  if (!root->IsType(Value::TYPE_DICTIONARY))
    return false;

  return ExtractUninstallMetrics(*static_cast<DictionaryValue*>(root.get()),
                                 uninstall_metrics_string);
}

bool GoogleChromeDistribution::ExtractUninstallMetrics(
    const DictionaryValue& root, std::wstring* uninstall_metrics_string) {
  // Make sure that the user wants us reporting metrics. If not, don't
  // add our uninstall metrics.
  bool metrics_reporting_enabled = false;
  if (!root.GetBoolean(prefs::kMetricsReportingEnabled,
                       &metrics_reporting_enabled) ||
      !metrics_reporting_enabled) {
    return false;
  }

  DictionaryValue* uninstall_metrics_dict;
  if (!root.HasKey(installer_util::kUninstallMetricsName) ||
      !root.GetDictionary(installer_util::kUninstallMetricsName,
                          &uninstall_metrics_dict)) {
    return false;
  }

  if (!BuildUninstallMetricsString(uninstall_metrics_dict,
                                   uninstall_metrics_string)) {
    return false;
  }

  // We use the creation date of the metric's client id as the installation
  // date, as this is fairly close to the first-run date for those who
  // opt-in to metrics from the get-go. Slightly more accurate would be to
  // have setup.exe write something somewhere at installation time.
  // As is, the absence of this field implies that the browser was never
  // run with an opt-in to metrics selection.
  std::wstring installation_date;
  if (root.GetString(prefs::kMetricsClientIDTimestamp, &installation_date)) {
    uninstall_metrics_string->append(L"&");
    uninstall_metrics_string->append(
        installer_util::kUninstallInstallationDate);
    uninstall_metrics_string->append(L"=");
    uninstall_metrics_string->append(installation_date);
  }

  return true;
}

void GoogleChromeDistribution::DoPostUninstallOperations(
    const installer::Version& version, const std::wstring& local_data_path) {
  // Send the Chrome version and OS version as params to the form.
  // It would be nice to send the locale, too, but I don't see an
  // easy way to get that in the existing code. It's something we
  // can add later, if needed.
  // We depend on installed_version.GetString() not having spaces or other
  // characters that need escaping: 0.2.13.4. Should that change, we will
  // need to escape the string before using it in a URL.
  const std::wstring kVersionParam = L"crversion";
  const std::wstring kVersion = version.GetString();
  const std::wstring kOSParam = L"os";
  std::wstring os_version = L"na";
  OSVERSIONINFO version_info;
  version_info.dwOSVersionInfoSize = sizeof(version_info);
  if (GetVersionEx(&version_info)) {
    os_version = StringPrintf(L"%d.%d.%d", version_info.dwMajorVersion,
                                           version_info.dwMinorVersion,
                                           version_info.dwBuildNumber);
  }

  std::wstring iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  file_util::AppendToPath(&iexplore, L"Internet Explorer");
  file_util::AppendToPath(&iexplore, L"iexplore.exe");

  std::wstring command = iexplore + L" " + GetUninstallSurveyUrl() + L"&" +
      kVersionParam + L"=" + kVersion + L"&" + kOSParam + L"=" + os_version;

  std::wstring uninstall_metrics;
  if (ExtractUninstallMetricsFromFile(local_data_path, &uninstall_metrics)) {
    command += uninstall_metrics;
  }

  int pid = 0;
  WMIProcessUtil::Launch(command, &pid);
}

std::wstring GoogleChromeDistribution::GetApplicationName() {
  const std::wstring& product_name =
      installer_util::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
  return product_name;
}

std::wstring GoogleChromeDistribution::GetAlternateApplicationName() {
  // TODO(cpu): return the right localized strings when it arrives. 
  return L"";
}

std::wstring GoogleChromeDistribution::GetInstallSubDir() {
  return L"Google\\Chrome";
}

std::wstring GoogleChromeDistribution::GetNewGoogleUpdateApKey(
    bool diff_install, installer_util::InstallStatus status,
    const std::wstring& value) {
  // Magic suffix that we need to add or remove to "ap" key value.
  const std::wstring kMagicSuffix = L"-full";

  bool has_magic_string = false;
  if ((value.length() >= kMagicSuffix.length()) &&
      (value.rfind(kMagicSuffix) == (value.length() - kMagicSuffix.length()))) {
    LOG(INFO) << "Incremental installer failure key already set.";
    has_magic_string = true;
  }

  std::wstring new_value(value);
  if ((!diff_install || !GetInstallReturnCode(status)) && has_magic_string) {
    LOG(INFO) << "Removing failure key from value " << value;
    new_value = value.substr(0, value.length() - kMagicSuffix.length());
  } else if ((diff_install && GetInstallReturnCode(status)) &&
             !has_magic_string) {
    LOG(INFO) << "Incremental installer failed, setting failure key.";
    new_value.append(kMagicSuffix);
  }

  return new_value;
}

std::wstring GoogleChromeDistribution::GetPublisherName() {
  const std::wstring& publisher_name =
      installer_util::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

std::wstring GoogleChromeDistribution::GetAppDescription() {
  // TODO(cpu): Wire the actual localized strings when they arrive.
  return L"";
}

int GoogleChromeDistribution::GetInstallReturnCode(
    installer_util::InstallStatus status) {
  switch (status) {
    case installer_util::FIRST_INSTALL_SUCCESS:
    case installer_util::INSTALL_REPAIRED:
    case installer_util::NEW_VERSION_UPDATED:
    case installer_util::HIGHER_VERSION_EXISTS:
      return 0;  // For Google Update's benefit we need to return 0 for success
    default:
      return status;
  }
}

std::wstring GoogleChromeDistribution::GetStateKey() {
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(google_update::kChromeGuid);
  return key;
}

std::wstring GoogleChromeDistribution::GetUninstallLinkName() {
  const std::wstring& link_name =
      installer_util::GetLocalizedString(IDS_UNINSTALL_CHROME_BASE);
  return link_name;
}

std::wstring GoogleChromeDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome";
}

std::wstring GoogleChromeDistribution::GetVersionKey() {
  std::wstring key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(google_update::kChromeGuid);
  return key;
}

// This method checks if we need to change "ap" key in Google Update to try
// full installer as fall back method in case incremental installer fails.
// - If incremental installer fails we append a magic string ("-full"), if
// it is not present already, so that Google Update server next time will send
// full installer to update Chrome on the local machine
// - If we are currently running full installer, we remove this magic
// string (if it is present) regardless of whether installer failed or not.
// There is no fall-back for full installer :)
void GoogleChromeDistribution::UpdateDiffInstallStatus(bool system_install,
    bool incremental_install, installer_util::InstallStatus install_status) {
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring ap_key_value;
  std::wstring reg_key(google_update::kRegPathClientState);
  reg_key.append(L"\\");
  reg_key.append(google_update::kChromeGuid);
  if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS) ||
      !key.ReadValue(google_update::kRegApField, &ap_key_value)) {
    LOG(INFO) << "Application key not found.";
    if (!incremental_install || !GetInstallReturnCode(install_status)) {
      LOG(INFO) << "Returning without changing application key.";
      key.Close();
      return;
    } else if (!key.Valid()) {
      reg_key.assign(google_update::kRegPathClientState);
      if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS) ||
          !key.CreateKey(google_update::kChromeGuid, KEY_ALL_ACCESS)) {
        LOG(ERROR) << "Failed to create application key.";
        key.Close();
        return;
      }
    }
  }

  std::wstring new_value = GoogleChromeDistribution::GetNewGoogleUpdateApKey(
      incremental_install, install_status, ap_key_value);
  if ((new_value.compare(ap_key_value) != 0) &&
      !key.WriteValue(google_update::kRegApField, new_value.c_str())) {
    LOG(ERROR) << "Failed to write value " << new_value
               << " to the registry field " << google_update::kRegApField;
  }
  key.Close();
}
