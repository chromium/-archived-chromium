// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "courgette/courgette.h"
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
  file_util::AppendToPath(&patch_archive, installer::kChromeArchivePatch);

  LOG(INFO) << "Applying patch " << patch_archive
            << " to file " << existing_archive
            << " and generating file " << uncompressed_archive;

  // Try Courgette first.  Courgette checks the patch file first and fails
  // quickly if the patch file does not have a valid Courgette header.
  courgette::Status patch_status =
      courgette::ApplyEnsemblePatch(existing_archive.c_str(),
                                    patch_archive.c_str(),
                                    uncompressed_archive.c_str());

  if (patch_status == courgette::C_OK) {
    return 0;
  }

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
// (chrome.7z) or uncompressed archive patch file (chrome_patch.diff). If it
// is patch file, it is applied to the old archive file that should be
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

    std::wstring uncompressed_archive(temp_path);
    file_util::AppendToPath(&uncompressed_archive, installer::kChromeArchive);

    // Check if this is differential update and if it is, patch it to the
    // installer archive that should already be on the machine.
    std::wstring archive_name = file_util::GetFilenameFromPath(archive);
    std::wstring prefix = installer::kChromeCompressedPatchArchivePrefix;
    if ((archive_name.size() >= prefix.size()) &&
        (std::equal(prefix.begin(), prefix.end(), archive_name.begin(),
                    CaseInsensitiveCompare<wchar_t>()))) {
      incremental_install = true;
      LOG(INFO) << "Differential patch found. Applying to existing archive.";
      // First pre-emptively set flag in registry to get full installer next
      // time. If the current installer works, this flag will get reset at the
      // the end of installation.
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      dist->UpdateDiffInstallStatus(system_install, incremental_install,
                                    installer_util::INSTALL_FAILED);
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

  if (preferences & installer_util::MASTER_PROFILE_ALT_SHORTCUT_TXT ||
      cmd_line.HasSwitch(installer_util::switches::kAltDesktopShortcut))
    options |= installer_util::ALT_DESKTOP_SHORTCUT;

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

bool CheckPreInstallConditions(const installer::Version* installed_version,
                               int options,
                               installer_util::InstallStatus& status) {
  bool system_install = (options & installer_util::SYSTEM_LEVEL) != 0;

  // Check to avoid simultaneous per-user and per-machine installs.
  scoped_ptr<installer::Version>
      chrome_version(InstallUtil::GetChromeVersion(!system_install));
  if (chrome_version.get()) {
    LOG(ERROR) << "Already installed version " << chrome_version->GetString()
               << " conflicts with the current install mode.";
    status = system_install ? installer_util::USER_LEVEL_INSTALL_EXISTS :
                              installer_util::SYSTEM_LEVEL_INSTALL_EXISTS;
    int str_id = system_install ? IDS_INSTALL_USER_LEVEL_EXISTS_BASE :
                                  IDS_INSTALL_SYSTEM_LEVEL_EXISTS_BASE;
    InstallUtil::WriteInstallerResult(system_install, status, str_id, NULL);
    return false;
  }

  // If no previous installation of Chrome, make sure installation directory
  // either does not exist or can be deleted (i.e. is not locked by some other
  // process).
  if (!installed_version) {
    std::wstring install_path(installer::GetChromeInstallPath(system_install));
    if (file_util::PathExists(install_path) &&
        !file_util::Delete(install_path, true)) {
      LOG(ERROR) << "Installation directory " << install_path
                 << " exists and can not be deleted.";
      status = installer_util::INSTALL_DIR_IN_USE;
      int str_id = IDS_INSTALL_DIR_IN_USE_BASE;
      InstallUtil::WriteInstallerResult(system_install, status, str_id, NULL);
      return false;
    }
  }

  return true;
}

installer_util::InstallStatus InstallChrome(const CommandLine& cmd_line,
    const installer::Version* installed_version, int options) {
  installer_util::InstallStatus install_status = installer_util::UNKNOWN_STATUS;
  if (!CheckPreInstallConditions(installed_version, options, install_status))
    return install_status;

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

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring unpack_path(temp_path);
  file_util::AppendToPath(&unpack_path,
                          std::wstring(installer::kInstallSourceDir));
  bool incremental_install = false;
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
        installer_version(setup_util::GetVersionFromDir(src_path));
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
        }
      }
    }
    // There might be an experiment (for upgrade usually) that needs to happen.
    // An experiment's outcome can include chrome's uninstallation. If that is
    // the case we would not do that directly at this point but in another
    //  instance of setup.exe
    dist->LaunchUserExperiment(install_status, *installer_version,
                               system_install, options);
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

  dist->UpdateDiffInstallStatus(system_install, incremental_install,
                                install_status);
  return install_status;
}

