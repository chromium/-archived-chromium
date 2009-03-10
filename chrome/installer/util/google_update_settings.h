// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_

#include <string>

#include "base/basictypes.h"

// This class provides accessors to the Google Update 'ClientState' information
// that recorded when the user downloads the chrome installer. It is
// google_update.exe responsability to write the initial values.
class GoogleUpdateSettings {
 public:
  // Returns whether the user has given consent to collect UMA data and send
  // crash dumps to Google. This information is collected by the web server
  // used to download the chrome installer.
  static bool GetCollectStatsConsent();

  // Sets the user consent to send UMA and crash dumps to Google. Returns
  // false if the setting could not be recorded.
  static bool SetCollectStatsConsent(bool consented);

  // Returns in 'browser' the browser used to download chrome as recorded
  // Google Update. Returns false if the information is not available.
  static bool GetBrowser(std::wstring* browser);

  // Returns in 'language' the language selected by the user when downloading
  // chrome. This information is collected by the web server used to download
  // the chrome installer. Returns false if the information is not available.
  static bool GetLanguage(std::wstring* language);

  // Returns in 'brand' the RLZ brand code or distribution tag that has been
  // assigned to a partner. Returns false if the information is not available.
  static bool GetBrand(std::wstring* brand);

  // Returns in 'client' the RLZ referral available for some distribution
  // partners. This value does not exist for most chrome or chromium installs.
  static bool GetReferral(std::wstring* referral);

  // Overwrites the current value of the referral with an empty string. Returns
  // true if this operation succeeded.
  static bool ClearReferral();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
