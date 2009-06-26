// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_RESOLVER_H_
#define NET_BASE_HOST_RESOLVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/host_cache.h"

class MessageLoop;

namespace net {

class AddressList;
class HostMapper;

// This class represents the task of resolving hostnames (or IP address
// literal) to an AddressList object.
//
// HostResolver handles multiple requests at a time, so when cancelling a
// request the Request* handle that was returned by Resolve() needs to be
// given.  A simpler alternative for consumers that only have 1 outstanding
// request at a time is to create a SingleRequestHostResolver wrapper around
// HostResolver (which will automatically cancel the single request when it
// goes out of scope).
//
// For each hostname that is requested, HostResolver creates a
// HostResolver::Job. This job gets dispatched to a thread in the global
// WorkerPool, where it runs "getaddrinfo(hostname)". If requests for that same
// host are made while the job is already outstanding, then they are attached
// to the existing job rather than creating a new one. This avoids doing
// parallel resolves for the same host.
//
// The way these classes fit together is illustrated by:
//
//
//            +------------- HostResolver ---------------+
//            |                    |                     |
//           Job                  Job                   Job
//       (for host1)          (for host2)           (for hostX)
//       /    |   |            /   |   |             /   |   |
//   Request ... Request  Request ... Request   Request ... Request
//  (port1)     (port2)  (port3)      (port4)  (port5)      (portX)
//
//
// When a HostResolver::Job finishes its work in the threadpool, the callbacks
// of each waiting request are run on the origin thread.
//
// Thread safety: This class is not threadsafe, and must only be called
// from one thread!
//
class HostResolver {
 public:
  // The parameters for doing a Resolve(). |hostname| and |port| are required,
  // the rest are optional (and have reasonable defaults).
  class RequestInfo {
   public:
    RequestInfo(const std::string& hostname, int port)
        : hostname_(hostname),
          port_(port),
          allow_cached_response_(true),
          is_speculative_(false) {}

    const int port() const { return port_; }
    const std::string& hostname() const { return hostname_; }

    bool allow_cached_response() const { return allow_cached_response_; }
    void set_allow_cached_response(bool b) { allow_cached_response_ = b; }

    bool is_speculative() const { return is_speculative_; }
    void set_is_speculative(bool b) { is_speculative_ = b; }

    const GURL& referrer() const { return referrer_; }
    void set_referrer(const GURL& referrer) { referrer_ = referrer; }

   private:
    // The hostname to resolve.
    std::string hostname_;

    // The port number to set in the result's sockaddrs.
    int port_;

    // Whether it is ok to return a result from the host cache.
    bool allow_cached_response_;

    // Whether this request was started by the DNS prefetcher.
    bool is_speculative_;

    // Optional data for consumption by observers. This is the URL of the
    // page that lead us to the navigation, for DNS prefetcher's benefit.
    GURL referrer_;
  };

  // Interface for observing the requests that flow through a HostResolver.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called at the start of HostResolver::Resolve(). |id| is a unique number
    // given to the request, so it can be matched up with a corresponding call
    // to OnFinishResolutionWithStatus() or OnCancelResolution().
    virtual void OnStartResolution(int id, const RequestInfo& info) = 0;

    // Called on completion of request |id|. Note that if the request was
    // cancelled, OnCancelResolution() will be called instead.
    virtual void OnFinishResolutionWithStatus(int id, bool was_resolved,
                                              const RequestInfo& info) = 0;

    // Called when request |id| has been cancelled. A request is "cancelled"
    // if either the HostResolver is destroyed while a resolution is in
    // progress, or HostResolver::CancelRequest() is called.
    virtual void OnCancelResolution(int id, const RequestInfo& info) = 0;
  };

  // Creates a HostResolver that caches up to |max_cache_entries| for
  // |cache_duration_ms| milliseconds.
  //
  // TODO(eroman): Get rid of the default parameters as it violate google
  // style. This is temporary to help with refactoring.
  HostResolver(int max_cache_entries = 100, int cache_duration_ms = 60000);

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  ~HostResolver();

  // Opaque type used to cancel a request.
  class Request;

  // Resolves the given hostname (or IP address literal), filling out the
  // |addresses| object upon success.  The |info.port| parameter will be set as
  // the sin(6)_port field of the sockaddr_in{6} struct.  Returns OK if
  // successful or an error code upon failure.
  //
  // When callback is null, the operation completes synchronously.
  //
  // When callback is non-null, the operation will be performed asynchronously.
  // ERR_IO_PENDING is returned if it has been scheduled successfully. Real
  // result code will be passed to the completion callback. If |req| is
  // non-NULL, then |*req| will be filled with a handle to the async request.
  // This handle is not valid after the request has completed.
  int Resolve(const RequestInfo& info, AddressList* addresses,
              CompletionCallback* callback, Request** req);

