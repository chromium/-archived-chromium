// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CancelableRequestProviders and Consumers work together to make requests that
// execute on a background thread in the provider and return data to the
// consumer. These class collaborate to keep a list of open requests and to
// make sure that requests to not outlive either of the objects involved in the
// transaction.
//
// If you do not need to return data to the consumer, do not use this system,
// just use the regular Task/RunnableMethod stuff.
//
// The CancelableRequest object is used internally to each provider to track
// request data and callback information.
//
// Example consumer calling |StartRequest| on a frontend service:
//
//   class MyClass {
//     void MakeRequest() {
//       frontend_service->StartRequest(some_input1, some_input2, this,
//           NewCallback(this, &MyClass:RequestComplete));
//     }
//
//     void RequestComplete(int status) {
//       ...
//     }
//
//    private:
//     CallbackConsumer callback_consumer_;
//   };
//
//
// Example frontend provider. It receives requests and forwards them to the
// backend on another thread:
//
//   class Frontend : public CancelableRequestProvider {
//     typedef Callback1<int>::Type RequestCallbackType;
//
//     Handle StartRequest(int some_input1, int some_input2,
//                         CallbackConsumer* consumer,
//                         RequestCallbackType* callback) {
//       scoped_refptr<CancelableRequest<RequestCallbackType> > request(
//           new CancelableRequest<RequestCallbackType>(callback));
//       AddRequest(request, consumer);
//
//       // Send the parameters and the request to the backend thread.
//       backend_thread_->PostTask(FROM_HERE,
//           NewRunnableMethod(backend_, &Backend::DoRequest, request,
//                             some_input1, some_input2));
//
//       // The handle will have been set by AddRequest.
//       return request->handle();
//     }
//   };
//
//
// Example backend provider that does work and dispatches the callback back
// to the original thread. Note that we need to pass it as a scoped_refptr so
// that the object will be kept alive if the request is canceled (releasing
// the provider's reference to it).
//
//   class Backend {
//     void DoRequest(
//         scoped_refptr< CancelableRequest<Frontend::RequestCallbackType> >
//             request,
//         int some_input1, int some_input2) {
//       if (request->canceled())
//         return;
//
//       ... do your processing ...
//
//       // Depending on your typedefs, one of these two forms will be more
//       // convenient:
//       request->ForwardResult(Tuple1<int>(return_value));
//
//       // -- or --  (inferior in this case)
//       request->ForwardResult(Frontend::RequestCallbackType::TupleType(
//           return_value));
//     }
//   };

#ifndef CHROME_BROWSER_CANCELABLE_REQUEST_H__
#define CHROME_BROWSER_CANCELABLE_REQUEST_H__

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/task.h"

class CancelableRequestBase;
class CancelableRequestConsumerBase;

// CancelableRequestProvider --------------------------------------------------
//
// This class is threadsafe. Requests may be added or canceled from any thread,
// but a task must only be canceled from the same thread it was initially run
// on.
//
// It is intended that providers inherit from this class to provide the
// necessary functionality.

class CancelableRequestProvider {
 public:
  // Identifies a specific request from this provider.
  typedef int Handle;

  CancelableRequestProvider();
  virtual ~CancelableRequestProvider();

  // Called by the enduser of the request to cancel it. This MUST be called on
  // the same thread that originally issued the request (which is also the same
  // thread that would have received the callback if it was not canceled).
  void CancelRequest(Handle handle);

 protected:
  // Adds a new request and initializes it. This is called by a derived class
  // to add a new request. The request's Init() will be called (which is why
  // the consumer is required. The handle to the new request is returned.
  Handle AddRequest(CancelableRequestBase* request,
                    CancelableRequestConsumerBase* consumer);

  // Called by the CancelableRequest when the request has executed. It will
  // be removed from the list of pending requests (as opposed to canceling,
  // which will also set some state on the request).
  void RequestCompleted(Handle handle);

 private:
  // Only call this when you already have acquired pending_request_lock_.
  void CancelRequestLocked(Handle handle);

  friend class CancelableRequestBase;

  typedef std::map<Handle, scoped_refptr<CancelableRequestBase> >
      CancelableRequestMap;

  Lock pending_request_lock_;

  // Lists all outstanding requests. Protected by the |lock_|.
  CancelableRequestMap pending_requests_;

  // The next handle value we will return. Protected by the |lock_|.
  int next_handle_;

  DISALLOW_EVIL_CONSTRUCTORS(CancelableRequestProvider);
};

