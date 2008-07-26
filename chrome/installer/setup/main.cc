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

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/registry.h"
#include "base/string_util.h"
#include "chrome/installer/setup/setup.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/uninstall.h"
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

namespace {

// Checks if the current system is running Windows XP or later. We are not
// supporting Windows 2K for beta release.
bool IsWindowsXPorLater() {
  OSVERSIONINFOEX osvi;
  ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  GetVersionEx((OSVERSIONINFO *) &osvi);

  // Windows versioning scheme doesn't seem very clear but here is what
  // the code is checking as the minimum version required for Chrome:
  // * Major > 5 is Vista or later so no further checks for Service Pack
  // * Major = 5 && Minor > 1 is Windows Server 2003 so again no SP checks
  // * Major = 5 && Minor = 1 is WinXP so check for SP1 or later
  LOG(INFO) << "Windows Version: Major - " << osvi.dwMajorVersion
            << " Minor - " << osvi.dwMinorVersion
            << " Service Pack Major - " << osvi.wServicePackMajor
            << " Service Pack Minor - " << osvi.wServicePackMinor;
  if ((osvi.dwMajorVersion > 5) || // Vista or later
      ((osvi.dwMajorVersion == 5) && (osvi.dwMinorVersion > 1)) || // Win 2003
      ((osvi.dwMajorVersion == 5) && (osvi.dwMinorVersion == 1) &&
       (osvi.wServicePackMajor >= 1))) { // WinXP with SP1
    return true;
  }
  return false;
}

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
  return ApplyBinaryPatch(WideToUTF8(existing_archive).c_str(),
                          WideToUTF8(patch_archive).c_str(),
                          WideToUTF8(uncompressed_archive).c_str());
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
      LOG(ERROR) << "unable to open install archive: " << archive;
    } else {
      LOG(INFO) << "Uncompressing archive to path " << temp_path;
      if ((ret = util.UnPack(temp_path)) != NO_ERROR) {
        LOG(ERROR) << "error during uncompression: " << ret;
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
      if (PatchArchiveFile(system_install, temp_path, uncompressed_archive,
                           installed_version)) {
        LOG(ERROR) << "Binary patching failed.";
        return 1;
      }
    }

    // If we got the uncompressed archive, lets unpack it
    LOG(INFO) << "Opening archive " << uncompressed_archive;
    if ((ret = util.OpenArchive(uncompressed_archive)) != NO_ERROR) {
      LOG(ERROR) << "unable to open install archive: " <<
          uncompressed_archive;
    } else {
      LOG(INFO) << "Unpacking archive to path " << path;
      if ((ret = util.UnPack(path)) != NO_ERROR) {
        LOG(ERROR) << "error during uncompression: " << ret;
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

// This method checks if we need to change "ap" key in Google Update to try
// full installer as fall back method in case incremental installer fails.
// - If incremental installer fails we append a magic string ("-full"), if
// it is not present already, so that Google Update server next time will send
// full installer to update Chrome on the local machine
// - If we are currently running full installer, we remove this magic
// string (if it is present) regardless of whether installer failed or not.
// There is no fall-back for full installer :)
void ResetGoogleUpdateApKey(bool system_install, bool incremental_install,
                            installer_util::InstallStatus install_status) {
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring ap_key_value;
  std::wstring chrome_google_update_state_key(
      google_update::kRegPathClientState);
  chrome_google_update_state_key.append(L"\\");
  chrome_google_update_state_key.append(google_update::kChromeGuid);
  if (!key.Open(reg_root, chrome_google_update_state_key.c_str(),
      KEY_ALL_ACCESS) || !key.ReadValue(google_update::kRegApFieldName,
      &ap_key_value)) {
    LOG(INFO) << "Application key not found. Returning without changing it.";
    key.Close();
    return;
  }

  std::wstring new_value = InstallUtil::GetNewGoogleUpdateApKey(
      incremental_install, install_status, ap_key_value);
  if ((new_value.compare(ap_key_value) != 0) &&
      !key.WriteValue(google_update::kRegApFieldName, new_value.c_str())) {
    LOG(ERROR) << "Failed to write value " << new_value
               << " to the registry field " << google_update::kRegApFieldName;
  }
  key.Close();
}
}  // namespace


int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  CommandLine parsed_command_line;
  installer::InitInstallerLogging(parsed_command_line);

  // Check to make sure current system is WinXP or later. If not, log
  // error message and get out.
  if (!IsWindowsXPorLater()) {
    LOG(ERROR) << "Chrome only supports Windows XP or later";
    return installer_util::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK) {
    LOG(ERROR) << "COM initialization failed.";
    return installer_util::OS_ERROR;
  }

  bool system_install =
      parsed_command_line.HasSwitch(installer_util::switches::kSystemInstall);
  LOG(INFO) << "system install is " << system_install;

  // Check the existing version installed.
  scoped_ptr<installer::Version>
      installed_version(InstallUtil::GetChromeVersion(system_install));
  if (installed_version.get()) {
    LOG(INFO) << "version on the system: " << installed_version->GetString();
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

  installer_util::InstallStatus install_status = installer_util::UNKNOWN_STATUS;
  if (parsed_command_line.HasSwitch(installer_util::switches::kUninstall)) {
    bool remove_all = true;
    if (parsed_command_line.HasSwitch(
        installer_util::switches::kDoNotRemoveSharedItems))
      remove_all = false;
    // If --uninstall option is given, uninstall chrome
    LOG(INFO) << "Uninstalling Chome";
    if (!installed_version.get()) {
      LOG(ERROR) << "No Chrome installation found for uninstall";
      install_status = installer_util::CHROME_NOT_INSTALLED;
    } else {
      install_status = installer_setup::UninstallChrome(
          parsed_command_line.program(), system_install,
          *installed_version, remove_all);
    }
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    // For install the default location for chrome.packed.7z is in current
    // folder, so get that value first.
    std::wstring archive_path =
        file_util::GetDirectoryFromPath(parsed_command_line.program());
    file_util::AppendToPath(&archive_path,
                            std::wstring(installer::kChromeCompressedArchive));
    // If --install-archive is given, get the user specified value
    if (parsed_command_line.HasSwitch(
        installer_util::switches::kInstallArchive)) {
      archive_path = parsed_command_line.GetSwitchValue(
          installer_util::switches::kInstallArchive);
    }
    LOG(INFO) << "Archive found to install Chrome " << archive_path;

    // Create a temp folder where we will unpack Chrome archive. If it fails,
    // then we are doomed so return immediately and no cleanup is required.
    std::wstring install_temp_path;
    if (!file_util::CreateNewTempDirectory(std::wstring(L"chrome_"),
                                           &install_temp_path)) {
      LOG(ERROR) << "can not create temporary path";
      return installer_util::TEMP_DIR_FAILED;
    }
    LOG(INFO) << "created path " << install_temp_path;
    std::wstring unpack_path(install_temp_path);
    file_util::AppendToPath(&unpack_path,
                            std::wstring(installer::kInstallSourceDir));

    bool incremental_install = false;
    if (UnPackArchive(archive_path, system_install, installed_version.get(),
                      install_temp_path, unpack_path, incremental_install)) {
      install_status = installer_util::UNCOMPRESSION_FAILED;
    } else {
      LOG(INFO) << "unpacked to " << unpack_path;
      std::wstring src_path(unpack_path);
      file_util::AppendToPath(&src_path,
          std::wstring(installer::kInstallSourceChromeDir));
      scoped_ptr<installer::Version>
          installer_version(GetVersionFromDir(src_path));
      if (!installer_version.get()) {
        LOG(ERROR) << "didn't find any valid version in installer";
        install_status = installer_util::INVALID_ARCHIVE;
      } else {
        LOG(INFO) << "version to be installed: " <<
            installer_version->GetString();
        if (installed_version.get() &&
            installed_version->IsHigherThan(installer_version.get())) {
          LOG(ERROR) << "Higher version is already installed.";
          install_status = installer_util::HIGHER_VERSION_EXISTS;
        } else {
          // We want to keep uncompressed archive (chrome.7z) that we get after
          // uncompressing and binary patching. Get the location for this file.
          std::wstring archive_to_copy(install_temp_path);
          file_util::AppendToPath(&archive_to_copy,
                                  std::wstring(installer::kChromeArchive));
          install_status = installer::InstallOrUpdateChrome(
              parsed_command_line.program(), archive_to_copy,
              install_temp_path, system_install,
              *installer_version, installed_version.get());
          if (install_status == installer_util::FIRST_INSTALL_SUCCESS) {
            LOG(INFO) << "First install successful. Launching Chrome.";
            installer::LaunchChrome(system_install);
          }
        }
      }
    }

    // Delete install temporary directory.
    LOG(INFO) << "Deleting temporary directory " << install_temp_path;
    scoped_ptr<DeleteTreeWorkItem> delete_tree(
        WorkItem::CreateDeleteTreeWorkItem(install_temp_path,
                                           std::wstring()));
    delete_tree->Do();

    ResetGoogleUpdateApKey(system_install, incremental_install, install_status);

    // TBD: The previous installs/updates may leave some temporary files
    // that were not deleted when the installs/updates exited, probably due
    // to a crash. Try delete those temporary files again?
  }

  CoUninitialize();
  if (InstallUtil::InstallSuccessful(install_status))
    return 0;  // For Google Update's benefit we need to return 0 for success
               // cases.
  else
    return install_status;
}
