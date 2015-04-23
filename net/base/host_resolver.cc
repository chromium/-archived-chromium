// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#elif defined(OS_POSIX)
#include <netdb.h>
#include <sys/socket.h>
#endif
#if defined(OS_LINUX)
#include <resolv.h>
#endif

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/worker_pool.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"

#if defined(OS_LINUX)
#include "base/singleton.h"
#include "base/thread_local_storage.h"
#endif

#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif

namespace net {

//-----------------------------------------------------------------------------

static HostMapper* host_mapper;

std::string HostMapper::MapUsingPrevious(const std::string& host) {
  return previous_mapper_.get() ? previous_mapper_->Map(host) : host;
}

HostMapper* SetHostMapper(HostMapper* value) {
  std::swap(host_mapper, value);
  return value;
}

#if defined(OS_LINUX)
// On Linux changes to /etc/resolv.conf can go unnoticed thus resulting in
// DNS queries failing either because nameservers are unknown on startup
// or because nameserver info has changed as a result of e.g. connecting to
// a new network. Some distributions patch glibc to stat /etc/resolv.conf
// to try to automatically detect such changes but these patches are not
// universal and even patched systems such as Jaunty appear to need calls
// to res_ninit to reload the nameserver information in different threads.
//
// We adopt the Mozilla solution here which is to call res_ninit when
// lookups fail and to rate limit the reloading to once per second per
// thread.

// Keep a timer per calling thread to rate limit the calling of res_ninit.
class DnsReloadTimer {
 public:
  DnsReloadTimer() {
    tls_index_.Initialize(SlotReturnFunction);
  }

  ~DnsReloadTimer() { }

  // Check if the timer for the calling thread has expired. When no
  // timer exists for the calling thread, create one.
  bool Expired() {
    const base::TimeDelta kRetryTime = base::TimeDelta::FromSeconds(1);
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks* timer_ptr =
      static_cast<base::TimeTicks*>(tls_index_.Get());

    if (!timer_ptr) {
      timer_ptr = new base::TimeTicks();
      *timer_ptr = base::TimeTicks::Now();
      tls_index_.Set(timer_ptr);
      // Return true to reload dns info on the first call for each thread.
      return true;
    } else if (now - *timer_ptr > kRetryTime) {
      *timer_ptr = now;
      return true;
    } else {
      return false;
    }
  }

  // Free the allocated timer.
  static void SlotReturnFunction(void* data) {
    base::TimeTicks* tls_data = static_cast<base::TimeTicks*>(data);
    delete tls_data;
  }

 private:
  // We use thread local storage to identify which base::TimeTicks to
  // interact with.
  static ThreadLocalStorage::Slot tls_index_ ;

  DISALLOW_COPY_AND_ASSIGN(DnsReloadTimer);
};

// A TLS slot to the TimeTicks for the current thread.
// static
ThreadLocalStorage::Slot DnsReloadTimer::tls_index_(base::LINKER_INITIALIZED);

#endif  // defined(OS_LINUX)

static int HostResolverProc(const std::string& host, struct addrinfo** out) {
  struct addrinfo hints = {0};
  hints.ai_family = AF_UNSPEC;

#if defined(OS_WIN)
  // DO NOT USE AI_ADDRCONFIG ON WINDOWS.
  //
  // The following comment in <winsock2.h> is the best documentation I found
  // on AI_ADDRCONFIG for Windows:
  //   Flags used in "hints" argument to getaddrinfo()
  //       - AI_ADDRCONFIG is supported starting with Vista
  //       - default is AI_ADDRCONFIG ON whether the flag is set or not
  //         because the performance penalty in not having ADDRCONFIG in
  //         the multi-protocol stack environment is severe;
  //         this defaulting may be disabled by specifying the AI_ALL flag,
  //         in that case AI_ADDRCONFIG must be EXPLICITLY specified to
  //         enable ADDRCONFIG behavior
  //
  // Not only is AI_ADDRCONFIG unnecessary, but it can be harmful.  If the
  // computer is not connected to a network, AI_ADDRCONFIG causes getaddrinfo
  // to fail with WSANO_DATA (11004) for "localhost", probably because of the
  // following note on AI_ADDRCONFIG in the MSDN getaddrinfo page:
  //   The IPv4 or IPv6 loopback address is not considered a valid global
  //   address.
  // See http://crbug.com/5234.
  hints.ai_flags = 0;
#else
  hints.ai_flags = AI_ADDRCONFIG;
#endif

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host.c_str(), NULL, &hints, out);
#if defined(OS_LINUX)
  net::DnsReloadTimer* dns_timer = Singleton<net::DnsReloadTimer>::get();
  // If we fail, re-initialise the resolver just in case there have been any
  // changes to /etc/resolv.conf and retry. See http://crbug.com/11380 for info.
  if (err && dns_timer->Expired()) {
    res_nclose(&_res);
    if (!res_ninit(&_res))
      err = getaddrinfo(host.c_str(), NULL, &hints, out);
  }
#endif

