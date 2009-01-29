// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <windows.h>
#include <msi.h>
#include <shlobj.h>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/installer/setup/setup.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "third_party/bspatch/mbspatch.h"

#include "installer_util_strings.h"

namespace {

// Applies a binary patch to existing Chrome installer archive on the system.
// Uses bspatch library.
int PatchArchiveFile(bool system_install, const std::wstring& archive_path,
                     const std::wstring& uncompressed_archive,
                     const installer::Version* installed_version) {
  std::wstring existing_archive =
      installer::GetChromeInstallPath(system_install);
  file_util::AppendToPath(&existing_archive,
                          installed_version->GetString());
  file_util::AppendToPath(&existing_archive, installer_util::kInstallerDir);
  file_util::AppendToPath(&existing_archive, installer::kChromeArchive);

  std::wstring patch_archive(archive_path);
  file_util::AppendToPath(&patch_archive, installer::kChromePatchArchive);

  LOG(INFO) << "Applying patch " << patch_archive
            << " to file " << existing_archive
            << " and generating file " << uncompressed_archive;
  return ApplyBinaryPatch(existing_archive.c_str(),
                          patch_archive.c_str(),
                          uncompressed_archive.c_str());
}


// This method unpacks and uncompresses the given archive file. For Chrome
// install we are creating a uncompressed archive that contains all the files
// needed for the installer. This uncompressed archive is later compressed.
//
// This method first uncompresses archive specified by parameter "archive"
// and assumes that it will result in an uncompressed full archive file
// (chrome.7z) or uncompressed patch archive file (patch.7z). If it is patch
// archive file, the patch is applied to the old archive file that should be
// present on the system already. As the final step the new archive file
// is unpacked in the path specified by parameter "path".
DWORD UnPackArchive(const std::wstring& archive, bool system_install,
                    const installer::Version* installed_version,
                    const std::wstring& temp_path, const std::wstring& path,
                    bool& incremental_install) {
    DWORD ret = NO_ERROR;
    installer::LzmaUtil util;
    // First uncompress the payload. This could be a differential
    // update (patch.7z) or full archive (chrome.7z). If this uncompress fails
    // return with error.
    LOG(INFO) << "Opening archive " << archive;
    if ((ret = util.OpenArchive(archive)) != NO_ERROR) {
      LOG(ERROR) << "Unable to open install archive: " << archive;
    } else {
      LOG(INFO) << "Uncompressing archive to path " << temp_path;
      if ((ret = util.UnPack(temp_path)) != NO_ERROR) {
        LOG(ERROR) << "Error during uncompression: " << ret;
      }
      util.CloseArchive();
    }
    if (ret != NO_ERROR)
      return ret;

    std::wstring archive_name = file_util::GetFilenameFromPath(archive);
    std::wstring uncompressed_archive(temp_path);
    file_util::AppendToPath(&uncompressed_archive, installer::kChromeArchive);
    // Check if this is differential update and if it is, patch it to the
    // installer archive that should already be on the machine.
    std::wstring prefix = installer::kChromeCompressedPatchArchivePrefix;
    if ((archive_name.size() >= prefix.size()) &&
        (std::equal(prefix.begin(), prefix.end(), archive_name.begin(),
                    CaseInsensitiveCompare<wchar_t>()))) {
      LOG(INFO) << "Differential patch found. Applying to existing archive.";
      incremental_install = true;
      if (!installed_version) {
        LOG(ERROR) << "Can not use differential update when Chrome is not "
                   << "installed on the system.";
        return 1;
      }
      if (int i = PatchArchiveFile(system_install, temp_path,
                                   uncompressed_archive, installed_version)) {
        LOG(ERROR) << "Binary patching failed with error " << i;
        return 1;
      }
    }

    // If we got the uncompressed archive, lets unpack it
    LOG(INFO) << "Opening archive " << uncompressed_archive;
    if ((ret = util.OpenArchive(uncompressed_archive)) != NO_ERROR) {
      LOG(ERROR) << "Unable to open install archive: " <<
          uncompressed_archive;
    } else {
      LOG(INFO) << "Unpacking archive to path " << path;
      if ((ret = util.UnPack(path)) != NO_ERROR) {
        LOG(ERROR) << "Error during uncompression: " << ret;
      }
      util.CloseArchive();
    }

    return ret;
}


// Find the version of Chrome from an install source directory.
// Chrome_path should contain a complete and unpacked install package (i.e.
// a Chrome directory under which there is a version folder).
// Returns the version or NULL if no version is found.
installer::Version* GetVersionFromDir(const std::wstring& chrome_path) {
  LOG(INFO) << "Looking for Chrome version folder under " << chrome_path;
  std::wstring root_path(chrome_path);
  file_util::AppendToPath(&root_path, L"*");

  WIN32_FIND_DATA find_file_data;
  HANDLE file_handle = FindFirstFile(root_path.c_str(), &find_file_data);
  BOOL ret = TRUE;
  installer::Version *version = NULL;
  // Here we are assuming that the installer we have is really valid so there
  // can not be two version directories. We exit as soon as we find a valid
  // version directory.
  while (ret) {
    if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      LOG(INFO) << "directory found: " << find_file_data.cFileName;
      version =
          installer::Version::GetVersionFromString(find_file_data.cFileName);
      if (version) break;
    }
    ret = FindNextFile(file_handle, &find_file_data);
  }
  FindClose(file_handle);

