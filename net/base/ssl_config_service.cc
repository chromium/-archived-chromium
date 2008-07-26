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

#include "net/base/ssl_config_service.h"

#include "base/registry.h"

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
