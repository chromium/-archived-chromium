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
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/google_update_constants.h"
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
  file_util::AppendToPath(&existing_archive, installer::kInstallerDir);
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
    const installer::Version* installed_version, bool system_install) {
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
                                          IDS_INSTALL_HIGHER_VERSION_BASE, NULL);
      } else {
        // We want to keep uncompressed archive (chrome.7z) that we get after
        // uncompressing and binary patching. Get the location for this file.
        std::wstring archive_to_copy(temp_path);
        file_util::AppendToPath(&archive_to_copy,
                                std::wstring(installer::kChromeArchive));
        install_status = installer::InstallOrUpdateChrome(
            cmd_line.program(), archive_to_copy, temp_path, system_install,
            *installer_version, installed_version);
        if (install_status == installer_util::FIRST_INSTALL_SUCCESS) {
          LOG(INFO) << "First install successful.";
          if (cmd_line.HasSwitch(
              installer_util::switches::kDoNotLaunchChrome)) {
            std::wstring chrome_exe =
                installer::GetChromeInstallPath(system_install);
            if (!chrome_exe.empty()) {
              file_util::AppendToPath(&chrome_exe, installer_util::kChromeExe);
              InstallUtil::WriteInstallerResult(system_install, install_status,
                                                0, &chrome_exe);
            }
          } else {
            installer::LaunchChrome(system_install);
          }
        } else if (install_status == installer_util::NEW_VERSION_UPDATED) {
          // This is temporary hack and will be deleted after one release.
          UpdateChromeOpenCmd(system_install);

#if defined(GOOGLE_CHROME_BUILD)
          // TODO(kuchhal): This is just temporary until all users move to the
          // new Chromium version which ships with gears.dll.
          LOG(INFO) << "Google Chrome updated. Uninstalling gears msi.";
          wchar_t product[39];  // GUID + '\0'
          MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);  // Don't show any UI
          for (int i = 0;
               MsiEnumRelatedProducts(google_update::kGearsUpgradeCode, 0,
                                      i, product) != ERROR_NO_MORE_ITEMS; ++i) {
            LOG(INFO) << "Uninstalling Gears - " << product;
            unsigned int ret = MsiConfigureProduct(product,
                INSTALLLEVEL_MAXIMUM, INSTALLSTATE_ABSENT);
            if (ret != ERROR_SUCCESS)
              LOG(ERROR) << "Failed to uninstall Gears " << product;
          }
#endif
        }
      }
    }
  }

  // Delete install temporary directory.
  LOG(INFO) << "Deleting temporary directory " << temp_path;
  scoped_ptr<DeleteTreeWorkItem> delete_tree(
      WorkItem::CreateDeleteTreeWorkItem(temp_path, std::wstring()));
  delete_tree->Do();

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
}  // namespace


int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  CommandLine parsed_command_line;
  installer::InitInstallerLogging(parsed_command_line);
  bool system_install =
      parsed_command_line.HasSwitch(installer_util::switches::kSystemLevel);
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
  }

  if (system_install &&
      win_util::GetWinVersion() == win_util::WINVERSION_VISTA &&
      !IsUserAnAdmin()) {
    std::wstring exe = parsed_command_line.program();
    std::wstring params(command_line);
    DWORD exit_code = installer_util::UNKNOWN_STATUS;
    InstallUtil::ExecuteExeAsAdmin(exe, params, &exit_code);
    return exit_code;
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
    install_status = InstallChrome(parsed_command_line,
                                   installed_version.get(),
                                   system_install);
  }

  CoUninitialize();
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return dist->GetInstallReturnCode(install_status);
}