// CancelableRequestConsumer --------------------------------------------------
//
// Classes wishing to make requests on a provider should have an instance of
// this class. Callers will need to pass a pointer to this consumer object
// when they make the request. It will automatically track any pending
// requests, and will automatically cancel them on destruction to prevent the
// accidental calling of freed memory.
//
// It is recommended to just have this class as a member variable since there
// is nothing to be gained by inheriting from it other than polluting your
// namespace.
//
// THIS CLASS IS NOT THREADSAFE (unlike the provider). You must make requests
// and get callbacks all from the same thread.

// Base class used to notify of new requests.
class CancelableRequestConsumerBase {
 protected:
  friend class CancelableRequestProvider;

  virtual ~CancelableRequestConsumerBase() {
  }

  // Adds a new request to the list of requests that are being tracked. This
  // is called by the provider when a new request is created.
  virtual void OnRequestAdded(CancelableRequestProvider* provider,
                              CancelableRequestProvider::Handle handle) = 0;

  // Removes the given request from the list of pending requests. Called
  // by the CancelableRequest immediately after the callback has executed for a
  // given request, and by the provider when a request is canceled.
  virtual void OnRequestRemoved(CancelableRequestProvider* provider,
                                CancelableRequestProvider::Handle handle) = 0;
};

// Template for clients to use. It allows them to associate random "client
// data" with a specific request. The default value for this type is NULL.
// The type T should be small and easily copyable (like a pointer
// or an integer).
template<class T>
class CancelableRequestConsumerTSimple : public CancelableRequestConsumerBase {
 public:
  CancelableRequestConsumerTSimple() {
  }

  // Cancel any outstanding requests so that we do not get called back after we
  // are destroyed. As these requests are removed, the providers will call us
  // back on OnRequestRemoved, which will then update the list. To iterate
  // successfully while the list is changing out from under us, we make a copy.
  virtual ~CancelableRequestConsumerTSimple() {
    CancelAllRequests();
  }

  // Associates some random data with a specified request. The request MUST be
  // outstanding, or it will assert. This is intended to be called immediately
  // after a request is issued.
  void SetClientData(CancelableRequestProvider* p,
                     CancelableRequestProvider::Handle h,
                     T client_data) {
    PendingRequest request(p, h);
    DCHECK(pending_requests_.find(request) != pending_requests_.end());
    pending_requests_[request] = client_data;
  }

  // Retrieves previously associated data for a specified request. The request
  // MUST be outstanding, or it will assert. This is intended to be called
  // during processing of a callback to retrieve extra data.
  T GetClientData(CancelableRequestProvider* p,
                  CancelableRequestProvider::Handle h) {
    PendingRequest request(p, h);
    DCHECK(pending_requests_.find(request) != pending_requests_.end());
    return pending_requests_[request];
  }

  // Returns true if there are any pending requests.
  bool HasPendingRequests() const {
    return !pending_requests_.empty();
  }

  // Returns the number of pending requests.
  size_t PendingRequestCount() const {
    return pending_requests_.size();
  }

  // Cancels all requests outstanding.
  void CancelAllRequests() {
    PendingRequestList copied_requests(pending_requests_);
    for (typename PendingRequestList::iterator i = copied_requests.begin();
         i != copied_requests.end(); ++i)
      i->first.provider->CancelRequest(i->first.handle);
    copied_requests.clear();

    // That should have cleared all the pending items.
    DCHECK(pending_requests_.empty());
  }

  // Gets the client data for all pending requests.
  void GetAllClientData(std::vector<T>* data) {
    DCHECK(data);
    for (typename PendingRequestList::iterator i = pending_requests_.begin();
         i != pending_requests_.end(); ++i)
      data->push_back(i->second);
  }

 protected:
  struct PendingRequest {
    PendingRequest(CancelableRequestProvider* p,
                   CancelableRequestProvider::Handle h)
        : provider(p), handle(h) {
    }

    // Comparison operator for stl.
    bool operator<(const PendingRequest& other) const {
      if (provider != other.provider)
        return provider < other.provider;
      return handle < other.handle;
    }

    CancelableRequestProvider* provider;
    CancelableRequestProvider::Handle handle;
  };
  typedef std::map<PendingRequest, T> PendingRequestList;

  virtual T get_initial_t() const {
    return NULL;
  }

  virtual void OnRequestAdded(CancelableRequestProvider* provider,
                              CancelableRequestProvider::Handle handle) {
    DCHECK(pending_requests_.find(PendingRequest(provider, handle)) ==
           pending_requests_.end());
    pending_requests_[PendingRequest(provider, handle)] = get_initial_t();
  }