  return err ? ERR_NAME_NOT_RESOLVED : OK;
}

static int ResolveAddrInfo(HostMapper* mapper, const std::string& host,
                           struct addrinfo** out) {
  if (mapper) {
    std::string mapped_host = mapper->Map(host);

    if (mapped_host.empty())
      return ERR_NAME_NOT_RESOLVED;

    return HostResolverProc(mapped_host, out);
  } else {
    return HostResolverProc(host, out);
  }
}

//-----------------------------------------------------------------------------

class HostResolver::Request {
 public:
  Request(int id, const RequestInfo& info, CompletionCallback* callback,
          AddressList* addresses)
      : id_(id), info_(info), job_(NULL), callback_(callback),
        addresses_(addresses) {}

  // Mark the request as cancelled.
  void MarkAsCancelled() {
    job_ = NULL;
    callback_ = NULL;
    addresses_ = NULL;
  }

  bool was_cancelled() const {
    return callback_ == NULL;
  }

  void set_job(Job* job) {
    DCHECK(job != NULL);
    // Identify which job the request is waiting on.
    job_ = job;
  }

  void OnComplete(int error, const AddressList& addrlist) {
    if (error == OK)
      addresses_->SetFrom(addrlist, port());
    callback_->Run(error);
  }

  int port() const {
    return info_.port();
  }

  Job* job() const {
    return job_;
  }

  int id() const {
    return id_;
  }

  const RequestInfo& info() const {
    return info_;
  }

 private:
  // Unique ID for this request. Used by observers to identify requests.
  int id_;

  // The request info that started the request.
  RequestInfo info_;

  // The resolve job (running in worker pool) that this request is dependent on.
  Job* job_;

  // The user's callback to invoke when the request completes.
  CompletionCallback* callback_;

  // The address list to save result into.
  AddressList* addresses_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

//-----------------------------------------------------------------------------

// This class represents a request to the worker pool for a "getaddrinfo()"
// call.
class HostResolver::Job : public base::RefCountedThreadSafe<HostResolver::Job> {
 public:
  Job(HostResolver* resolver, const std::string& host)
      : host_(host),
        resolver_(resolver),
        origin_loop_(MessageLoop::current()),
        host_mapper_(host_mapper),
        error_(OK),
        results_(NULL) {
  }

  ~Job() {
    if (results_)
      freeaddrinfo(results_);

    // Free the requests attached to this job.
    STLDeleteElements(&requests_);
  }

  // Attaches a request to this job. The job takes ownership of |req| and will
  // take care to delete it.
  void AddRequest(HostResolver::Request* req) {
    req->set_job(this);
    requests_.push_back(req);
  }

  // Called from origin loop.
  void Start() {
    // Dispatch the job to a worker thread.
    if (!WorkerPool::PostTask(FROM_HERE,
            NewRunnableMethod(this, &Job::DoLookup), true)) {
      NOTREACHED();

      // Since we could be running within Resolve() right now, we can't just
      // call OnLookupComplete().  Instead we must wait until Resolve() has
      // returned (IO_PENDING).
      error_ = ERR_UNEXPECTED;
      MessageLoop::current()->PostTask(
          FROM_HERE, NewRunnableMethod(this, &Job::OnLookupComplete));
    }
  }

  // Cancels the current job. Callable from origin thread.
  void Cancel() {
    HostResolver* resolver = resolver_;
    resolver_ = NULL;

    // Mark the job as cancelled, so when worker thread completes it will
    // not try to post completion to origin loop.
    {
      AutoLock locked(origin_loop_lock_);
      origin_loop_ = NULL;
    }

    // We don't have to do anything further to actually cancel the requests
    // that were attached to this job (since they are unreachable now).
    // But we will call HostResolver::CancelRequest(Request*) on each one
    // in order to notify any observers.
    for (RequestsList::const_iterator it = requests_.begin();
         it != requests_.end(); ++it) {
      HostResolver::Request* req = *it;
      if (!req->was_cancelled())
        resolver->CancelRequest(req);
    }
  }

  // Called from origin thread.
  bool was_cancelled() const {
    return resolver_ == NULL;
  }

  // Called from origin thread.
  const std::string& host() const {
    return host_;
  }

  // Called from origin thread.
  const RequestsList& requests() const {
    return requests_;
  }

