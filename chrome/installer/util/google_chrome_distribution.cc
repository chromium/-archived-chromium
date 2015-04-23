// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include "chrome/installer/util/google_chrome_distribution.h"

#include <atlbase.h>
#include <windows.h>
#include <msi.h>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/wmi_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/result_codes.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/util_constants.h"

#include "installer_util_strings.h"

namespace {
// The following strings are the possible outcomes of the toast experiment
// as recorded in the  |client| field.
const wchar_t kToastExpBaseGroup[] =         L"TS00";
const wchar_t kToastExpQualifyGroup[] =      L"TS01";
const wchar_t kToastExpCancelGroup[] =       L"TS02";
const wchar_t kToastExpUninstallGroup[] =    L"TS04";
const wchar_t kToastExpTriesOkGroup[] =      L"TS18";
const wchar_t kToastExpTriesErrorGroup[] =   L"TS28";

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

// Converts FILETIME to hours. FILETIME times are absolute times in
// 100 nanosecond units. For example 5:30 pm of June 15, 2009 is 3580464.
int FileTimeToHours(const FILETIME& time) {
  const ULONGLONG k100sNanoSecsToHours = 10000000LL * 60 * 60;
  ULARGE_INTEGER uli = {time.dwLowDateTime, time.dwHighDateTime};
  return static_cast<int>(uli.QuadPart / k100sNanoSecsToHours);
}

// Returns the directory last write time in hours since January 1, 1601.
// Returns -1 if there was an error retrieving the directory time.
int GetDirectoryWriteTimeInHours(const wchar_t* path) {
  // To open a directory you need to pass FILE_FLAG_BACKUP_SEMANTICS.
  DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  HANDLE file = ::CreateFileW(path, 0, share, NULL, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (INVALID_HANDLE_VALUE == file)
    return -1;
  FILETIME time;
  if (!::GetFileTime(file, NULL, NULL, &time))
    return -1;
  return FileTimeToHours(time);
}

// Returns the directory last-write time age in hours, relative to current
// time, so if it returns 14 it means that the directory was last written 14
// hours ago. Returns -1 if there was an error retrieving the directory.
int GetDirectoryWriteAgeInHours(const wchar_t* path) {
  int dir_time = GetDirectoryWriteTimeInHours(path);
  if (dir_time < 0)
    return dir_time;
  FILETIME time;
  GetSystemTimeAsFileTime(&time);
  int now_time = FileTimeToHours(time);
  if (dir_time >= now_time)
    return 0;
  return (now_time - dir_time);
}

// Launches again this same process with a single switch --|flag|. Does not
// wait for the process to terminate.
bool RelaunchSetup(const std::wstring& flag) {
  CommandLine cmd_line(CommandLine::ForCurrentProcess()->program());
  cmd_line.AppendSwitch(flag);
  return base::LaunchApp(cmd_line, false, false, NULL);
}

}  // namespace

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

  JSONFileValueSerializer json_serializer(FilePath::FromWStringHack(file_path));

  std::string json_error_string;
  scoped_ptr<Value> root(json_serializer.Deserialize(NULL));
  if (!root.get())
    return false;

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

  return true;
}