  virtual void OnRequestRemoved(CancelableRequestProvider* provider,
                                CancelableRequestProvider::Handle handle) {
    typename PendingRequestList::iterator i =
        pending_requests_.find(PendingRequest(provider, handle));
    if (i == pending_requests_.end()) {
      NOTREACHED() << "Got a complete notification for a nonexistant request";
      return;
    }

    pending_requests_.erase(i);
  }

  // Lists all outstanding requests.
  PendingRequestList pending_requests_;
};

// See CancelableRequestConsumerTSimple. The default value for T
// is given in |initial_t|.
template<class T, T initial_t>
class CancelableRequestConsumerT : public CancelableRequestConsumerTSimple<T> {
 protected:
  virtual T get_initial_t() const {
    return initial_t;
  }
};

// Some clients may not want to store data. Rather than do some complicated
// thing with virtual functions to allow some consumers to store extra data and
// some not to, we just define a default one that stores some dummy data.
typedef CancelableRequestConsumerT<int, 0> CancelableRequestConsumer;

// CancelableRequest ----------------------------------------------------------
//
// The request object that is used by a CancelableRequestProvider to send
// results to a CancelableRequestConsumer. This request handles the returning
// of results from a thread where the request is being executed to the thread
// and callback where the results are used. IT SHOULD BE PASSED AS A
// scoped_refptr TO KEEP IT ALIVE.
//
// It does not handle input parameters to the request. The caller must either
// transfer those separately or derive from this class to add the desired
// parameters.
//
// When the processing is complete on this message, the caller MUST call
// ForwardResult() with the return arguments that will be passed to the
// callback. If the request has been canceled, Return is optional (it will not
// do anything). If you do not have to return to the caller, the cancelable
// request system should not be used! (just use regular fire-and-forget tasks).
//
// Callback parameters are passed by value. In some cases, the request will
// want to return a large amount of data (for example, an image). One good
// approach is to derive from the CancelableRequest and make the data object
// (for example, a std::vector) owned by the CancelableRequest. The pointer
// to this data would be passed for the callback parameter. Since the
// CancelableRequest outlives the callback call, the data will be valid on the
// other thread for the callback, but will still be destroyed properly.

// Non-templatized base class that provides cancellation
class CancelableRequestBase :
    public base::RefCountedThreadSafe<CancelableRequestBase> {
 public:
  friend class CancelableRequestProvider;

  // Initializes most things to empty, Init() must be called to complete
  // initialization of the object. This will be done by the provider when
  // the request is dispatched.
  //
  // This must be called on the same thread the callback will be executed on,
  // it will save that thread for later.
  //
  // This two-phase init is done so that the constructor can have no
  // parameters, which makes it much more convenient for derived classes,
  // which can be common. The derived classes need only declare the variables
  // they provide in the constructor rather than many lines of internal
  // tracking data that are passed to the base class (us).
  //
  // In addition, not all of the information (for example, the handle) is known
  // at construction time.
  CancelableRequestBase()
      : provider_(NULL),
        consumer_(NULL),
        handle_(0),
        canceled_(false) {
    callback_thread_ = MessageLoop::current();
  }
  virtual ~CancelableRequestBase() {
  }

  CancelableRequestConsumerBase* consumer() const {
    return consumer_;
  }

  CancelableRequestProvider::Handle handle() const {
    return handle_;
  }

  // The canceled flag indicates that the request should not be executed.
  // A request can never be uncanceled, so only a setter for true is provided.
  void set_canceled() {
    canceled_ = true;
  }
  bool canceled() {
    return canceled_;
  }

 protected:
  // Initializes the object with the particulars from the provider. It may only
  // be called once (it is called by the provider, which is a friend).
  void Init(CancelableRequestProvider* provider,
            CancelableRequestProvider::Handle handle,
            CancelableRequestConsumerBase* consumer) {
    DCHECK(handle_ == 0 && provider_ == NULL && consumer_ == NULL);
    provider_ = provider;
    consumer_ = consumer;
    handle_ = handle;
  }

  // Tells the provider that the request is complete, which then tells the
  // consumer.
  void NotifyCompleted() const {
    provider_->RequestCompleted(handle());
  }

  // The message loop that this request was created on. The callback will
  // happen on the same thread.
  MessageLoop* callback_thread_;

  // The provider for this request. When we execute, we will notify this that
  // request is complete to it can remove us from the requests it tracks.
  CancelableRequestProvider* provider_;

  // Notified after we execute that the request is complete.  This should only
  // be accessed if !canceled_, otherwise the pointer is invalid.
  CancelableRequestConsumerBase* consumer_;

  // The handle to this request inside the provider. This will be initialized
  // to 0 when the request is created, and the provider will set it once the
  // request has been dispatched.
  CancelableRequestProvider::Handle handle_;

  // Set if the caller cancels this request. No callbacks should be made when
  // this is set.
  bool canceled_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(CancelableRequestBase);
};

