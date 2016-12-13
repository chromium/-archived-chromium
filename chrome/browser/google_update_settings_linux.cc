// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"

namespace google_update {
std::string linux_guid;
}

// File name used in the user data dir to indicate consent.
static const char kConsentToSendStats[] = "Consent To Send Stats";
static const int kGuidLen = sizeof(uint64) * 4;  // 128 bits -> 32 bytes hex.

// static
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  FilePath consent_file;
  PathService::Get(chrome::DIR_USER_DATA, &consent_file);
  consent_file = consent_file.Append(kConsentToSendStats);
  bool r = file_util::ReadFileToString(consent_file,
                                       &google_update::linux_guid);
  google_update::linux_guid.resize(kGuidLen, '0');
  return r;
}

// static
bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  FilePath consent_dir;
  PathService::Get(chrome::DIR_USER_DATA, &consent_dir);
  if (!file_util::DirectoryExists(consent_dir))
    return false;

  FilePath consent_file = consent_dir.AppendASCII(kConsentToSendStats);
  if (consented) {
    uint64 random;
    google_update::linux_guid.clear();
    for (int i = 0; i < 2; i++) {
      random = base::RandUint64();
      google_update::linux_guid += HexEncode(&random, sizeof(uint64));
    }
    const char* c_str = google_update::linux_guid.c_str();
    return file_util::WriteFile(consent_file, c_str, kGuidLen) == kGuidLen;
  }
  else {
    google_update::linux_guid .clear();
    google_update::linux_guid.resize(kGuidLen, '0');
    return file_util::Delete(consent_file, false);
  }
}

// static
bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  NOTIMPLEMENTED();
  return false;
}
