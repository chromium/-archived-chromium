// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Callback classes provides a generic interface for classes requiring
// callback from other classes.
// We support callbacks with 0, 1, 2, 3, and 4 arguments.
//   Closure                  -- provides "void Run()"
//   Callback1<T1>            -- provides "void Run(T1)"
//   Callback2<T1,T2>         -- provides "void Run(T1, T2)"
//   Callback3<T1,T2,T3>      -- provides "void Run(T1, T2, T3)"
//   Callback4<T1,T2,T3,T4>   -- provides "void Run(T1, T2, T3, T4)"
//
// In addition, ResultCallback classes provide a generic interface for
// callbacks that return a value.
//   ResultCallback<R>              -- provides "R Run()"
//   ResultCallback1<R,T1>          -- provides "R Run(T1)"
//   ResultCallback2<R,T1,T2>       -- provides "R Run(T1, T2)"
//   ResultCallback3<R,T1,T2,T3>    -- provides "R Run(T1, T2, T3)"
//   ResultCallback4<R,T1,T2,T3,T4> -- provides "R Run(T1, T2, T3, T4)"

// We provide a convenient mechanism, NewCallback, for generating one of these
// callbacks given an object pointer, a pointer to a member
// function with the appropriate signature in that object's class,
// and some optional arguments that can be bound into the callback
// object.  The mechanism also works with just a function pointer.
//
// Note: there are two types of arguments passed to the callback method:
//   * "pre-bound arguments" - supplied when the callback object is created
//   * "call-time arguments" - supplied when the callback object is invoked
//
// These two types correspond to "early binding" and "late
// binding". An argument whose value is known when the callback is
// created ("early") can be pre-bound (a.k.a. "Curried"), You can
// combine pre-bound and call-time arguments in different ways. For
// example, invoking a callback with 3 pre-bound arguments and 1
// call-time argument will have the same effect as invoking a callback
// with 2 pre-bound arguments and 2 call-time arguments, or 4
// pre-bound arguments and no call-time arguments. This last case is
// often useful; a callback with no call-time arguments is a Closure;
// these are used in many places in the Google libraries, e.g., "done"
// closures. See the examples below.
//
// WARNING:  In the current implementation (or perhaps with the current
// compiler) NewCallback() is pickier about the types of pre-bound arguments
// than you might expect.  The types must match exactly, rather than merely
// being compatible.
// For example, if you pre-bind an argument with the "const" specifier,
// make sure that the actual parameter passed to NewCallback also has the
// const specifier.   If you don't you'll get an error about
// passing a your function "as argument 1 of NewCallback(void (*)())".
// Using a method or function that has reference arguments among its pre-bound
// arguments may not always work.
//
// Examples:
//
//   void Call0(Closure* cb) { cb->Run(); }
//   void Call1(Callback1<int>* cb, int a) { cb->Run(a); }
//   void Call2(Callback2<int, float>* cb, int a, float f) { cb->Run(a, f); }
//   float Call3(ResultCallback1<float, int>* cb, int a) { return cb->Run(a); }
//
//   class Foo {
//    public:
//     void A(int a);
//     void B(int a, float f);
//     void C(const char* label, int a, float f);
//     float D(int a);
//   };
//   void F0(int a);
//   void F1(int a, float f);
//   void F2(const char *label, int a, float f);
//   float F3(int a);
//   float v;
//
//   // Run stuff immediately
//   // calling a method
//   Foo* foo = new Foo;
//
//      NewCallback(foo, &Foo::A)      ->Run(10); // 0 [pre-bound] + 1 [call-time]
//   == NewCallback(foo, &Foo::A, 10)  ->Run();   // 1 + 0
//   == foo->A(10);
//
//      NewCallback(foo, &Foo::B)            ->Run(10, 3.0f); // 0 + 2
//   == NewCallback(foo, &Foo::B, 10)        ->Run(3.0f);     // 1 + 1
//   == NewCallback(foo, &Foo::B, 10, 3.0f)  ->Run();         // 2 + 0
//   == foo->B(10, 3.0f);
//
//      NewCallback(foo, &Foo::C)                 ->Run("Y", 10, 3.0f); // 0 + 3
//   == NewCallback(foo, &Foo::C, "Y")            ->Run(10, 3.0f);      // 1 + 2
//   == NewCallback(foo, &Foo::C, "Y", 10)        ->Run(3.0f);          // 2 + 1
//   == NewCallback(foo, &Foo::C, "Y", 10, 3.0f)  ->Run();              // 3 + 0
//   == foo->C("Y", 10, 3.0f);
//
//   v = NewCallback(foo, &Foo::D)  ->Run(10);        == v = foo->D(10)
//
//   // calling a function
//   NewCallback(F0)      ->Run(10);        // == F0(10)  // 0 + 1
//   NewCallback(F0, 10)  ->Run();          // == F0(10)  // 1 + 0
//   NewCallback(F1)      ->Run(10, 3.0f);  // == F1(10, 3.0f)
//   NewCallback(F2, "X") ->Run(10, 3.0f);  // == F2("X", 10, 3.0f)
//   NewCallback(F2, "Y") ->Run(10, 3.0f);  // == F2("Y", 10, 3.0f)
//   v = NewCallback(F3) ->Run(10);         // == v = F3(10)
//
//
//   // Pass callback object to somebody else, who runs it.
//   // Calling a method:
//      Call1(NewCallback(foo, &Foo::A),      10);   // 0 + 1
//   == Call0(NewCallback(foo, &Foo::A, 10)     );   // 1 + 0
//   == foo->A(10)
//
//      Call2(NewCallback(foo, &Foo::B),      10, 3.0f);  // 0 + 2
//   == Call1(NewCallback(foo, &Foo::B, 10),      3.0f);  // 1 + 1
//   == Call0(NewCallback(foo, &Foo::B, 10, 30.f)     );  // 2 + 0
//   == foo->B(10, 3.0f)
//
//   Call2(NewCallback(foo, &Foo::C, "X"), 10, 3.0f); == foo->C("X", 10, 3.0f)
//   Call2(NewCallback(foo, &Foo::C, "Y"), 10, 3.0f); == foo->C("Y", 10, 3.0f)
//
//   // Calling a function:
//      Call1(NewCallback(F0),      10);  // 0 + 1
//   == Call0(NewCallback(F0, 10)     );  // 1 + 0
//   == F0(10);
//
//   Call2(NewCallback(F1),      10, 3.0f); // == F1(10, 3.0f)
//   Call2(NewCallback(F2, "X"), 10, 3.0f); // == F2("X", 10, 3.0f)
//   Call2(NewCallback(F2, "Y"), 10, 3.0f); // == F2("Y", 10, 3.0f)
//   v = Call3(NewCallback(F3),  10);       // == v = F3(10)
//
//  Example of a "done" closure:
//
//  SelectServer ss;
//  Closure* done = NewCallback(&ss, &SelectServer::MakeLoopExit);
//  ProcessMyControlFlow(..., done);
//  ss.Loop();
//  ...
//
//  The following WILL NOT WORK:
//          NewCallback(F2, (char *) "Y") ->Run(10, 3.0f);
//  It gets the error:
//       passing `void (*)(const char *, int, float)' as argument 1 of
//       `NewCallback(void (*)())'
//  The problem is that "char *" is not an _exact_ match for
//  "const char *", even though it's normally a legal implicit
//  conversion.
//
//
// The callback objects generated by NewCallback are self-deleting:
// i.e., they call the member function, and then delete themselves.
// If you want a callback that does not delete itself every time
// it runs, use "NewPermanentCallback" instead of "NewCallback".
//
// All the callback/closure classes also provide
//   virtual void CheckIsRepeatable() const;
// It crashes if (we know for sure that) the callback's Run method
// can not be called an arbitrary number of times (including 0).
// It crashes for all NewCallback() generated callbacks,
// does not crash for NewPermanentCallback() generated callbacks,
// and although by default it does not crash for all callback-derived classes,
// for these new types of callbacks, the callback writer is encouraged to
// redefine this method appropriately.
//
// CAVEAT: Interfaces that accept callback pointers should clearly document
// if they might call Run methods of those callbacks multiple times
// (and use "c->CheckIsRepeatable();" as an active precondition check),
// or if they call the callbacks exactly once or potentially not at all,
// as well as if they take ownership of the passed callbacks
// (i.e. might manually deallocate them without calling their Run methods).
// The clients can then provide properly allocated and behaving callbacks
// (e.g. choose between NewCallback, NewPermanentCallback, or a custom object).
// Obviously, one should also be careful to ensure that the data a callback
// points to and needs for its Run method is still live when
// the Run method might be called.
//
// MOTIVATION FOR CALLBACK OBJECTS
// -------------------------------
// It frees service providers from depending on service requestors by
// calling a generic callback other than a callback which depends on
// the service requestor (typically its member function).  As a
// result, service provider classes can be developed independently.
//
// Typical usage: Suppose class A wants class B to do something and
// notify A when it is done.  As part of the notification, it wants
// to be given a boolean that says what happened.
//
// class A {
//   public:
//     void RequestService(B* server) {
//       ...
//       server->StartService(NewCallback(this, &A::ServiceDone), other_args));
//       // the new callback deletes itself after it runs
//     }
//     void ServiceDone(bool status) {...}
// };
//
// Class B {
//   public:
//     void StartService(Callback1<bool>* cb, other_args) : cb_(cb) { ...}
//     void FinishService(bool result) { ...; cb_->Run(result); }
//   private:
//     Callback1<bool>* cb_;
// };
//
// As can be seen, B is completely independent of A. (Of course, they
// have to agree on callback data type.)
//
// The result of NewCallback() is thread-compatible. The result of
// NewPermanentCallback() is thread-safe if the call its Run() method
// represents is thread-safe and thread-compatible otherwise.
//
// Other modules associated with callbacks may be found in //util/callback
//
// USING CALLBACKS WITH TRACECONTEXT
// ---------------------------------
// Callbacks generated by NewCallback() automatically propagate trace
// context.  Callbacks generated by NewPermanentCallback() do not.  For
// manually-derived subclasses of Closure and CallbackN, you may decide
// to propagate TraceContext as follows.
//
// struct MyClosure : public Closure {
//   MyClosure()
//     : Closure(TraceContext::THREAD) { }
//
//   void Run() {
//     TraceContext *tc = TraceContext::Thread();
//     tc->Swap(&trace_context_);
//     DoMyOperation()
//     tc->Swap(&trace_context_);
//     delete this;
//   }
// };

