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

#ifndef CHROME_COMMON_CHROME_PLUGIN_LIB_H_
#define CHROME_COMMON_CHROME_PLUGIN_LIB_H_

#include <hash_map>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/chrome_plugin_api.h"

// A ChromePluginLib is a single Chrome Plugin Library.
// This class is used in the browser process (IO thread), and the plugin process
// (plugin thread).  It should not be accessed on other threads, because it
// issues a NOTIFY_CHROME_PLUGIN_UNLOADED notification.
class ChromePluginLib : public base::RefCounted<ChromePluginLib>  {
 public:
  static ChromePluginLib* Create(const std::wstring& filename,
                                 const CPBrowserFuncs* bfuncs);
  static ChromePluginLib* Find(const std::wstring& filename);
  static void Destroy(const std::wstring& filename);
  static bool IsPluginThread();

  static ChromePluginLib* FromCPID(CPID id) {
    return reinterpret_cast<ChromePluginLib*>(id);
  }

  // Adds Chrome plugins to the NPAPI plugin list.
  static void RegisterPluginsWithNPAPI();

  // Loads all the plugin dlls that are marked as "LoadOnStartup" in the
  // registry. This should only be called in the browser process.
  static void LoadChromePlugins(const CPBrowserFuncs* bfuncs);

  // Unloads all the loaded plugin dlls and cleans up the plugin map.
  static void UnloadAllPlugins();

  // Returns true if the plugin is currently loaded.
  const bool is_loaded() const { return initialized_; }

  // Get the Plugin's function pointer table.
  const CPPluginFuncs& functions() const;

  CPID cpid() { return reinterpret_cast<CPID>(this); }

  const std::wstring& filename() { return filename_; }

  // Plugin API functions

  // Method to call a test function in the plugin, used for unit tests.
  int CP_Test(void* param);

  // The registroy path to search for Chrome Plugins/
  static const TCHAR kRegistryChromePlugins[];

 private:
  friend class base::RefCounted<ChromePluginLib>;

  ChromePluginLib(const std::wstring& filename);
  ~ChromePluginLib();

  // Method to initialize a Plugin.
  // Initialize can be safely called multiple times.
  bool CP_Initialize(const CPBrowserFuncs* bfuncs);

  // Method to shutdown a Plugin.
  void CP_Shutdown();

  // Attempts to load the plugin from the DLL.
  // Returns true if it is a legitimate plugin, false otherwise
  bool Load();

  // Unloading the plugin DLL.
  void Unload();

  std::wstring     filename_;         // the path to the DLL
  HMODULE          module_;           // the opened DLL handle
  bool             initialized_;      // is the plugin initialized

  // DLL exports, looked up by name.
  CP_VersionNegotiateFunc  CP_VersionNegotiate_;
  CP_InitializeFunc       CP_Initialize_;

  // Additional function pointers provided by the plugin.
  CPPluginFuncs    plugin_funcs_;

  // Used for unit tests.
  typedef int (STDCALL *CP_TestFunc)(void*);
  CP_TestFunc             CP_Test_;

  DISALLOW_COPY_AND_ASSIGN(ChromePluginLib);
};

#endif  // CHROME_COMMON_CHROME_PLUGIN_LIB_H_