// Templatized class. This is the one you should use directly or inherit from.
// The callback can be invoked by calling the ForwardResult() method. For this,
// you must either pack the parameters into a tuple, or use DispatchToMethod
// (in tuple.h).
//
// If you inherit to add additional input parameters or to do more complex
// memory management (see the bigger comment about this above), you can put
// those on a subclass of this.
//
// We have decided to allow users to treat derived classes of this as structs,
// so you can add members without getters and setters (which just makes the
// code harder to read). Don't use underscores after these vars. For example:
//
//   typedef Callback1<int>::Type DoodieCallback;
//
//   class DoodieRequest : public CancelableRequest<DoodieCallback> {
//    public:
//     DoodieRequest(CallbackType* callback) : CancelableRequest(callback) {
//     }
//
//     int input_arg1;
//     std::wstring input_arg2;
//   };
template<typename CB>
class CancelableRequest : public CancelableRequestBase {
 public:
  typedef CB CallbackType;          // CallbackRunner<...>
  typedef typename CB::TupleType TupleType;  // Tuple of the callback args.

  // The provider MUST call Init() (on the base class) before this is valid.
  // This class will take ownership of the callback object and destroy it when
  // appropriate.
  explicit CancelableRequest(CallbackType* callback)
      : CancelableRequestBase(),
        callback_(callback) {
    DCHECK(callback) << "We should always have a callback";
  }
  virtual ~CancelableRequest() {
  }

  // Dispatches the parameters to the correct thread so the callback can be
  // executed there. The caller does not need to check for cancel before
  // calling this. It is optional in the cancelled case. In the non-cancelled
  // case, this MUST be called.
  //
  // If there are any pointers in the parameters, they must live at least as
  // long as the request so that it can be forwarded to the other thread.
  // For complex objects, this would typically be done by having a derived
  // request own the data itself.
  void ForwardResult(const TupleType& param) {
    DCHECK(callback_.get());
    if (!canceled()) {
      if (callback_thread_ == MessageLoop::current()) {
        // We can do synchronous callbacks when we're on the same thread.
        ExecuteCallback(param);
      } else {
        callback_thread_->PostTask(FROM_HERE, NewRunnableMethod(this,
            &CancelableRequest<CB>::ExecuteCallback, param));
      }
    }
  }

 private:
  // Executes the callback and notifies the provider and the consumer that this
  // request has been completed. This must be called on the callback_thread_.
  void ExecuteCallback(const TupleType& param) {
    if (!canceled_) {
      // Execute the callback.
      callback_->RunWithParams(param);

      // Notify the provider that the request is complete. The provider will
      // notify the consumer for us.
      NotifyCompleted();
    }
  }

  // This should only be executed if !canceled_, otherwise the pointers may be
  // invalid.
  scoped_ptr<CallbackType> callback_;
};

// A CancelableRequest with a single value. This is intended for use when
// the provider provides a single value. The provider fills the result into
// the value, and notifies the request with a pointer to the value. For example,
// HistoryService has many methods that callback with a vector. Use the
// following pattern for this:
// 1. Define the callback:
//      typedef Callback2<Handle, std::vector<Foo>*>::Type FooCallback;
// 2. Define the CancelableRequest1 type.
//    typedef CancelableRequest1<FooCallback, std::vector<Foo>> FooRequest;
// 3. The provider method should then fillin the contents of the vector,
//    forwarding the result like so:
//    request->ForwardResult(FooRequest::TupleType(request->handle(),
//                                                 &request->value));
//
// Tip: for passing more than one value, use a Tuple for the value.
template<typename CB, typename Type>
class CancelableRequest1 : public CancelableRequest<CB> {
 public:
  explicit CancelableRequest1(
      typename CancelableRequest<CB>::CallbackType* callback)
      : CancelableRequest<CB>(callback) {
  }

  virtual ~CancelableRequest1() {
  }

  // The value.
  Type value;
};

#endif  // CHROME_BROWSER_CANCELABLE_REQUEST_H__
