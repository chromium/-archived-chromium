/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contain the class declarations related to callbacks in o3d.

#ifndef O3D_CORE_CROSS_CALLBACK_H_
#define O3D_CORE_CROSS_CALLBACK_H_

#include "core/cross/smart_ptr.h"

namespace o3d {

class Closure {
 public:
  virtual ~Closure() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual void Run() = 0;
 protected:
  Closure() {}
};

template <class R>
class ResultCallback {
 public:
  virtual ~ResultCallback() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual R Run() = 0;
 protected:
  ResultCallback() {}
};


template <class A1>
class Callback1 {
 public:
  virtual ~Callback1() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual void Run(A1) = 0;
 protected:
  Callback1() {}
};

template <class R, class A1>
class ResultCallback1 {
 public:
  virtual ~ResultCallback1() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual R Run(A1) = 0;
 protected:
  ResultCallback1() {}
};

template <class A1, class A2>
class Callback2 {
 public:
  virtual ~Callback2() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual void Run(A1, A2) = 0;
 protected:
  Callback2() {}
};

template <class R, class A1, class A2>
class ResultCallback2 {
 public:
  virtual ~ResultCallback2() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual R Run(A1, A2) = 0;
 protected:
  ResultCallback2() {}
};

template <class A1, class A2, class A3>
class Callback3 {
 public:
  virtual ~Callback3() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual void Run(A1, A2, A3) = 0;
 protected:
  Callback3() {}
};

template <class R, class A1, class A2, class A3>
class ResultCallback3 {
 public:
  virtual ~ResultCallback3() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual R Run(A1, A2, A3) = 0;
 protected:
  ResultCallback3() {}
};

template <class A1, class A2, class A3, class A4>
class Callback4 {
 public:
  virtual ~Callback4() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual void Run(A1, A2, A3, A4) = 0;
 protected:
  Callback4() {}
};

template <class R, class A1, class A2, class A3, class A4>
class ResultCallback4 {
 public:
  virtual ~ResultCallback4() { }
  virtual bool IsRepeatable() const { return false; }
  virtual void CheckIsRepeatable() const { }
  virtual R Run(A1, A2, A3, A4) = 0;
 protected:
  ResultCallback4() {}
};


// Manages a callback so it can not be called recursively. The manager takes
// ownership of the callback. Calling Set a second time, calling Clear or
// destroying the manager deletes any callback that was previously set.
template <typename T>
class NonRecursiveCallback1Manager {
 public:
  typedef T ArgumentType;
  typedef Callback1<ArgumentType> CallbackType;
  NonRecursiveCallback1Manager()
      : callback_(NULL),
        called_(false) {
  }
  ~NonRecursiveCallback1Manager() {
    Clear();
  }

  // Check if the callback is currently set to call something
  // Returns:
  //   true if the closuer is set.
  bool IsSet() const {
    return callback_ != NULL;
  }

  // Sets the callback. Note that the manager owns any callback set and will
  // delete that callback if Set or Clear is called or if the manager is
  // destroyed.
  // Paramaters:
  //   callback: The callback to be called when Run is called.
  void Set(CallbackType* callback) {
    Clear();
    callback_ = callback;
  }

  // Clear the callback. Note that the manager owns any callback set and will
  // delete that callback if Set or Clear is called or if the manager is
  // destroyed.
  void Clear() {
    if (callback_) {
      delete callback_;
      callback_ = NULL;
    }
  }

  // Runs the callback if one is currently set and if it is not already inside a
  // previous call.
  // Parameter:
  //   argument: the argument for the callback.
  void Run(ArgumentType argument) const {
    if (callback_ && !called_) {
      called_ = true;
      callback_->Run(argument);
      called_ = false;
    }
  }

  // Exchanges the callback with a new callback, returning the old
  // callback.
  // Parameters:
  //   callback: The new callback.
  CallbackType* Exchange(CallbackType* callback) {
    CallbackType* old = callback_;
    callback_ = callback;
    return old;
  }

  // True if we're currently running the callback.
  bool called() const { return called_; }

 private:
  CallbackType* callback_;
  mutable bool called_;
  DISALLOW_COPY_AND_ASSIGN(NonRecursiveCallback1Manager);
};

// Manages a closure so it can not be called recursively. The manager takes
// ownership of the closure. Calling Set a second time, calling Clear or
// destroying the manager deletes any closure that was previously set.
class NonRecursiveClosureManager {
 public:
  typedef Closure ClosureType;

  NonRecursiveClosureManager()
      : closure_(NULL),
        called_(false) {
  }

  ~NonRecursiveClosureManager() {
    Clear();
  }

  // Check if the closure is currently set to call something
  // Returns:
  //   true if the closuer is set.
  bool IsSet() const {
    return closure_ != NULL;
  }

  // Sets the closure. Note that the manager owns any closure set and will
  // delete that closure if Set or Clear is called or if the manager is
  // destroyed.
  // Paramaters:
  //   closure: The closure to be called when Run is called.
  void Set(ClosureType* closure) {
    Clear();
    closure_ = closure;
  }

  // Clear the closure. Note that the manager owns any closure set and will
  // delete that closure if Set or Clear is called or if the manager is
  // destroyed.
  void Clear() {
    if (closure_) {
      delete closure_;
      closure_ = NULL;
    }
  }

  // Runs the closure if one is currently set and if it is not already inside a
  // previous call.
  void Run() const {
    if (closure_ && !called_) {
      called_ = true;
      closure_->Run();
      called_ = false;
    }
  }

 private:
  ClosureType* closure_;
  mutable bool called_;
  DISALLOW_COPY_AND_ASSIGN(NonRecursiveClosureManager);
};

// Manages a closure so it can not be called recursively. The manager takes
// ownership of the closure. Calling Set a second time, calling Clear or
// destroying the manager deletes any closure that was previously set.
class RefCountedNonRecursiveClosureManager : public RefCounted {
 public:
  typedef SmartPointer<RefCountedNonRecursiveClosureManager> Ref;
  typedef Closure ClosureType;

  RefCountedNonRecursiveClosureManager()
      : closure_(NULL),
        called_(false) {
  }

  ~RefCountedNonRecursiveClosureManager() {
    Clear();
  }

  // Check if the closure is currently set to call something
  // Returns:
  //   true if the closuer is set.
  bool IsSet() const {
    return closure_ != NULL;
  }

  // Sets the closure. Note that the manager owns any closure set and will
  // delete that closure if Set or Clear is called or if the manager is
  // destroyed.
  // Paramaters:
  //   closure: The closure to be called when Run is called.
  void Set(ClosureType* closure) {
    Clear();
    closure_ = closure;
  }

  // Clear the closure. Note that the manager owns any closure set and will
  // delete that closure if Set or Clear is called or if the manager is
  // destroyed.
  void Clear() {
    if (closure_) {
      delete closure_;
      closure_ = NULL;
    }
  }

  // Runs the closure if one is currently set and if it is not already inside a
  // previous call.
  void Run() const {
    if (closure_ && !called_) {
      called_ = true;
      closure_->Run();
      called_ = false;
    }
  }

 private:
  ClosureType* closure_;
  mutable bool called_;
  DISALLOW_COPY_AND_ASSIGN(RefCountedNonRecursiveClosureManager);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CALLBACK_H_
