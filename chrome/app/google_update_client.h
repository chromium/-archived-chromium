// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the Chrome-GoogleUpdater integration glue. Current features of this
// code include:
// * checks to ensure that client is properly registered with GoogleUpdater
// * versioned directory launcher to allow for completely transparent silent
//   autoupdates


#ifndef CHROME_APP_GOOGLE_UPDATE_CLIENT_H_
#define CHROME_APP_GOOGLE_UPDATE_CLIENT_H_

#include <windows.h>
#include <tchar.h>

#include <string>

#include "sandbox/src/sandbox_factory.h"

namespace google_update {

class GoogleUpdateClient {
 public:
  GoogleUpdateClient();
  virtual ~GoogleUpdateClient();

  // Returns the path of the DLL that is going to be loaded.
  // This function can be called only after Init().
  std::wstring GetDLLPath();

  // For the client guid, returns the associated version string, or NULL
  // if Init() was unable to obtain one.
  const wchar_t* GetVersion() const;

  // Init must be called prior to other methods.
  // client_guid is the guid that you registered with Google Update when you
  // installed.
  // Returns false if client is not properly registered with GoogleUpdate.  If
  // not registered, autoupdates won't be performed for this client.
  bool Init(const wchar_t* client_guid, const wchar_t* client_dll);

  // Launches your app's main code and initializes Google Update services.
  // - looks up the registered version via GoogleUpdate, loads dll from version
  //   dir (e.g. Program Files/Google/1.0.101.0/chrome.dll) and calls the
  //   entry_name. If chrome.dll is found in this path the version is stored
  //   in the environment block such that subsequent launches invoke the
  //   save dll version.
  // - instance is handle to the current instance of application
  // - sandbox provides information about sandbox services
  // - command_line contains command line parameters
  // - entry_name is the function of type DLL_MAIN that is called
  //   from chrome.dll
  // - ret is an out param with the return value of entry
  // Returns false if unable to load the dll or find entry_name's proc addr.
  bool Launch(HINSTANCE instance, sandbox::SandboxInterfaceInfo* sandbox,
              wchar_t* command_line, const char* entry_name, int* ret);

 private:
  // disallow copy ctor and operator=
  GoogleUpdateClient(const GoogleUpdateClient&);
  void operator=(const GoogleUpdateClient&);

  // The GUID that this client has registered with GoogleUpdate for autoupdate.
  std::wstring guid_;
  // The name of the dll to load.
  std::wstring dll_;
  // The current version of this client registered with GoogleUpdate.
  wchar_t* version_;
  // The location of current chrome.dll.
  wchar_t dll_path_[MAX_PATH];
};

}  // namespace google_update

#endif  // CHROME_APP_GOOGLE_UPDATE_CLIENT_H_