installer_util::InstallStatus UninstallChrome(const CommandLine& cmd_line,
                                              const wchar_t* cmd_params,
                                              const installer::Version* version,
                                              bool system_install) {
  LOG(INFO) << "Uninstalling Chome";
  bool force = cmd_line.HasSwitch(installer_util::switches::kForceUninstall);
  if (!version && !force) {
    LOG(ERROR) << "No Chrome installation found for uninstall.";
    InstallUtil::WriteInstallerResult(system_install,
                                      installer_util::CHROME_NOT_INSTALLED,
                                      IDS_UNINSTALL_FAILED_BASE, NULL);
    return installer_util::CHROME_NOT_INSTALLED;
  }

  bool remove_all = !cmd_line.HasSwitch(
      installer_util::switches::kDoNotRemoveSharedItems);
  return installer_setup::UninstallChrome(cmd_line.program(), system_install,
                                          remove_all, force,
                                          cmd_line, cmd_params);
}

installer_util::InstallStatus ShowEULADialog(const std::wstring& inner_frame) {
  LOG(INFO) << "About to show EULA";
  std::wstring eula_path = installer_util::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer_util::EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  if (!inner_frame.empty()) {
    eula_path += L"?innerframe=";
    eula_path += inner_frame;
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

// This method processes any command line options that make setup.exe do
// various tasks other than installation (renaming chrome.exe, showing eula
// among others). This function returns true if any such command line option
// has been found and processed (so setup.exe should exit at that point).
bool HandleNonInstallCmdLineOptions(const CommandLine& cmd_line,
                                    bool system_install,
                                    int& exit_code) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (cmd_line.HasSwitch(installer_util::switches::kUpdateSetupExe)) {
    // First to handle situation where the current process hangs or crashes,
    // we pre-emptively set a flag in registry to get full installer next time.
    installer_util::InstallStatus status = installer_util::SETUP_PATCH_FAILED;
    dist->UpdateDiffInstallStatus(system_install, true, status);

    // If --update-setup-exe command line option is given, we apply the given
    // patch to current exe, and store the resulting binary in the path
    // specified by --new-setup-exe. But we need to first unpack the file
    // given in --update-setup-exe.
    std::wstring temp_path;
    if (!file_util::CreateNewTempDirectory(std::wstring(L"chrome_"),
                                           &temp_path)) {
      LOG(ERROR) << "Could not create temporary path.";
      status = installer_util::SETUP_PATCH_FAILED;
    } else {
      std::wstring setup_patch = cmd_line.GetSwitchValue(
          installer_util::switches::kUpdateSetupExe);
      LOG(INFO) << "Opening archive " << setup_patch;
      DWORD ret = NO_ERROR;
      installer::LzmaUtil util;
      if ((ret = util.OpenArchive(setup_patch)) != NO_ERROR) {
        LOG(ERROR) << "Unable to open install archive: " << setup_patch;
      } else {
        LOG(INFO) << "Uncompressing archive to path " << temp_path;
        if ((ret = util.UnPack(temp_path)) != NO_ERROR) {
          LOG(ERROR) << "Error during uncompression: " << ret;
        }
        util.CloseArchive();
      }

      if (ret != NO_ERROR) {
        status = installer_util::SETUP_PATCH_FAILED;
      } else {
        std::wstring old_setup_exe = cmd_line.program();
        std::wstring uncompressed_setup_patch(temp_path);
        file_util::AppendToPath(&uncompressed_setup_patch,
                                installer::kSetupExePatch);
        std::wstring new_setup_exe = cmd_line.GetSwitchValue(
            installer_util::switches::kNewSetupExe);
        LOG(INFO) << "Patching " << old_setup_exe
                  << " with patch " << uncompressed_setup_patch
                  << " and creating new exe " << new_setup_exe;

        // Try Courgette first.
        courgette::Status patch_status = courgette::ApplyEnsemblePatch(
            old_setup_exe.c_str(), uncompressed_setup_patch.c_str(),
            new_setup_exe.c_str());

        // If courgette didn't work, try regular bspatch.
        if (patch_status != courgette::C_OK) {
          LOG(WARNING) << "setup patch failed using courgette " << patch_status;
          if (!ApplyBinaryPatch(old_setup_exe.c_str(),
                                uncompressed_setup_patch.c_str(),
                                new_setup_exe.c_str()))
            status = installer_util::NEW_VERSION_UPDATED;
        } else {
          status = installer_util::NEW_VERSION_UPDATED;
        }
      }
    }

    // If the current patching worked reset the flag set in the registry
    dist->UpdateDiffInstallStatus(system_install, true, status);

    exit_code = dist->GetInstallReturnCode(status);
    if (exit_code) {
      LOG(WARNING) << "setup.exe patching failed.";
      InstallUtil::WriteInstallerResult(system_install, status,
                                        IDS_SETUP_PATCH_FAILED_BASE, NULL);
    }
    return true;
  } else if (cmd_line.HasSwitch(installer_util::switches::kShowEula)) {
    // Check if we need to show the EULA. If it is passed as a command line
    // then the dialog is shown and regardless of the outcome setup exits here.
    std::wstring inner_frame =
        cmd_line.GetSwitchValue(installer_util::switches::kShowEula);
    exit_code = ShowEULADialog(inner_frame);
    if (installer_util::EULA_REJECTED != exit_code)
      GoogleUpdateSettings::SetEULAConsent(true);
    return true;;
  } else if (cmd_line.HasSwitch(
      installer_util::switches::kRegisterChromeBrowser)) {
    // If --register-chrome-browser option is specified, register all
    // Chrome protocol/file associations as well as register it as a valid
    // browser for Start Menu->Internet shortcut. This option should only
    // be used when setup.exe is launched with admin rights. We do not
    // make any user specific changes in this option.
    std::wstring chrome_exe(cmd_line.GetSwitchValue(
        installer_util::switches::kRegisterChromeBrowser));
    exit_code = ShellUtil::AddChromeToSetAccessDefaults(chrome_exe, true);
    return true;
  } else if (cmd_line.HasSwitch(installer_util::switches::kRenameChromeExe)) {
    // If --rename-chrome-exe is specified, we want to rename the executables
    // and exit.
    exit_code = RenameChromeExecutables(system_install);
    return true;
  } else if (cmd_line.HasSwitch(
      installer_util::switches::kRemoveChromeRegistration)) {
    installer_util::InstallStatus tmp = installer_util::UNKNOWN_STATUS;
    installer_setup::DeleteChromeRegistrationKeys(HKEY_LOCAL_MACHINE, tmp);
    exit_code = tmp;
    return true;
  } else if (cmd_line.HasSwitch(installer_util::switches::kInactiveUserToast)) {
    // Launch the inactive user toast experiment.
    dist->InactiveUserToastExperiment();
    return true;
  }
  return false;
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

  int exit_code = 0;
  if (HandleNonInstallCmdLineOptions(parsed_command_line, system_install,
                                     exit_code))
    return exit_code;

  if (system_install && !IsUserAnAdmin()) {
    if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA &&
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
                                     command_line,
                                     installed_version.get(),
                                     system_install);
  // If --uninstall option is not specified, we assume it is install case.
  } else {
    install_status = InstallChrome(parsed_command_line,
                                   installed_version.get(),
                                   options);
  }

  CoUninitialize();
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return dist->GetInstallReturnCode(install_status);
}