void GoogleChromeDistribution::DoPostUninstallOperations(
    const installer::Version& version, const std::wstring& local_data_path,
    const std::wstring& distribution_data) {
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

  FilePath iexplore;
  if (!PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  std::wstring command = iexplore.value() + L" " + GetUninstallSurveyUrl() +
      L"&" + kVersionParam + L"=" + kVersion + L"&" + kOSParam + L"=" +
      os_version;

  std::wstring uninstall_metrics;
  if (ExtractUninstallMetricsFromFile(local_data_path, &uninstall_metrics)) {
    // The user has opted into anonymous usage data collection, so append
    // metrics and distribution data.
    command += uninstall_metrics;
    if (!distribution_data.empty()) {
      command += L"&";
      command += distribution_data;
    }
  }

  int pid = 0;
  // The reason we use WMI to launch the process is because the uninstall
  // process runs inside a Job object controlled by the shell. As long as there
  // are processes running, the shell will not close the uninstall applet. WMI
  // allows us to escape from the Job object so the applet will close.
  WMIProcessUtil::Launch(command, &pid);
}

std::wstring GoogleChromeDistribution::GetApplicationName() {
  const std::wstring& product_name =
      installer_util::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
  return product_name;
}

std::wstring GoogleChromeDistribution::GetAlternateApplicationName() {
  const std::wstring& alt_product_name =
      installer_util::GetLocalizedString(IDS_OEM_MAIN_SHORTCUT_NAME_BASE);
  return alt_product_name;
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
  const std::wstring& app_description =
      installer_util::GetLocalizedString(IDS_SHORTCUT_TOOLTIP_BASE);
  return app_description;
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

std::wstring GoogleChromeDistribution::GetStateMediumKey() {
  std::wstring key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(google_update::kChromeGuid);
  return key;
}

std::wstring GoogleChromeDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::wstring GoogleChromeDistribution::GetDistributionData(RegKey* key) {
  DCHECK(NULL != key);
  std::wstring sub_key(google_update::kRegPathClientState);
  sub_key.append(L"\\");
  sub_key.append(google_update::kChromeGuid);

  RegKey client_state_key(key->Handle(), sub_key.c_str());
  std::wstring result;
  std::wstring brand_value;
  if (client_state_key.ReadValue(google_update::kRegRLZBrandField,
                                 &brand_value)) {
    result = google_update::kRegRLZBrandField;
    result.append(L"=");
    result.append(brand_value);
    result.append(L"&");
  }

  std::wstring client_value;
  if (client_state_key.ReadValue(google_update::kRegClientField,
                                 &client_value)) {
    result.append(google_update::kRegClientField);
    result.append(L"=");
    result.append(client_value);
    result.append(L"&");
  }

  std::wstring ap_value;
  // If we fail to read the ap key, send up "&ap=" anyway to indicate
  // that this was probably a stable channel release.
  client_state_key.ReadValue(google_update::kRegApField, &ap_value);
  result.append(google_update::kRegApField);
  result.append(L"=");
  result.append(ap_value);

  return result;
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

// Currently we only have one experiment: the inactive user toast. Which only
// applies for users doing upgrades and non-systemwide install.
void GoogleChromeDistribution::LaunchUserExperiment(
    installer_util::InstallStatus status, const installer::Version& version,
    bool system_install, int options) {
  if ((installer_util::NEW_VERSION_UPDATED != status) || system_install)
    return;

  // If user has not opted-in for usage stats we don't do the experiments.
  if (!GoogleUpdateSettings::GetCollectStatsConsent())
    return;

  std::wstring brand;
  if (GoogleUpdateSettings::GetBrand(&brand) && (brand == L"CHXX")) {
    // The user automatically qualifies for the experiment.
  } else {
    // Time to verify the conditions for the experiment.
    std::wstring client_info;
    if (GoogleUpdateSettings::GetClient(&client_info)) {
      // The user might be participating on another experiment. The only
      // users eligible for this experiment are that have no client info
      // or the client info is "TS00".
      if (client_info != kToastExpBaseGroup)
        return;
    }
    // User must be in the Great Britain as defined by googe_update language.
    std::wstring lang;
    if (!GoogleUpdateSettings::GetLanguage(&lang) || (lang != L"en-GB"))
      return;
    // Check browser usage inactivity by the age of the last-write time of the
    // chrome user data directory. Ninety days is our trigger.
    std::wstring user_data_dir = installer::GetChromeUserDataPath();
    const int kNinetyDays = 90 * 24;
    int dir_age_hours = GetDirectoryWriteAgeInHours(user_data_dir.c_str());
    if (dir_age_hours < kNinetyDays)
      return;
    // At this point the user qualifies for the experiment, however we need to
    // tag a control group, which is at random 50% of the population.
    if (::GetTickCount() & 0x1) {
      // We tag the user, but it wont participate in the experiment.
      GoogleUpdateSettings::SetClient(kToastExpQualifyGroup);
      LOG(INFO) << "User is toast experiment control group";
      return;
    }
  }
  LOG(INFO) << "User drafted for toast experiment";
  if (!GoogleUpdateSettings::SetClient(kToastExpBaseGroup))
    return;
  // The experiment needs to be performed in a different process because
  // google_update expects the upgrade process to be quick and nimble.
  RelaunchSetup(installer_util::switches::kInactiveUserToast);
}

void GoogleChromeDistribution::InactiveUserToastExperiment() {
  // User qualifies for the experiment. Launch chrome with --try-chrome. Before
  // that we need to change the client so we can track the progress.
  int32 exit_code = 0;
  std::wstring option(std::wstring(L" --") + switches::kTryChromeAgain);
  if (!installer::LaunchChromeAndWaitForResult(false, option, &exit_code))
    return;
  // The chrome process has exited, figure out what happened.
  const wchar_t* outcome = NULL;
  switch (exit_code) {
    case ResultCodes::NORMAL_EXIT:
      outcome = kToastExpTriesOkGroup;
      break;
    case ResultCodes::NORMAL_EXIT_EXP1:
      outcome = kToastExpCancelGroup;
      break;
    case ResultCodes::NORMAL_EXIT_EXP2:
      outcome = kToastExpUninstallGroup;
      break;
    default:
      outcome = kToastExpTriesErrorGroup;
  };
  GoogleUpdateSettings::SetClient(outcome);
  if (outcome != kToastExpUninstallGroup)
    return;
  // The user wants to uninstall. This is a best effort operation. Note that
  // we waited for chrome to exit so the uninstall would not detect chrome
  // running.
  base::LaunchApp(InstallUtil::GetChromeUninstallCmd(false),
                  false, false, NULL);
}
