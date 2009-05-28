// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

// File name used in the user data dir to indicate consent.
static const char kConsentToSendStats[] = "Consent To Send Stats";

// static
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  FilePath consent_file;
  PathService::Get(chrome::DIR_USER_DATA, &consent_file);
  consent_file = consent_file.Append(kConsentToSendStats);
  return file_util::PathExists(consent_file);
}

// static
bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  FilePath consent_dir;
  PathService::Get(chrome::DIR_USER_DATA, &consent_dir);
  if (!file_util::DirectoryExists(consent_dir))
    return false;

  FilePath consent_file = consent_dir.AppendASCII(kConsentToSendStats);
  if (consented)
    return file_util::WriteFile(consent_file, "", 0) == 0;
  else
    return file_util::Delete(consent_file, false);
}

// static
bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  NOTIMPLEMENTED();
  return false;
}
