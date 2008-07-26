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

// Uninstall registry location
const wchar_t kUninstallRegistryKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Chrome";
const wchar_t kUninstallRegistryValueName[] = L"UninstallString";

// Uninstall registry key that lets user tell Chrome installer not to delete
// extracted files.
const wchar_t kCleanupRegistryKey[] = L"Software\\Google";
const wchar_t kCleanupRegistryValueName[] = L"ChromeInstallerCleanup";

// One gigabyte is the biggest resource size that it can handle.
const int kMaxResourceSize = 1024*1024*1024;

// This is the file that contains the list of files to be linked in the
// executable. This file is updated by the installer generator tool chain.
const wchar_t kManifestFilename[] = L"packed_files.txt";

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER__