  return version;
}

// This function is called when --rename-chrome-exe option is specified on
// setup.exe command line. This function assumes an in-use update has happened
// for Chrome so there should be a file called new_chrome.exe on the file
// system and a key called 'opv' in the registry. This function will move
// new_chrome.exe to chrome.exe and delete 'opv' key in one atomic operation.
installer_util::InstallStatus RenameChromeExecutables(bool system_install) {
  std::wstring chrome_path(installer::GetChromeInstallPath(system_install));

  std::wstring chrome_exe(chrome_path);
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
  std::wstring chrome_old_exe(chrome_path);
  file_util::AppendToPath(&chrome_old_exe, installer_util::kChromeOldExe);
  std::wstring chrome_new_exe(chrome_path);
  file_util::AppendToPath(&chrome_new_exe, installer_util::kChromeNewExe);

  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddDeleteTreeWorkItem(chrome_old_exe, std::wstring());
  std::wstring temp_path;
  if (!file_util::CreateNewTempDirectory(std::wstring(L"chrome_"),
                                         &temp_path)) {
    LOG(ERROR) << "Failed to create Temp directory " << temp_path;
    return installer_util::RENAME_FAILED;
  }
  install_list->AddCopyTreeWorkItem(chrome_new_exe,
                                    chrome_exe,
                                    temp_path,
                                    WorkItem::IF_DIFFERENT,
                                    std::wstring());
  HKEY reg_root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  BrowserDistribution *dist = BrowserDistribution::GetDistribution();
  install_list->AddDeleteRegValueWorkItem(reg_root,
                                          dist->GetVersionKey(),
                                          google_update::kRegOldVersionField,
                                          true);
  install_list->AddDeleteTreeWorkItem(chrome_new_exe, std::wstring());
  install_list->AddDeleteRegValueWorkItem(reg_root,
                                          dist->GetVersionKey(),
                                          google_update::kRegRenameCmdField,
                                          true);
  installer_util::InstallStatus ret = installer_util::RENAME_SUCCESSFUL;
  if (!install_list->Do()) {
    LOG(ERROR) << "Renaming of executables failed. Rolling back any changes.";
    install_list->Rollback();
    ret = installer_util::RENAME_FAILED;
  }
  file_util::Delete(temp_path, true);
  return ret;
}