#ifndef _CALLBACK_H_
#define _CALLBACK_H_

#include <functional>

// The actual callback classes and various NewCallback() implementations
// are automatically generated by base/generate-callback-specializations.py.
// We include that output here.
#include "base/callback-specializations.h"

// A new barrier closure executes another closure after it has been
// invoked N times, and then deletes itself.
//
// If "N" is zero, the supplied closure is executed immediately.
// Barrier closures are thread-safe.  They use an atomic operation to
// guarantee a correct count.
//
// REQUIRES N >= 0.
extern Closure* NewBarrierClosure(int N, Closure* done_closure);

// Function that does nothing; can be used to make new no-op closures:
//   NewCallback(&DoNothing)
//   NewPermanentCallback(&DoNothing)
// This is a replacement for the formerly available TheNoopClosure() primitive.
extern void DoNothing();

// AutoClosureRunner executes a closure upon deletion.  This class
// is similar to scoped_ptr: it is typically stack-allocated and can be
// used to perform some type of cleanup upon exiting a block.
//
// Note: use of AutoClosureRunner with Closures that must be executed at
// specific points is discouraged, since the point at which the Closure
// executes is not explicitly marked.  For example, consider a Closure
// that should execute after a mutex has been released.  The following
// code looks correct, but executes the Closure too early (before release):
// {
//   MutexLock l(...);
//   AutoClosureRunner r(run_after_unlock);
//   ...
// }
// AutoClosureRunner is primarily intended for cleanup operations that
// are relatively independent from other code.
//
// The Reset() method replaces the callback with a new callback. The new
// callback can be supplied as NULL to disable the AutoClosureRunner. This is
// intended as part of a strategy to execute a callback at all exit points of a
// method except where Reset() was called. This method must be used only with
// non-permanent callbacks. The Release() method disables and returns the
// callback, instead of deleting it.
class AutoClosureRunner {
 private:
  Closure* closure_;
 public:
  explicit AutoClosureRunner(Closure* c) : closure_(c) {}
  ~AutoClosureRunner() { if (closure_) closure_->Run(); }
  void Reset(Closure *c) { delete closure_; closure_ = c; }
  Closure* Release() { Closure* c = closure_; closure_ = NULL; return c; }
 private:
  DISALLOW_EVIL_CONSTRUCTORS(AutoClosureRunner);
};

// DeletePointerClosure can be used to create a closure that calls delete
// on a pointer.  Here is an example:
//
//  thread->Add(DeletePointerClosure(expensive_to_delete));
//
template<typename T>
void DeletePointer(T* p) {
  delete p;
}

template<typename T>
Closure* DeletePointerClosure(T* p) {
  return NewCallback(&DeletePointer<T>, p);
}

#endif /* _CALLBACK_H_ */
