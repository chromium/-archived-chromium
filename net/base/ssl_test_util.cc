// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <algorithm>

#include "net/base/ssl_test_util.h"

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
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "net/base/tcp_pinger.h"
#include "net/base/host_resolver.h"
#include "net/base/tcp_client_socket.h"
#include "net/base/test_completion_callback.h"
#include "testing/platform_test.h"

namespace {

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

  // TODO(dkegel): figure out how to get this to only happen once?
  return cert;
}
#endif

}  // namespace

namespace net {

// static
const char TestServerLauncher::kHostName[] = "127.0.0.1";
const char TestServerLauncher::kMismatchedHostName[] = "localhost";
const int TestServerLauncher::kOKHTTPSPort = 9443;
const int TestServerLauncher::kBadHTTPSPort = 9666;

// The issuer name of the cert that should be trusted for the test to work.
const wchar_t TestServerLauncher::kCertIssuerName[] = L"Test CA";

TestServerLauncher::TestServerLauncher() : process_handle_(NULL)
#if defined(OS_LINUX)
, cert_(NULL)
#endif
{
  PathService::Get(base::DIR_SOURCE_ROOT, &data_dir_);
  data_dir_ = data_dir_.Append(FILE_PATH_LITERAL("net"))
                       .Append(FILE_PATH_LITERAL("data"))
                       .Append(FILE_PATH_LITERAL("ssl"));
  cert_dir_ = data_dir_.Append(FILE_PATH_LITERAL("certificates"));
}

namespace {

void AppendToPythonPath(FilePath dir) {
  // Do nothing if dir already on path.

#if defined(OS_WIN)
  const wchar_t kPythonPath[] = L"PYTHONPATH";
  // FIXME(dkegel): handle longer PYTHONPATH variables
  wchar_t oldpath[4096];
  if (GetEnvironmentVariable(kPythonPath, oldpath, sizeof(oldpath)) == 0) {
    SetEnvironmentVariableW(kPythonPath, dir.value().c_str());
  } else if (!wcsstr(oldpath, dir.value().c_str())) {
    std::wstring newpath(oldpath);
    newpath.append(L":");
    newpath.append(dir.value());
    SetEnvironmentVariableW(kPythonPath, newpath.c_str());
  }
#elif defined(OS_POSIX)
  const char kPythonPath[] = "PYTHONPATH";
  const char* oldpath = getenv(kPythonPath);
  if (!oldpath) {
    setenv(kPythonPath, dir.value().c_str(), 1);
  } else if (!strstr(oldpath, dir.value().c_str())) {
    std::string newpath(oldpath);
    newpath.append(":");
    newpath.append(dir.value());
    setenv(kPythonPath, newpath.c_str(), 1);
  }
#endif
}

}  // end namespace

void TestServerLauncher::SetPythonPath() {
  FilePath third_party_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &third_party_dir));
  third_party_dir = third_party_dir.Append(FILE_PATH_LITERAL("third_party"));

  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("tlslite")));
  AppendToPythonPath(third_party_dir.Append(FILE_PATH_LITERAL("pyftpdlib")));
}

bool TestServerLauncher::Start(Protocol protocol,
                               const std::string& host_name, int port,
                               const FilePath& document_root,
                               const FilePath& cert_path) {
  if (!TestServerLauncher::CheckCATrusted())
    return false;

  std::string port_str = IntToString(port);

  // Get path to python server script
  FilePath testserver_path;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &testserver_path))
    return false;
  testserver_path = testserver_path
      .Append(FILE_PATH_LITERAL("net"))
      .Append(FILE_PATH_LITERAL("tools"))
      .Append(FILE_PATH_LITERAL("testserver"))
      .Append(FILE_PATH_LITERAL("testserver.py"));

  FilePath test_data_directory;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_data_directory);
  test_data_directory = test_data_directory.Append(document_root);