  // Cancels the specified request. |req| is the handle returned by Resolve().
  // After a request is cancelled, its completion callback will not be called.
  void CancelRequest(Request* req);

  // Adds an observer to this resolver. The observer will be notified of the
  // start and completion of all requests (excluding cancellation). |observer|
  // must remain valid for the duration of this HostResolver's lifetime.
  void AddObserver(Observer* observer);

  // Unregisters an observer previously added by AddObserver().
  void RemoveObserver(Observer* observer);

 private:
  class Job;
  typedef std::vector<Request*> RequestsList;
  typedef base::hash_map<std::string, scoped_refptr<Job> > JobMap;
  typedef std::vector<Observer*> ObserversList;

  // Adds a job to outstanding jobs list.
  void AddOutstandingJob(Job* job);

  // Returns the outstanding job for |hostname|, or NULL if there is none.
  Job* FindOutstandingJob(const std::string& hostname);

  // Removes |job| from the outstanding jobs list.
  void RemoveOutstandingJob(Job* job);

  // Callback for when |job| has completed with |error| and |addrlist|.
  void OnJobComplete(Job* job, int error, const AddressList& addrlist);

  // Notify all observers of the start of a resolve request.
  void NotifyObserversStartRequest(int request_id,
                                   const RequestInfo& info);

  // Notify all observers of the completion of a resolve request.
  void NotifyObserversFinishRequest(int request_id,
                                    const RequestInfo& info,
                                    int error);

  // Notify all observers of the cancellation of a resolve request.
  void NotifyObserversCancelRequest(int request_id,
                                    const RequestInfo& info);

  // Cache of host resolution results.
  HostCache cache_;

  // Map from hostname to outstanding job.
  JobMap jobs_;

  // The job that OnJobComplete() is currently processing (needed in case
  // HostResolver gets deleted from within the callback).
  scoped_refptr<Job> cur_completing_job_;

  // The observers to notify when a request starts/ends.
  ObserversList observers_;

  // Monotonically increasing ID number to assign to the next request.
  // Observers are the only consumers of this ID number.
  int next_request_id_;

  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

// This class represents the task of resolving a hostname (or IP address
// literal) to an AddressList object.  It wraps HostResolver to resolve only a
// single hostname at a time and cancels this request when going out of scope.
class SingleRequestHostResolver {
 public:
  explicit SingleRequestHostResolver(HostResolver* resolver);

  // If a completion callback is pending when the resolver is destroyed, the
  // host resolution is cancelled, and the completion callback will not be
  // called.
  ~SingleRequestHostResolver();

  // Resolves the given hostname (or IP address literal), filling out the
  // |addresses| object upon success. See HostResolver::Resolve() for details.
  int Resolve(const HostResolver::RequestInfo& info,
              AddressList* addresses, CompletionCallback* callback);

 private:
  // Callback for when the request to |resolver_| completes, so we dispatch
  // to the user's callback.
  void OnResolveCompletion(int result);

  // The actual host resolver that will handle the request.
  HostResolver* resolver_;

  // The current request (if any).
  HostResolver::Request* cur_request_;
  CompletionCallback* cur_request_callback_;

  // Completion callback for when request to |resolver_| completes.
  net::CompletionCallbackImpl<SingleRequestHostResolver> callback_;

  DISALLOW_COPY_AND_ASSIGN(SingleRequestHostResolver);
};

// A helper class used in unit tests to alter hostname mappings.  See
// SetHostMapper for details.
class HostMapper : public base::RefCountedThreadSafe<HostMapper> {
 public:
  virtual ~HostMapper() {}

  // Returns possibly altered hostname, or empty string to simulate
  // a failed lookup.
  virtual std::string Map(const std::string& host) = 0;

 protected:
  // Ask previous host mapper (if set) for mapping of given host.
  std::string MapUsingPrevious(const std::string& host);

 private:
  friend class ScopedHostMapper;

  // Set mapper to ask when this mapper doesn't want to modify the result.
  void set_previous_mapper(HostMapper* mapper) {
    previous_mapper_ = mapper;
  }

  scoped_refptr<HostMapper> previous_mapper_;
};

#ifdef UNIT_TEST
// This function is designed to allow unit tests to override the behavior of
// HostResolver.  For example, a HostMapper instance can force all hostnames
// to map to a fixed IP address such as 127.0.0.1.
//
// The previously set HostMapper (or NULL if there was none) is returned.
//
// NOTE: This function is not thread-safe, so take care to only call this
// function while there are no outstanding HostResolver instances.
//
// NOTE: In most cases, you should use ScopedHostMapper instead, which is
// defined in host_resolver_unittest.h
//
HostMapper* SetHostMapper(HostMapper* host_mapper);
#endif

}  // namespace net

#endif  // NET_BASE_HOST_RESOLVER_H_
