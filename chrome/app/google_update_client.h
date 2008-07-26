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
//
// This is the Chrome-GoogleUpdater integration glue. Current features of this
// code include:
// * checks to ensure that client is properly registered with GoogleUpdater
// * versioned directory launcher to allow for completely transparent silent
//   autoupdates


#ifndef CHROME_APP_GOOGLE_UPDATE_CLIENT_H__
#define CHROME_APP_GOOGLE_UPDATE_CLIENT_H__

#include <windows.h>
#include <tchar.h>

#include <string>

namespace google_update {

class GoogleUpdateClient {
 public:
  // callback prototype for GoogleUpdateClient::Launch entry_name
  typedef int (*GoogleUpdateEntry)(HINSTANCE instance, HINSTANCE prev_instance,
                                  wchar_t* command_line, int show_command);

  GoogleUpdateClient();
  virtual ~GoogleUpdateClient();

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
  // The first four arguments are simply WinMain args passed through.
  // - entry_name is the (typedef) GoogleUpdateEntry that is called
  // - ret is an out param with the return value of entry
  // Returns false if unable to load the dll or find entry_name's proc addr.
  bool Launch(HINSTANCE instance, HINSTANCE prev_instance,
              wchar_t* command_line, int show_command, const char* entry_name,
              int* ret);

  // For the client guid, fills in the path to the dir for the exe that
  // calls this function.  (e.g. Program Files/Google/Chrome/ or
  // Documents and Settings\user\Local Settings\Application Data\Google\Chrome)
  // It also sets the user_mode_ depending on whether the exe was called
  // from Program Files location (user_mode_ = false) or not.
  void GetExePathAndInstallMode();

  // Returns the path of the DLL that is going to be loaded.
  // This function can be called only after Init().
  std::wstring GetDLLPath();

  // For the client guid, returns the associated version string, or NULL
  // if Init() was unable to obtain one.
  const wchar_t* GetVersion() const;

 private:
  // disallow copy ctor and operator=
  GoogleUpdateClient(const GoogleUpdateClient&);
  void operator=(const GoogleUpdateClient&);

  // The GUID that this client has registered with GoogleUpdate for autoupdate.
  wchar_t* guid_;
  // The name of the dll to load.
  wchar_t* dll_;
  // The current version of this client registered with GoogleUpdate.
  wchar_t* version_;
  // The location of current chrome.dll.
  wchar_t dll_path_[MAX_PATH];
  // Are we running in user mode or admin mode
  bool user_mode_;
};

}  // namespace google_update

#endif  // CHROME_APP_GOOGLE_UPDATE_CLIENT_H__