#if defined(OS_LINUX)
  if (!cert_ && !cert_path.value().empty()) {
    cert_ = reinterpret_cast<PrivateCERTCertificate*>(
            LoadTemporaryCert(GetRootCertPath()));
    DCHECK(cert_);
  }
#endif

  SetPythonPath();

#if defined(OS_WIN)
  // Get path to python interpreter
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &python_runtime_))
    return false;
  python_runtime_ = python_runtime_
      .Append(FILE_PATH_LITERAL("third_party"))
      .Append(FILE_PATH_LITERAL("python_24"))
      .Append(FILE_PATH_LITERAL("python.exe"));

  std::wstring command_line =
      L"\"" + python_runtime_.ToWStringHack() + L"\" " +
      L"\"" + testserver_path.ToWStringHack() +
      L"\" --port=" + UTF8ToWide(port_str) +
      L" --data-dir=\"" + test_data_directory.ToWStringHack() + L"\"";
  if (protocol == ProtoFTP)
    command_line.append(L" -f");
  if (!cert_path.value().empty()) {
    command_line.append(L" --https=\"");
    command_line.append(cert_path.ToWStringHack());
    command_line.append(L"\"");
  }

  if (!base::LaunchApp(command_line, false, true, &process_handle_)) {
    LOG(ERROR) << "Failed to launch " << command_line;
    return false;
  }
#elif defined(OS_POSIX)
  std::vector<std::string> command_line;
  command_line.push_back("python");
  command_line.push_back(WideToUTF8(testserver_path.ToWStringHack()));
  command_line.push_back("--port=" + port_str);
  command_line.push_back("--data-dir=" +
                         WideToUTF8(test_data_directory.ToWStringHack()));
  if (protocol == ProtoFTP)
    command_line.push_back("-f");
  if (!cert_path.value().empty())
    command_line.push_back("--https=" + WideToUTF8(cert_path.ToWStringHack()));

  base::file_handle_mapping_vector no_mappings;
  LOG(INFO) << "Trying to launch " << command_line[0] << " ...";
  if (!base::LaunchApp(command_line, no_mappings, false, &process_handle_)) {
    LOG(ERROR) << "Failed to launch " << command_line[0] << " ...";
    return false;
  }
#endif

  // Let the server start, then verify that it's up.
  // Our server is Python, and takes about 500ms to start
  // up the first time, and about 200ms after that.
  if (!Wait(host_name, port)) {
    LOG(ERROR) << "Failed to connect to server";
    Stop();
    return false;
  }

  LOG(INFO) << "Started on port " << port_str;
  return true;
}

bool TestServerLauncher::Wait(const std::string& host_name, int port) {
  // Verify that the webserver is actually started.
  // Otherwise tests can fail if they run faster than Python can start.
  net::AddressList addr;
  net::HostResolver resolver;
  int rv = resolver.Resolve(host_name, port, &addr, NULL);
  if (rv != net::OK)
    return false;

  net::TCPPinger pinger(addr);
  rv = pinger.Ping();
  return rv == net::OK;
}

void TestServerLauncher::Stop() {
  if (!process_handle_)
    return;

  base::KillProcess(process_handle_, 1, true);

#if defined(OS_WIN)
  CloseHandle(process_handle_);
#endif

  process_handle_ = NULL;
  LOG(INFO) << "Stopped.";
}

TestServerLauncher::~TestServerLauncher() {
#if defined(OS_LINUX)
  if (cert_)
    CERT_DestroyCertificate(reinterpret_cast<CERTCertificate*>(cert_));
#endif
  Stop();
}

FilePath TestServerLauncher::GetRootCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("root_ca_cert.crt");
  return path;
}

FilePath TestServerLauncher::GetOKCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("ok_cert.pem");
  return path;
}

FilePath TestServerLauncher::GetExpiredCertPath() {
  FilePath path(cert_dir_);
  path = path.AppendASCII("expired_cert.pem");
  return path;
}

bool TestServerLauncher::CheckCATrusted() {
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

}  // namespace net