// Parse command line and read master profile, if present, to get distribution
// related install options.
int GetInstallOptions(const CommandLine& cmd_line) {
  int options = 0;
  int preferences = 0;

  if (cmd_line.HasSwitch(installer_util::switches::kInstallerData)) {
    std::wstring prefs_path = cmd_line.GetSwitchValue(
        installer_util::switches::kInstallerData);
    preferences = installer_util::ParseDistributionPreferences(prefs_path);
    if ((preferences & installer_util::MASTER_PROFILE_NOT_FOUND) == 0) {
      options |= installer_util::MASTER_PROFILE_PRESENT;
      if ((preferences & installer_util::MASTER_PROFILE_ERROR) == 0)
        options |= installer_util::MASTER_PROFILE_VALID;
    }
    // While there is a --show-eula command line flag, we don't process
    // it in this function because it requires special handling.
    if (preferences & installer_util::MASTER_PROFILE_REQUIRE_EULA)
      options |= installer_util::SHOW_EULA_DIALOG;
  }

  if (preferences & installer_util::MASTER_PROFILE_CREATE_ALL_SHORTCUTS ||
      cmd_line.HasSwitch(installer_util::switches::kCreateAllShortcuts))
    options |= installer_util::CREATE_ALL_SHORTCUTS;

  if (preferences & installer_util::MASTER_PROFILE_DO_NOT_LAUNCH_CHROME ||
      cmd_line.HasSwitch(installer_util::switches::kDoNotLaunchChrome))
    options |= installer_util::DO_NOT_LAUNCH_CHROME;

  if (preferences & installer_util::MASTER_PROFILE_MAKE_CHROME_DEFAULT ||
      cmd_line.HasSwitch(installer_util::switches::kMakeChromeDefault))
    options |= installer_util::MAKE_CHROME_DEFAULT;

  if (preferences & installer_util::MASTER_PROFILE_SYSTEM_LEVEL ||
      cmd_line.HasSwitch(installer_util::switches::kSystemLevel))
    options |= installer_util::SYSTEM_LEVEL;

  if (preferences & installer_util::MASTER_PROFILE_VERBOSE_LOGGING ||
      cmd_line.HasSwitch(installer_util::switches::kVerboseLogging))
    options |= installer_util::VERBOSE_LOGGING;
  
  return options;
}

// Copy master preference file if provided to installer to the same path
// of chrome.exe so Chrome first run can find it.
// This function will be called only when Chrome is launched the first time.
void CopyPreferenceFileForFirstRun(int options, const CommandLine& cmd_line) {
  if (options & installer_util::MASTER_PROFILE_VALID) {
    std::wstring prefs_source_path = cmd_line.GetSwitchValue(
        installer_util::switches::kInstallerData);
    bool system_install = (options & installer_util::SYSTEM_LEVEL) != 0;
    std::wstring prefs_dest_path(
        installer::GetChromeInstallPath(system_install));
    file_util::AppendToPath(&prefs_dest_path,
                            installer_util::kDefaultMasterPrefs);
    if (!file_util::CopyFile(prefs_source_path, prefs_dest_path))
      LOG(ERROR) << "failed copying master profile";
  }
}

// This method is temporary and only called by UpdateChromeOpenCmd() below.
void ReplaceRegistryValue(const std::wstring& reg_key,
                          const std::wstring& old_val,
                          const std::wstring& new_val) {
  RegKey key;
  std::wstring value;
  if (key.Open(HKEY_CLASSES_ROOT, reg_key.c_str(), KEY_READ) &&
      key.ReadValue(NULL, &value) && (old_val == value)) {
    std::wstring key_path = L"Software\\Classes\\" + reg_key;
    if (key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_WRITE))
      key.WriteValue(NULL, new_val.c_str());
    if (key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_WRITE))
      key.WriteValue(NULL, new_val.c_str());
  }
}

// This method is only temporary to update Chrome open cmd for existing users
// of Chrome. This can be deleted once we make one release including this patch
// to every user.
void UpdateChromeOpenCmd(bool system_install) {
  std::wstring chrome_exe =  installer::GetChromeInstallPath(system_install);
  file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
  std::wstring old_open_cmd = L"\"" + chrome_exe + L"\" \"%1\"";
  std::wstring new_open_cmd = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
  std::wstring reg_key[] = { L"ChromeHTML\\shell\\open\\command",
                             L"http\\shell\\open\\command",
                             L"https\\shell\\open\\command" };
  for (int i = 0; i < _countof(reg_key); i++)
    ReplaceRegistryValue(reg_key[i], old_open_cmd, new_open_cmd);
}

