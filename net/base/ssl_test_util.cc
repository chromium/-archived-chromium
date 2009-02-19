// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <algorithm>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wincrypt.h>
#elif defined(OS_LINUX)
#include <nspr.h>
#include <nss.h>
#include <secerr.h>
// Work around https://bugzilla.mozilla.org/show_bug.cgi?id=455424
// until NSS 3.12.2 comes out and we update to it.
#define Lock FOO_NSS_Lock
#include <ssl.h>
#include <sslerr.h>
#include <pk11pub.h>
#undef Lock
#include "base/nss_init.h"
#endif

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"

#include "net/base/ssl_test_util.h"

// static
const char SSLTestUtil::kHostName[] = "127.0.0.1";
const int SSLTestUtil::kOKHTTPSPort = 9443;
const int SSLTestUtil::kBadHTTPSPort = 9666;

// The issuer name of the cert that should be trusted for the test to work.
const wchar_t SSLTestUtil::kCertIssuerName[] = L"Test CA";

#if defined(OS_LINUX)
static CERTCertificate* LoadTemporaryCert(const FilePath& filename) {
  base::EnsureNSSInit();

  std::string rawcert;
  if (!file_util::ReadFileToString(filename.ToWStringHack(), &rawcert)) {
    LOG(ERROR) << "Can't load certificate " << filename.ToWStringHack();
    return NULL;
  }

  CERTCertificate *cert;
  cert = CERT_DecodeCertFromPackage(const_cast<char *>(rawcert.c_str()),
                                    rawcert.length());
  if (!cert) {
    LOG(ERROR) << "Can't convert certificate " << filename.ToWStringHack();
    return NULL;
  }

  // TODO(port): remove this const_cast after NSS 3.12.3 is released
  CERTCertTrust trust;
  int rv = CERT_DecodeTrustString(&trust, const_cast<char *>("TCu,Cu,Tu"));
  if (rv != SECSuccess) {
    LOG(ERROR) << "Can't decode trust string";
    CERT_DestroyCertificate(cert);
    return NULL;
  }

  rv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert, &trust);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Can't change trust for certificate " 
               << filename.ToWStringHack();
    CERT_DestroyCertificate(cert);
    return NULL;
  }

  LOG(INFO) << "Loaded temporary certificate " << filename.ToWStringHack();
  return cert;
}
#endif

SSLTestUtil::SSLTestUtil() {
  PathService::Get(base::DIR_SOURCE_ROOT, &cert_dir_);
  cert_dir_ = cert_dir_.AppendASCII("net");
  cert_dir_ = cert_dir_.AppendASCII("data");
  cert_dir_ = cert_dir_.AppendASCII("ssl");
  cert_dir_ = cert_dir_.AppendASCII("certificates");

#if defined(OS_LINUX)
  cert_ = reinterpret_cast<PrivateCERTCertificate*>(
            LoadTemporaryCert(GetRootCertPath()));
  DCHECK(cert_);
#endif
}

SSLTestUtil::~SSLTestUtil() {
#if defined(OS_LINUX)
  if (cert_)
    CERT_DestroyCertificate(reinterpret_cast<CERTCertificate*>(cert_));
#endif
}

FilePath SSLTestUtil::GetRootCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("root_ca_cert.crt");
  return path;
}

FilePath SSLTestUtil::GetOKCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("ok_cert.pem");
  return path;
}

FilePath SSLTestUtil::GetExpiredCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("expired_cert.pem");
  return path;
}

bool SSLTestUtil::CheckCATrusted() {
// TODO(port): Port either this or LoadTemporaryCert to MacOSX.
#if defined(OS_WIN)
  HCERTSTORE cert_store = CertOpenSystemStore(NULL, L"ROOT");
  if (!cert_store) {
    LOG(ERROR) << " could not open trusted root CA store";
    return false;
  }
  PCCERT_CONTEXT cert =
      CertFindCertificateInStore(cert_store,
                                 X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                 0,
                                 CERT_FIND_ISSUER_STR,
                                 kCertIssuerName,
                                 NULL);
  if (cert)
    CertFreeCertificateContext(cert);
  CertCloseStore(cert_store, 0);

  if (!cert) {
    LOG(ERROR) << " TEST CONFIGURATION ERROR: you need to import the test ca "
                  "certificate to your trusted roots for this test to work. "
                  "For more info visit:\n"
                  "http://dev.chromium.org/developers/testing\n";
    return false;
  }
#endif
  return true;
}
