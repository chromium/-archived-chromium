// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_UPDATE_H_
#define CHROME_BROWSER_GOOGLE_UPDATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "google_update_idl.h"

class MessageLoop;
namespace views {
class Window;
}

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
class GoogleUpdate : public base::RefCountedThreadSafe<GoogleUpdate> {
 public:
  GoogleUpdate();
  virtual ~GoogleUpdate();

  // Ask Google Update to see if a new version is available. If the parameter
  // |install_if_newer| is true then Google Update will also install that new
  // version.
  // |window| should point to a foreground window. This is needed to ensure
  // that Vista/Windows 7 UAC prompts show up in the foreground. It may also
  // be null.
  void CheckForUpdate(bool install_if_newer, views::Window* window);

  // Adds/removes a listener to report status back to. Only one listener is
  // maintained at the moment.
  void AddStatusChangeListener(GoogleUpdateStatusListener* listener);
  void RemoveStatusChangeListener();

 private:
  // We need to run the update check on another thread than the main thread, and
  // therefore CheckForUpdate will delegate to this function. |main_loop| points
  // to the message loop that we want the response to come from.
  // |window| should point to a foreground window. This is needed to ensure that
  // Vista/Windows 7 UAC prompts show up in the foreground. It may also be null.
  bool InitiateGoogleUpdateCheck(bool install_if_newer, views::Window* window,
                                 MessageLoop* main_loop);

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
