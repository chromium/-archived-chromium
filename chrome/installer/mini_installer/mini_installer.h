// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER__
#define CHROME_INSTALLER_MINI_INSTALLER__

// The windows command line to uncompress a LZ compressed file. It is a define
// because we need the string to be writable. We don't need the full path
// since it is located in windows\system32 and is available since windows2k.
#define UNCOMPRESS_CMD L"expand.exe -r "

namespace mini_installer {

// The installer filename. The installer can be compressed or uncompressed.
const wchar_t kSetupName[] = L"setup.exe";
const wchar_t kSetupLZName[] = L"setup.ex_";

// The resource types that would be unpacked from the mini installer.
// 'BN' is uncompressed binary and 'BL' is LZ compressed binary.
const wchar_t kBinResourceType[] = L"BN";
const wchar_t kLZCResourceType[] = L"BL";
const wchar_t kLZMAResourceType[] = L"B7";

// Registry key to get uninstall command
const wchar_t kUninstallRegistryValueName[] = L"UninstallString";
// Registry key that tells Chrome installer not to delete extracted files.
const wchar_t kCleanupRegistryValueName[] = L"ChromeInstallerCleanup";
// Paths for the above two registry keys
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kUninstallRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Google Chrome";
const wchar_t kCleanupRegistryKey[] = L"Software\\Google";
#else
const wchar_t kUninstallRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chromium";
const wchar_t kCleanupRegistryKey[] = L"Software\\Chromium";
#endif

// One gigabyte is the biggest resource size that it can handle.
const int kMaxResourceSize = 1024*1024*1024;

// This is the file that contains the list of files to be linked in the
// executable. This file is updated by the installer generator tool chain.
const wchar_t kManifestFilename[] = L"packed_files.txt";

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER__

