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

#ifndef BASE_SINGLETON_H__
#define BASE_SINGLETON_H__

#include <stdlib.h>

#include <utility>

#include "base/at_exit.h"
#include "base/lock.h"
#include "base/singleton_internal.h"

#ifdef WIN32
#include "base/fix_wp64.h"
#else  // WIN32
#include <pthread.h>
#endif  // WIN32

// Default traits for Singleton<Type>. Calls operator new and operator delete on
// the object. Registers automatic deletion at process exit.
// Overload if you need arguments or another memory allocation function.
template<typename Type>
struct DefaultSingletonTraits {
  // Allocates the object.
  static Type* New() {
    // The parenthesis is very important here; it forces POD type
    // initialization.
    return new Type();
  }

  // Destroys the object.
  static void Delete(Type* x) {
    delete x;
  }

  // Set to true to automatically register deletion of the object on process
  // exit. See below for the required call that makes this happen.
  static const bool kRegisterAtExit = true;

  // Note: Only apply on Windows. Has *no effect* on other platform.
  // When set to true, it signals that Trait::New() *must* not be called
  // multiple times at construction. Anything that must be done to not enter
  // this situation should be done at all cost. This simply involves creating a
  // temporary lock.
  static const bool kMustCallNewExactlyOnce = false;
};


// The Singleton<Type, Traits, DifferentiatingType> class manages a single
// instance of Type which will be created on first use and will be destroyed at
// normal process exit). The Trait::Delete function will not be called on
// abnormal process exit.
//
// DifferentiatingType is used as a key to differentiate two different
// singletons having the same memory allocation functions but serving a
// different purpose. This is mainly used for Locks serving different purposes.
//
// Example usages: (none are preferred, they all result in the same code)
//   1. FooClass* ptr = Singleton<FooClass>::get();
//      ptr->Bar();
//   2. Singleton<FooClass>()->Bar();
//   3. Singleton<FooClass>::get()->Bar();
//
// Singleton<> has no non-static members and doesn't need to actually be
// instantiated. It does no harm to instantiate it and use it as a class member
// or at global level since it is acting as a POD type.
//
// This class is itself thread-safe. The underlying Type must of course be
// thread-safe if you want to use it concurrently. Two parameters may be tuned
// depending on the user's requirements.
//
// Glossary:
//   MCNEO = kMustCallNewExactlyOnce
//   RAE = kRegisterAtExit
//
// On every platform, if Traits::RAE is true, the singleton will be destroyed at
// process exit. More precisely it uses base::AtExitManager which requires an
// object of this type to be instanciated. AtExitManager mimics the semantics
// of atexit() such as LIFO order but under Windows is safer to call. For more
// information see at_exit.h.
//
// If Traits::RAE is false, the singleton will not be freed at process exit,
// thus the singleton will be leaked if it is ever accessed. Traits::RAE
// shouldn't be false unless absolutely necessary. Remember that the heap where
// the object is allocated may be destroyed by the CRT anyway.
//
// On Windows, now the fun begins. Traits::New() may be called more than once
// concurrently, but no user will gain access to the object until the winning
// Traits::New() call is completed.
//
// On Windows, if Traits::MCNEO and Traits::RAE are both false,
// Traits::Delete() can still be called. The reason is that a race condition can
// occur during the object creation which will cause Traits::Delete() to be
// called even if Traits::RAE is false, so Traits::Delete() should still be
// implemented or objects may be leaked when there is a race condition in
// creating the singleton. Even though this case is very rare, it may happen in
// practice. To work around this situation, before creating a multithreaded
// environment, be sure to call Singleton<>::get() to force the creation of the
// instance.
//
// On Windows, If Traits::MCNEO is true, a temporary lock per singleton will be
// created to ensure that Trait::New() is only called once.
//
// If you want to ensure that your class can only exist as a singleton, make
// its constructors private, and make DefaultSingletonTraits<> a friend:
//
//   #include "base/singleton.h"
//   class FooClass {
//    public:
//     void Bar() { ... }
//    private:
//     FooClass() { ... }
//     friend DefaultSingletonTraits<FooClass>;
//
//     DISALLOW_EVIL_CONSTRUCTORS(FooClass);
//   };
//
// Caveats:
// (a) Every call to get(), operator->() and operator*() incurs some overhead
//     (16ns on my P4/2.8GHz) to check whether the object has already been
//     initialized.  You may wish to cache the result of get(); it will not
//     change.
//
// (b) Your factory function must never throw an exception. This class is not
//     exception-safe.
//
// (c) On Windows at least, if Traits::kMustCallNewExactlyOnce is false,
//     Traits::New() may be called two times in two different threads at the
//     same time so it must not have side effects. Set
//     Traits::kMustCallNewExactlyOnce to true to alleviate this issue, at
//     the cost of a slight increase of memory use and creation time.
//
template <typename Type,
          typename Traits = DefaultSingletonTraits<Type>,
          typename DifferentiatingType = Type>
