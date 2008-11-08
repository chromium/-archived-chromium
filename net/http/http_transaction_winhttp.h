// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_WINHTTP_H__
#define NET_HTTP_HTTP_TRANSACTION_WINHTTP_H__

#include <windows.h>
#include <winhttp.h>

#include <string>

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_service.h"

namespace net {

class UploadDataStream;

class HttpTransactionWinHttp : public HttpTransaction {
  class Session;  // Represents a WinHttp session handle.
  class SessionCallback;
 public:
  // Instantiate this class, and use it to create HttpTransaction objects.
  class Factory : public HttpTransactionFactory {
   public:
    Factory() : session_(NULL), proxy_info_(NULL), is_suspended_(false) {}
    explicit Factory(const ProxyInfo* info)
        : session_(NULL), proxy_info_(NULL), is_suspended_(false) {
      if (info) {
        proxy_info_.reset(new ProxyInfo());
        proxy_info_->Use(*info);
      }
    }
    ~Factory();

    virtual HttpTransaction* CreateTransaction();
    virtual HttpCache* GetCache();
    virtual void Suspend(bool suspend);

   private:
    Session* session_;
    scoped_ptr<ProxyInfo> proxy_info_;
    bool is_suspended_;
    DISALLOW_EVIL_CONSTRUCTORS(Factory);
  };

  virtual ~HttpTransactionWinHttp();

  // HttpTransaction methods:
  virtual int Start(const HttpRequestInfo*, CompletionCallback*);
  virtual int RestartIgnoringLastError(CompletionCallback*);
  virtual int RestartWithAuth(const std::wstring&,
                              const std::wstring&,
                              CompletionCallback*);
  virtual int Read(char*, int, CompletionCallback*);
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

  static void CALLBACK StatusCallback(HINTERNET handle,
                                      DWORD_PTR context,
                                      DWORD status,
                                      LPVOID status_info,
                                      DWORD status_info_len);

  // Called via the message loop in response to a WinHttp status callback.
  void HandleStatusCallback(DWORD status,
                            DWORD_PTR result,
                            DWORD error,
                            DWORD secure_failure);

 private:
  friend class Factory;

  // Methods ------------------------------------------------------------------

  HttpTransactionWinHttp(Session* session, const ProxyInfo* info);

  void DoCallback(int rv);
  int ResolveProxy();
  bool OpenRequest();
  int SendRequest();
  bool ReopenRequest();
  int Restart(CompletionCallback* callback);
  int DidResolveProxy();
  int DidReceiveError(DWORD error, DWORD secure_failure);
  int DidSendRequest();
  int DidWriteData(DWORD num_bytes);
  int DidReadData(DWORD num_bytes);
  int DidReceiveHeaders();

  int PopulateAuthChallenge();
  void ApplyAuth();

  std::string GetRequestHeaders() const;
  X509Certificate* GetServerCertificate() const;
  int GetSecurityBits() const;
  void PopulateSSLInfo(DWORD secure_failure);

  void OnProxyInfoAvailable(int result);

  // Variables ----------------------------------------------------------------

  Session* session_;
  const HttpRequestInfo* request_;

  // A copy of request_->load_flags that we can modify in
  // RestartIgnoringLastError.
  int load_flags_;

  // Optional auth data for proxy and origin server.
  scoped_refptr<AuthData> proxy_auth_;
  scoped_refptr<AuthData> server_auth_;

  // The key for looking up the auth data in the auth cache, consisting
  // of the scheme, host, and port of the request URL and the realm in
  // the auth challenge.
  std::string proxy_auth_cache_key_;
  std::string server_auth_cache_key_;

  // The peer of the connection.  For a direct connection, this is the
  // destination server.  If we use a proxy, this is the proxy.
  std::string connect_peer_;

  // The last error from SendRequest that occurred.  Used by
  // RestartIgnoringLastError to adjust load_flags_ to ignore this error.
  DWORD last_error_;

  // This value is non-negative when we are streaming a response over a
  // non-keepalive connection.  We decrement this value as we receive data to
  // allow us to discover end-of-file.  This is used to workaround a bug in
  // WinHttp (see bug 1063336).
  int64 content_length_remaining_;

  ProxyInfo proxy_info_;
  ProxyService::PacRequest* pac_request_;
  CompletionCallbackImpl<HttpTransactionWinHttp> proxy_callback_;

  HttpResponseInfo response_;
  CompletionCallback* callback_;
  HINTERNET connect_handle_;
  HINTERNET request_handle_;
  scoped_refptr<SessionCallback> session_callback_;
  scoped_ptr<UploadDataStream> upload_stream_;
  uint64 upload_progress_;

  // True if the URL's scheme is https.
  bool is_https_;

  // True if the SSL server doesn't support TLS but also cannot correctly
  // negotiate with a TLS-enabled client to use SSL 3.0.  The workaround is
  // for the client to downgrade to SSL 3.0 and retry the SSL handshake.
  bool is_tls_intolerant_;

  // True if revocation checking of the SSL server certificate is enabled.
  bool rev_checking_enabled_;

  // A flag to indicate whether or not we already have proxy information.
  // If false, we will attempt to resolve proxy information from the proxy
  // service.  This flag is set to true if proxy information is supplied by
  // a client.
  bool have_proxy_info_;

  // If WinHTTP is still using our caller's data (upload data or read buffer),
  // we need to wait for the HANDLE_CLOSING status notification after we close
  // the request handle.
  //
  // There are only five WinHTTP functions that work asynchronously (listed in
  // the order in which they're called):
  // WinHttpSendRequest, WinHttpWriteData, WinHttpReceiveResponse,
  // WinHttpQueryDataAvailable, WinHttpReadData.
  // WinHTTP is using our caller's data during the two time intervals:
  // - From the first WinHttpWriteData call to the completion of the last
  //   WinHttpWriteData call.  (We may call WinHttpWriteData multiple times.)
  // - From the WinHttpReadData call to its completion.
  // We set need_to_wait_for_handle_closing_ to true at the beginning of these
  // time intervals and set it to false at the end.  We're not sandwiching the
  // intervals as tightly as possible.  (To do that, we'd need to give WinHTTP
  // worker threads access to the need_to_wait_for_handle_closing_ flag and
  // worry about thread synchronization issues.)
  bool need_to_wait_for_handle_closing_;

  // True if we have called WinHttpRequestThrottle::SubmitRequest.
  bool request_submitted_;

  DISALLOW_EVIL_CONSTRUCTORS(HttpTransactionWinHttp);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_WINHTTP_H__

