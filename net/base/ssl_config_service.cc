// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"

#include "base/registry.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

static const int kConfigUpdateInterval = 10;  // seconds

static const wchar_t kInternetSettingsSubKeyName[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

static const wchar_t kRevocationValueName[] = L"CertificateRevocation";

static const wchar_t kProtocolsValueName[] = L"SecureProtocols";

// In SecureProtocols, each SSL version is represented by a bit:
//   SSL 2.0: 0x08
//   SSL 3.0: 0x20
//   TLS 1.0: 0x80
// The bits are OR'ed to form the DWORD value.  So 0xa0 means SSL 3.0 and
// TLS 1.0.
enum {
  SSL2 = 0x08,
  SSL3 = 0x20,
  TLS1 = 0x80
};

// If CertificateRevocation or SecureProtocols is missing, IE uses a default
// value.  Unfortunately the default is IE version specific.  We use WinHTTP's
// default.
enum {
  REVOCATION_DEFAULT = 0,
  PROTOCOLS_DEFAULT = SSL3 | TLS1
};

SSLConfigService::SSLConfigService() {
  UpdateConfig(TimeTicks::Now());
}

SSLConfigService::SSLConfigService(TimeTicks now) {
  UpdateConfig(now);
}

void SSLConfigService::GetSSLConfigAt(SSLConfig* config, TimeTicks now) {
  if (now - config_time_ > TimeDelta::FromSeconds(kConfigUpdateInterval))
    UpdateConfig(now);
  *config = config_info_;
}

// static
bool SSLConfigService::GetSSLConfigNow(SSLConfig* config) {
  RegKey internet_settings;
  if (!internet_settings.Open(HKEY_CURRENT_USER, kInternetSettingsSubKeyName,
                              KEY_READ))
    return false;

  DWORD revocation;
  if (!internet_settings.ReadValueDW(kRevocationValueName, &revocation))
    revocation = REVOCATION_DEFAULT;

  DWORD protocols;
  if (!internet_settings.ReadValueDW(kProtocolsValueName, &protocols))
    protocols = PROTOCOLS_DEFAULT;

  config->rev_checking_enabled = (revocation != 0);
  config->ssl2_enabled = ((protocols & SSL2) != 0);
  config->ssl3_enabled = ((protocols & SSL3) != 0);
  config->tls1_enabled = ((protocols & TLS1) != 0);

  return true;
}

// static
void SSLConfigService::SetRevCheckingEnabled(bool enabled) {
  DWORD value = enabled;
  RegKey internet_settings(HKEY_CURRENT_USER, kInternetSettingsSubKeyName,
                           KEY_WRITE);
  internet_settings.WriteValue(kRevocationValueName, value);
}

// static
void SSLConfigService::SetSSL2Enabled(bool enabled) {
  RegKey internet_settings(HKEY_CURRENT_USER, kInternetSettingsSubKeyName,
                           KEY_READ | KEY_WRITE);
  DWORD value;
  if (!internet_settings.ReadValueDW(kProtocolsValueName, &value))
    value = PROTOCOLS_DEFAULT;
  if (enabled)
    value |= SSL2;
  else
    value &= ~SSL2;
  internet_settings.WriteValue(kProtocolsValueName, value);
}

void SSLConfigService::UpdateConfig(TimeTicks now) {
  GetSSLConfigNow(&config_info_);
  config_time_ = now;
}

}  // namespace net