class Singleton
    : public SingletonStorage<
          Type,
          std::pair<Traits, DifferentiatingType>,
          UseVolatileSingleton<Traits::kMustCallNewExactlyOnce>::value> {
 public:
  // This class is safe to be constructed and copy-constructed since it has no
  // member.

  // Return a pointer to the one true instance of the class.
  static Type* get() {
    Type* value = instance_;
    // Acute readers may think: why not just discard "value" and use
    // "instance_" directly? Astute readers will remark that instance_ can be a
    // volatile pointer on Windows and hence the compiler would be forced to
    // generate two memory reads instead of just one. Since this is the hotspot,
    // this is inefficient.
    if (value)
      return value;

#ifdef WIN32
    // Statically determine which function to call.
    LockedConstruct<Traits::kMustCallNewExactlyOnce>();
#else  // WIN32
    // Posix platforms already have the functionality embedded.
    pthread_once(&control_, SafeConstruct);
#endif  // WIN32
    return instance_;
  }

  // Shortcuts.
  Type& operator*() {
    return *get();
  }

  Type* operator->() {
    return get();
  }

 private:
#ifdef WIN32
  // Use bool template differentiation to make sure to not build the other part
  // of the code. We don't want to instantiate Singleton<Lock, ...> uselessly.
  template<bool kUseLock>
  static void LockedConstruct() {
    // Define a differentiating type for the Lock.
    typedef std::pair<Type, std::pair<Traits, DifferentiatingType> >
        LockDifferentiatingType;

    // Object-type lock. Note that the lock singleton is different per singleton
    // type.
    AutoLock lock(*Singleton<Lock,
                             DefaultSingletonTraits<Lock>,
                             LockDifferentiatingType>());
    // Now that we have the lock, look if the instance is created, if not yet,
    // create it.
    if (!instance_)
      SafeConstruct();
  }

  template<>
  static void LockedConstruct<false>() {
    // Implemented using atomic compare-and-swap. The new object is
    // constructed and used as the new value in the operation; if the
    // compare fails, the new object will be deleted. Future implementations
    // for Windows might use InitOnceExecuteOnce (Vista-only), similar in
    // spirit to pthread_once.

    // On Windows, multiple concurrent Traits::New() calls are tolerated.
    Type* value = Traits::New();
    if (InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(&instance_), value, NULL)) {
      // Race condition, discard the temporary value.
      Traits::Delete(value);
    } else {
      if (Traits::kRegisterAtExit)
        base::AtExitManager::RegisterCallback(&OnExit);
    }
  }
#endif  // WIN32

  // SafeConstruct is guaranteed to be executed only once.
  static void SafeConstruct() {
    instance_ = Traits::New();

    if (Traits::kRegisterAtExit)
      base::AtExitManager::RegisterCallback(OnExit);
  }

  // Adapter function for use with AtExit().
  static void OnExit() {
    if (!instance_)
      return;
    Traits::Delete(instance_);
    instance_ = NULL;
  }

#ifndef WIN32
  static pthread_once_t control_;
#endif  // !WIN32
};

#ifndef WIN32

template <typename Type, typename Traits, typename DifferentiatingType>
pthread_once_t Singleton<Type, Traits, DifferentiatingType>::control_ =
    PTHREAD_ONCE_INIT;

#endif  // !WIN32

#endif  // BASE_SINGLETON_H__