installer_util::InstallStatus InstallChrome(const CommandLine& cmd_line,
    const installer::Version* installed_version, int options) {
  bool system_install = (options & installer_util::SYSTEM_LEVEL) != 0;
  // For install the default location for chrome.packed.7z is in current
  // folder, so get that value first.
  std::wstring archive = file_util::GetDirectoryFromPath(cmd_line.program());
  file_util::AppendToPath(&archive,
                          std::wstring(installer::kChromeCompressedArchive));
  // If --install-archive is given, get the user specified value
  if (cmd_line.HasSwitch(installer_util::switches::kInstallArchive)) {
    archive = cmd_line.GetSwitchValue(
        installer_util::switches::kInstallArchive);
  }
  LOG(INFO) << "Archive found to install Chrome " << archive;

  // Create a temp folder where we will unpack Chrome archive. If it fails,
  // then we are doomed, so return immediately and no cleanup is required.
  std::wstring temp_path;
  if (!file_util::CreateNewTempDirectory(std::wstring(L"chrome_"),
                                         &temp_path)) {
    LOG(ERROR) << "Could not create temporary path.";
    InstallUtil::WriteInstallerResult(system_install,
                                      installer_util::TEMP_DIR_FAILED,
                                      IDS_INSTALL_TEMP_DIR_FAILED_BASE,
                                      NULL);
    return installer_util::TEMP_DIR_FAILED;
  }
  LOG(INFO) << "created path " << temp_path;

  std::wstring unpack_path(temp_path);
  file_util::AppendToPath(&unpack_path,
                          std::wstring(installer::kInstallSourceDir));
  bool incremental_install = false;
  installer_util::InstallStatus install_status = installer_util::UNKNOWN_STATUS;
  if (UnPackArchive(archive, system_install, installed_version,
                    temp_path, unpack_path, incremental_install)) {
    install_status = installer_util::UNCOMPRESSION_FAILED;
    InstallUtil::WriteInstallerResult(system_install, install_status,
                                      IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                      NULL);
  } else {
    LOG(INFO) << "unpacked to " << unpack_path;
    std::wstring src_path(unpack_path);
    file_util::AppendToPath(&src_path,
        std::wstring(installer::kInstallSourceChromeDir));
    scoped_ptr<installer::Version>
        installer_version(GetVersionFromDir(src_path));
    if (!installer_version.get()) {
      LOG(ERROR) << "Did not find any valid version in installer.";
      install_status = installer_util::INVALID_ARCHIVE;
      InstallUtil::WriteInstallerResult(system_install, install_status,
                                        IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
    } else {
      LOG(INFO) << "version to install: " << installer_version->GetString();
      if (installed_version &&
          installed_version->IsHigherThan(installer_version.get())) {
        LOG(ERROR) << "Higher version is already installed.";
        install_status = installer_util::HIGHER_VERSION_EXISTS;
        InstallUtil::WriteInstallerResult(system_install, install_status,
                                          IDS_INSTALL_HIGHER_VERSION_BASE,
                                          NULL);
      } else {
        // We want to keep uncompressed archive (chrome.7z) that we get after
        // uncompressing and binary patching. Get the location for this file.
        std::wstring archive_to_copy(temp_path);
        file_util::AppendToPath(&archive_to_copy,
                                std::wstring(installer::kChromeArchive));
        install_status = installer::InstallOrUpdateChrome(
            cmd_line.program(), archive_to_copy, temp_path, options,
            *installer_version, installed_version);

        int install_msg_base = IDS_INSTALL_FAILED_BASE;
        std::wstring chrome_exe;
        if (install_status != installer_util::INSTALL_FAILED) {
          chrome_exe = installer::GetChromeInstallPath(system_install);
          if (chrome_exe.empty()) {
            // If we failed to construct install path, it means the OS call to
            // get %ProgramFiles% or %AppData% failed. Report this as failure.
            install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
            install_status = installer_util::OS_ERROR;
          } else {
            file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
            chrome_exe = L"\"" + chrome_exe + L"\"";
            install_msg_base = 0;
          }
        }
        InstallUtil::WriteInstallerResult(system_install, install_status,
                                          install_msg_base, &chrome_exe);
        if (install_status == installer_util::FIRST_INSTALL_SUCCESS) {
          LOG(INFO) << "First install successful.";
          CopyPreferenceFileForFirstRun(options, cmd_line);
          // We never want to launch Chrome in system level install mode.
          if (!(options & installer_util::DO_NOT_LAUNCH_CHROME) &&
              !(options & installer_util::SYSTEM_LEVEL))
            installer::LaunchChrome(system_install);
        } else if (install_status == installer_util::NEW_VERSION_UPDATED) {
          // This is temporary hack and will be deleted after one release.
          UpdateChromeOpenCmd(system_install);
        }
      }
    }
  }

  // Delete temporary files. These include install temporary directory
  // and master profile file if present.
  scoped_ptr<WorkItemList> cleanup_list(WorkItem::CreateWorkItemList());
  LOG(INFO) << "Deleting temporary directory " << temp_path;
  cleanup_list->AddDeleteTreeWorkItem(temp_path, std::wstring());
  if (options & installer_util::MASTER_PROFILE_PRESENT) {
    std::wstring prefs_path = cmd_line.GetSwitchValue(
        installer_util::switches::kInstallerData);
    cleanup_list->AddDeleteTreeWorkItem(prefs_path, std::wstring());
  }
  cleanup_list->Do();

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  dist->UpdateDiffInstallStatus(system_install, incremental_install,
                                install_status);
  return install_status;
}

installer_util::InstallStatus UninstallChrome(const CommandLine& cmd_line,
                                              const installer::Version* version,
                                              bool system_install) {
  LOG(INFO) << "Uninstalling Chome";
  if (!version) {
    LOG(ERROR) << "No Chrome installation found for uninstall.";
    InstallUtil::WriteInstallerResult(system_install,
                                      installer_util::CHROME_NOT_INSTALLED,
                                      IDS_UNINSTALL_FAILED_BASE, NULL);
    return installer_util::CHROME_NOT_INSTALLED;
  }

  bool remove_all = !cmd_line.HasSwitch(
      installer_util::switches::kDoNotRemoveSharedItems);
  bool force = cmd_line.HasSwitch(installer_util::switches::kForceUninstall);
  return installer_setup::UninstallChrome(cmd_line.program(), system_install,
                                          *version, remove_all, force);
}

installer_util::InstallStatus ShowEULADialog() {
  LOG(INFO) << "About to show EULA";
  std::wstring eula_path = installer_util::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer_util::EULA_REJECTED;
  }
  installer::EulaHTMLDialog dlg(eula_path);
  installer::EulaHTMLDialog::Outcome outcome = dlg.ShowModal();
  if (installer::EulaHTMLDialog::REJECTED == outcome) {
    LOG(ERROR) << "EULA rejected or EULA failure";
    return installer_util::EULA_REJECTED;
  }
  if (installer::EulaHTMLDialog::ACCEPTED_OPT_IN == outcome) {
    LOG(INFO) << "EULA accepted (opt-in)";
    return installer_util::EULA_ACCEPTED_OPT_IN;
  }
  LOG(INFO) << "EULA accepted (no opt-in)";
  return installer_util::EULA_ACCEPTED;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  CommandLine::Init(0, NULL);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  installer::InitInstallerLogging(parsed_command_line);
  int options = GetInstallOptions(parsed_command_line);
  if (options & installer_util::VERBOSE_LOGGING)
    logging::SetMinLogLevel(logging::LOG_INFO);

  bool system_install = (options & installer_util::SYSTEM_LEVEL) != 0;
  LOG(INFO) << "system install is " << system_install;

  // Check to make sure current system is WinXP or later. If not, log
  // error message and get out.
  if (!InstallUtil::IsOSSupported()) {
    LOG(ERROR) << "Chrome only supports Windows XP or later.";
    InstallUtil::WriteInstallerResult(system_install,
                                      installer_util::OS_NOT_SUPPORTED,
                                      IDS_INSTALL_OS_NOT_SUPPORTED_BASE, NULL);
    return installer_util::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK) {
    LOG(ERROR) << "COM initialization failed.";
    InstallUtil::WriteInstallerResult(system_install,
                                      installer_util::OS_ERROR,
                                      IDS_INSTALL_OS_ERROR_BASE, NULL);
    return installer_util::OS_ERROR;
  }

  // Check if we need to show the EULA. If it is passed as a command line
  // then the dialog is shown and regardless of the outcome setup exits here.
  if (parsed_command_line.HasSwitch(installer_util::switches::kShowEula)) {
    return ShowEULADialog();
  }

  // If --register-chrome-browser option is specified, register all
  // Chrome protocol/file associations as well as register it as a valid
  // browser for StarMenu->Internet shortcut. This option should only
  // be used when setup.exe is launched with admin rights. We do not
  // make any user specific changes in this option.
  if (parsed_command_line.HasSwitch(
      installer_util::switches::kRegisterChromeBrowser)) {
    std::wstring chrome_exe(parsed_command_line.GetSwitchValue(
        installer_util::switches::kRegisterChromeBrowser));
    return ShellUtil::AddChromeToSetAccessDefaults(chrome_exe, true);
  // If --rename-chrome-exe is specified, we want to rename the executables
  // and exit.
  } else if (parsed_command_line.HasSwitch(
      installer_util::switches::kRenameChromeExe)) {
    return RenameChromeExecutables(system_install);
  }

  if (system_install && !IsUserAnAdmin()) {
    if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA &&
        !parsed_command_line.HasSwitch(installer_util::switches::kRunAsAdmin)) {
      std::wstring exe = parsed_command_line.program();
      std::wstring params(command_line);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      params.append(L" --");
      params.append(installer_util::switches::kRunAsAdmin);
      DWORD exit_code = installer_util::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(exe, params, &exit_code);
      return exit_code;
    } else {
      LOG(ERROR) << "Non admin user can not install system level Chrome.";
      InstallUtil::WriteInstallerResult(system_install,
                                        installer_util::INSUFFICIENT_RIGHTS,
                                        IDS_INSTALL_INSUFFICIENT_RIGHTS_BASE,
                                        NULL);
      return installer_util::INSUFFICIENT_RIGHTS;
    }
  }

  // Check the existing version installed.
  scoped_ptr<installer::Version>
      installed_version(InstallUtil::GetChromeVersion(system_install));
  if (installed_version.get()) {
    LOG(INFO) << "version on the system: " << installed_version->GetString();
  }

  installer_util::InstallStatus install_status = installer_util::UNKNOWN_STATUS;
  // If --uninstall option is given, uninstall chrome
  if (parsed_command_line.HasSwitch(installer_util::switches::kUninstall)) {
    install_status = UninstallChrome(parsed_command_line,
                                     installed_version.get(),
                                     system_install);
  // If --uninstall option is not specified, we assume it is install case.
  } else {
    // Check to avoid simultaneous per-user and per-machine installs.
    scoped_ptr<installer::Version>
        chrome_version(InstallUtil::GetChromeVersion(!system_install));
    if (chrome_version.get()) {
      LOG(ERROR) << "Already installed version " << chrome_version->GetString()
                 << " conflicts with the current install mode.";
      installer_util::InstallStatus status = system_install ?
          installer_util::USER_LEVEL_INSTALL_EXISTS :
          installer_util::SYSTEM_LEVEL_INSTALL_EXISTS;
      int str_id = system_install ? IDS_INSTALL_USER_LEVEL_EXISTS_BASE :
                                    IDS_INSTALL_SYSTEM_LEVEL_EXISTS_BASE;
      InstallUtil::WriteInstallerResult(system_install, status, str_id, NULL);
      return status;
    }

    install_status = InstallChrome(parsed_command_line,
                                   installed_version.get(),
                                   options);
  }

  CoUninitialize();
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return dist->GetInstallReturnCode(install_status);
}
