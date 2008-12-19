// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cert_verifier.h"

#include "base/message_loop.h"
#include "base/worker_pool.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"

namespace net {

class CertVerifier::Request :
    public base::RefCountedThreadSafe<CertVerifier::Request> {
 public:
  Request(CertVerifier* verifier,
          X509Certificate* cert,
          const std::string& hostname,
          bool rev_checking_enabled,
          int* cert_status,
          CompletionCallback* callback)
      : cert_(cert),
        hostname_(hostname),
        rev_checking_enabled_(rev_checking_enabled),
        verifier_(verifier),
        cert_status_(cert_status),
        callback_(callback),
        origin_loop_(MessageLoop::current()),
        error_(OK),
        result_(0) {
  }

  ~Request() {}

  void DoVerify() {
    // Running on the worker thread
    error_ = cert_->Verify(hostname_, rev_checking_enabled_, &result_);

    Task* reply = NewRunnableMethod(this, &Request::DoCallback);

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~CertVerifier.
    {
      AutoLock locked(origin_loop_lock_);
      if (origin_loop_) {
        origin_loop_->PostTask(FROM_HERE, reply);
        reply = NULL;
      }
    }

    // Does nothing if it got posted.
    delete reply;
  }

  void DoCallback() {
    // Running on the origin thread.
    DCHECK(error_ || result_);

    // We may have been cancelled!
    if (!verifier_)
      return;

    *cert_status_ = result_;

    // Drop the verifier's reference to us.  Do this before running the
    // callback since the callback might result in the verifier being
    // destroyed.
    verifier_->request_ = NULL;

    callback_->Run(error_);
  }

  void Cancel() {
    verifier_ = NULL;

    AutoLock locked(origin_loop_lock_);
    origin_loop_ = NULL;
  }

 private:
  // Set on the origin thread, read on the worker thread.
  scoped_refptr<X509Certificate> cert_;
  std::string hostname_;
  bool rev_checking_enabled_;

  // Only used on the origin thread (where Verify was called).
  CertVerifier* verifier_;
  int* cert_status_;
  CompletionCallback* callback_;

  // Used to post ourselves onto the origin thread.
  Lock origin_loop_lock_;
  MessageLoop* origin_loop_;

  // Assigned on the worker thread, read on the origin thread.
  int error_;
  int result_;
};

//-----------------------------------------------------------------------------

CertVerifier::CertVerifier() {
}

CertVerifier::~CertVerifier() {
  if (request_)
    request_->Cancel();
}

int CertVerifier::Verify(X509Certificate* cert,
                         const std::string& hostname,
                         bool rev_checking_enabled,
                         int* cert_status,
                         CompletionCallback* callback) {
  DCHECK(!request_) << "verifier already in use";

  // Do a synchronous verification.
  if (!callback) {
    int result;
    int rv = cert->Verify(hostname, rev_checking_enabled, &result);
    *cert_status = result;
    return rv;
  }

  request_ = new Request(this, cert, hostname, rev_checking_enabled,
                         cert_status, callback);

  // Dispatch to worker thread...
  if (!WorkerPool::PostTask(FROM_HERE,
          NewRunnableMethod(request_.get(), &Request::DoVerify), true)) {
    NOTREACHED();
    request_ = NULL;
    return ERR_FAILED;
  }

  return ERR_IO_PENDING;
}

}  // namespace net