 private:
  void DoLookup() {
    // Running on the worker thread
    error_ = ResolveAddrInfo(host_mapper_, host_, &results_);

    Task* reply = NewRunnableMethod(this, &Job::OnLookupComplete);

    // The origin loop could go away while we are trying to post to it, so we
    // need to call its PostTask method inside a lock.  See ~HostResolver.
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

  // Callback for when DoLookup() completes (runs on origin thread).
  void OnLookupComplete() {
    // Should be running on origin loop.
    // TODO(eroman): this is being hit by URLRequestTest.CancelTest*,
    // because MessageLoop::current() == NULL.
    //DCHECK_EQ(origin_loop_, MessageLoop::current());
    DCHECK(error_ || results_);

    if (was_cancelled())
      return;

    DCHECK(!requests_.empty());

     // Adopt the address list using the port number of the first request.
    AddressList addrlist;
    if (error_ == OK) {
      addrlist.Adopt(results_);
      addrlist.SetPort(requests_[0]->port());
      results_ = NULL;
    }

    resolver_->OnJobComplete(this, error_, addrlist);
  }

  // Set on the origin thread, read on the worker thread.
  std::string host_;

  // Only used on the origin thread (where Resolve was called).
  HostResolver* resolver_;
  RequestsList requests_;  // The requests waiting on this job.

  // Used to post ourselves onto the origin thread.
  Lock origin_loop_lock_;
  MessageLoop* origin_loop_;

  // Hold an owning reference to the host mapper that we are going to use.
  // This may not be the current host mapper by the time we call
  // ResolveAddrInfo, but that's OK... we'll use it anyways, and the owning
  // reference ensures that it remains valid until we are done.
  scoped_refptr<HostMapper> host_mapper_;

  // Assigned on the worker thread, read on the origin thread.
  int error_;
  struct addrinfo* results_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

//-----------------------------------------------------------------------------

HostResolver::HostResolver(int max_cache_entries, int cache_duration_ms)
    : cache_(max_cache_entries, cache_duration_ms), next_request_id_(0),
      shutdown_(false) {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif
}

HostResolver::~HostResolver() {
  // Cancel the outstanding jobs. Those jobs may contain several attached
  // requests, which will also be cancelled.
  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->Cancel();

  // In case we are being deleted during the processing of a callback.
  if (cur_completing_job_)
    cur_completing_job_->Cancel();
}

// TODO(eroman): Don't create cache entries for hostnames which are simply IP
// address literals.
int HostResolver::Resolve(const RequestInfo& info,
                          AddressList* addresses,
                          CompletionCallback* callback,
                          Request** out_req) {
  if (shutdown_)
    return ERR_UNEXPECTED;

  // Choose a unique ID number for observers to see.
  int request_id = next_request_id_++;

  // Notify registered observers.
  NotifyObserversStartRequest(request_id, info);

  // If we have an unexpired cache entry, use it.
  if (info.allow_cached_response()) {
    const HostCache::Entry* cache_entry = cache_.Lookup(
        info.hostname(), base::TimeTicks::Now());
    if (cache_entry) {
      addresses->SetFrom(cache_entry->addrlist, info.port());
      int error = OK;

      // Notify registered observers.
      NotifyObserversFinishRequest(request_id, info, error);

      return error;
    }
  }

  // If no callback was specified, do a synchronous resolution.
  if (!callback) {
    struct addrinfo* results;
    int error = ResolveAddrInfo(host_mapper, info.hostname(), &results);

    // Adopt the address list.
    AddressList addrlist;
    if (error == OK) {
      addrlist.Adopt(results);
      addrlist.SetPort(info.port());
      *addresses = addrlist;
    }

    // Write to cache.
    cache_.Set(info.hostname(), error, addrlist, base::TimeTicks::Now());

    // Notify registered observers.
    NotifyObserversFinishRequest(request_id, info, error);

    return error;
  }

  // Create a handle for this request, and pass it back to the user if they
  // asked for it (out_req != NULL).
  Request* req = new Request(request_id, info, callback, addresses);
  if (out_req)
    *out_req = req;

  // Next we need to attach our request to a "job". This job is responsible for
  // calling "getaddrinfo(hostname)" on a worker thread.
  scoped_refptr<Job> job;

  // If there is already an outstanding job to resolve |info.hostname()|, use
  // it. This prevents starting concurrent resolves for the same hostname.
  job = FindOutstandingJob(info.hostname());
  if (job) {
    job->AddRequest(req);
  } else {
    // Create a new job for this request.
    job = new Job(this, info.hostname());
    job->AddRequest(req);
    AddOutstandingJob(job);
    // TODO(eroman): Bound the total number of concurrent jobs.
    // http://crbug.com/9598
    job->Start();
  }

  // Completion happens during OnJobComplete(Job*).
  return ERR_IO_PENDING;
}

// See OnJobComplete(Job*) for why it is important not to clean out
// cancelled requests from Job::requests_.
void HostResolver::CancelRequest(Request* req) {
  DCHECK(req);
  DCHECK(req->job());
  // NULL out the fields of req, to mark it as cancelled.
  req->MarkAsCancelled();
  NotifyObserversCancelRequest(req->id(), req->info());
}

void HostResolver::AddObserver(Observer* observer) {
  observers_.push_back(observer);
}

void HostResolver::RemoveObserver(Observer* observer) {
  ObserversList::iterator it =
      std::find(observers_.begin(), observers_.end(), observer);

  // Observer must exist.
  DCHECK(it != observers_.end());

  observers_.erase(it);
}

void HostResolver::Shutdown() {
  shutdown_ = true;

  // Cancel the outstanding jobs.
  for (JobMap::iterator it = jobs_.begin(); it != jobs_.end(); ++it)
    it->second->Cancel();
  jobs_.clear();
}

void HostResolver::AddOutstandingJob(Job* job) {
  scoped_refptr<Job>& found_job = jobs_[job->host()];
  DCHECK(!found_job);
  found_job = job;
}

HostResolver::Job* HostResolver::FindOutstandingJob(
    const std::string& hostname) {
  JobMap::iterator it = jobs_.find(hostname);
  if (it != jobs_.end())
    return it->second;
  return NULL;
}

void HostResolver::RemoveOutstandingJob(Job* job) {
  JobMap::iterator it = jobs_.find(job->host());
  DCHECK(it != jobs_.end());
  DCHECK_EQ(it->second.get(), job);
  jobs_.erase(it);
}

void HostResolver::OnJobComplete(Job* job,
                                 int error,
                                 const AddressList& addrlist) {
  RemoveOutstandingJob(job);

  // Write result to the cache.
  cache_.Set(job->host(), error, addrlist, base::TimeTicks::Now());

  // Make a note that we are executing within OnJobComplete() in case the
  // HostResolver is deleted by a callback invocation.
  DCHECK(!cur_completing_job_);
  cur_completing_job_ = job;

  // Complete all of the requests that were attached to the job.
  for (RequestsList::const_iterator it = job->requests().begin();
       it != job->requests().end(); ++it) {
    Request* req = *it;
    if (!req->was_cancelled()) {
      DCHECK_EQ(job, req->job());

      // Notify registered observers.
      NotifyObserversFinishRequest(req->id(), req->info(), error);

      req->OnComplete(error, addrlist);

      // Check if the job was cancelled as a result of running the callback.
      // (Meaning that |this| was deleted).
      if (job->was_cancelled())
        return;
    }
  }

  cur_completing_job_ = NULL;
}

void HostResolver::NotifyObserversStartRequest(int request_id,
                                               const RequestInfo& info) {
  for (ObserversList::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    (*it)->OnStartResolution(request_id, info);
  }
}

void HostResolver::NotifyObserversFinishRequest(int request_id,
                                                const RequestInfo& info,
                                                int error) {
  bool was_resolved = error == OK;
  for (ObserversList::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    (*it)->OnFinishResolutionWithStatus(request_id, was_resolved, info);
  }
}

void HostResolver::NotifyObserversCancelRequest(int request_id,
                                                const RequestInfo& info) {
  for (ObserversList::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    (*it)->OnCancelResolution(request_id, info);
  }
}

//-----------------------------------------------------------------------------

SingleRequestHostResolver::SingleRequestHostResolver(HostResolver* resolver)
    : resolver_(resolver),
      cur_request_(NULL),
      cur_request_callback_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &SingleRequestHostResolver::OnResolveCompletion)) {
  DCHECK(resolver_ != NULL);
}

SingleRequestHostResolver::~SingleRequestHostResolver() {
  if (cur_request_) {
    resolver_->CancelRequest(cur_request_);
  }
}

int SingleRequestHostResolver::Resolve(const HostResolver::RequestInfo& info,
                                       AddressList* addresses,
                                       CompletionCallback* callback) {
  DCHECK(!cur_request_ && !cur_request_callback_) << "resolver already in use";

  HostResolver::Request* request = NULL;

  // We need to be notified of completion before |callback| is called, so that
  // we can clear out |cur_request_*|.
  CompletionCallback* transient_callback = callback ? &callback_ : NULL;

  int rv = resolver_->Resolve(info, addresses, transient_callback, &request);

  if (rv == ERR_IO_PENDING) {
    // Cleared in OnResolveCompletion().
    cur_request_ = request;
    cur_request_callback_ = callback;
  }

  return rv;
}

void SingleRequestHostResolver::OnResolveCompletion(int result) {
  DCHECK(cur_request_ && cur_request_callback_);

  CompletionCallback* callback = cur_request_callback_;

  // Clear the outstanding request information.
  cur_request_ = NULL;
  cur_request_callback_ = NULL;

  // Call the user's original callback.
  callback->Run(result);
}

}  // namespace net
