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

#ifndef CHROME_BROWSER_GOOGLE_UPDATE_H_
#define CHROME_BROWSER_GOOGLE_UPDATE_H_

#include <string>

#include "base/basictypes.h"
#include "google_update_idl.h"

class MessageLoop;

// The status of the upgrade. UPGRADE_STARTED and UPGRADE_CHECK_STARTED are
// internal states and will not be reported as results to the listener.
enum GoogleUpdateUpgradeResult {
  // The upgrade has started.
  UPGRADE_STARTED = 0,
  // A check for upgrade has been initiated.
  UPGRADE_CHECK_STARTED,
  // An update is available.
  UPGRADE_IS_AVAILABLE,
  // The upgrade happened successfully.
  UPGRADE_SUCCESSFUL,
  // No need to upgrade, we are up to date.
  UPGRADE_ALREADY_UP_TO_DATE,
  // An error occurred.
  UPGRADE_ERROR,
};

enum GoogleUpdateErrorCode {
  // The upgrade completed successfully (or hasn't been started yet).
  GOOGLE_UPDATE_NO_ERROR = 0,
  // Google Update only supports upgrading if Chrome is installed in the default
  // location. This error will appear for developer builds and with
  // installations unzipped to random locations.
  CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY,
  // Failed to create Google Update JobServer COM class.
  GOOGLE_UPDATE_JOB_SERVER_CREATION_FAILED,
  // Failed to create Google Update OnDemand COM class.
  GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND,
  // Google Update OnDemand COM class reported an error during a check for
  // update (or while upgrading).
  GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR,
  // A call to GetResults failed.
  GOOGLE_UPDATE_GET_RESULT_CALL_FAILED,
  // A call to GetVersionInfo failed.
  GOOGLE_UPDATE_GET_VERSION_INFO_FAILED,
  // An error occurred while upgrading (or while checking for update).
  // Check the Google Update log in %TEMP% for more details.
  GOOGLE_UPDATE_ERROR_UPDATING,
};

// The GoogleUpdateStatusListener interface is used by components to receive
// notifications about the results of an Google Update operation.
class GoogleUpdateStatusListener {
 public:
  // This function is called when Google Update has finished its operation and
  // wants to notify us about the results. |results| represents what the end
  // state is, |error_code| represents what error occurred and |version|
  // specifies what new version Google Update detected (or installed). This
  // value can be a blank string, if the version tag in the Update{} block
  // (in Google Update's server config for Chrome) is blank.
  virtual void OnReportResults(GoogleUpdateUpgradeResult results,
                               GoogleUpdateErrorCode error_code,
                               const std::wstring& version) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// The Google Update class is responsible for communicating with Google Update
// and get it to perform operations on our behalf (for example, CheckForUpdate).
// This class will report back to its parent via the GoogleUpdateStatusListener
// interface and will delete itself after reporting back.
//
////////////////////////////////////////////////////////////////////////////////
class GoogleUpdate {
 public:
  GoogleUpdate();
  virtual ~GoogleUpdate();

  // Ask Google Update to see if a new version is available. If the parameter
  // |install_if_newer| is true then Google Update will also install that new
  // version.
  void CheckForUpdate(bool install_if_newer);

  // Adds/removes a listener to report status back to. Only one listener is
  // maintained at the moment.
  void AddStatusChangeListener(GoogleUpdateStatusListener* listener);
  void RemoveStatusChangeListener();

 private:
  // We need to run the update check on another thread than the main thread, and
  // therefore CheckForUpdate will delegate to this function. |main_loop| points
  // to the message loop that we want the response to come from.
  bool InitiateGoogleUpdateCheck(bool install_if_newer, MessageLoop* main_loop);

  // This function reports the results of the GoogleUpdate operation to the
  // listener. If results indicates an error, the error_code will indicate which
  // error occurred.
  // Note, after this function completes, this object will have deleted itself.
  void ReportResults(GoogleUpdateUpgradeResult results,
                     GoogleUpdateErrorCode error_code);

  // This function reports failure from the Google Update operation to the
  // listener.
  // Note, after this function completes, this object will have deleted itself.
  bool ReportFailure(HRESULT hr, GoogleUpdateErrorCode error_code,
                     MessageLoop* main_loop);

  // The listener who is interested in finding out the result of the operation.
  GoogleUpdateStatusListener* listener_;

  // Which version string Google Update found (if a new one was available).
  // Otherwise, this will be blank.
  std::wstring version_available_;

  DISALLOW_EVIL_CONSTRUCTORS(GoogleUpdate);
};

#endif  // CHROME_BROWSER_GOOGLE_UPDATE_H_
