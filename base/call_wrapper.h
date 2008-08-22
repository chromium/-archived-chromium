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

#ifndef BASE_CALL_WRAPPER_H_
#define BASE_CALL_WRAPPER_H_

// NOTE: If you want a callback that takes at-call-time parameters, you should
// use Callback (see task.h).  CallWrapper only supports creation-time binding.
//
// A function / method call invocation wrapper.  This creates a "closure" of
// sorts, storing a function pointer (or method pointer), along with a possible
// list of arguments.  The objects have a single method Run(), which will call
// the function / method with the arguments unpacked.  The arguments to the
// wrapped call are supplied at CallWrapper creation time, and no arguments can
// be supplied to the Run() method.
//
// All wrappers should be constructed through the two factory methods:
//   CallWrapper* NewFunctionCallWrapper(funcptr, ...);
//   CallWrapper* NewMethodCallWrapper(obj, &Klass::Method, ...);
//
// The classes memory should be managed like any other dynamically allocated
// object, and deleted when no longer in use.  It is of course wrong to destroy
// the object while a Run() is in progress, or to call Run() after the object
// has been destroyed.  Arguments are copied by value, and kept within the
// CallWrapper object.  You should only pass-by-value simple parameters, or
// pointers to more complex types.  If your parameters need custom memory
// management, then you should probably do this at the same point you destroy
// the CallWrapper.  You should be aware of how your CallWrapper is used, and
// it is valid for Run() to be called 0 or more times.
//
// Some example usage:
//   CallWrapper* wrapper = NewFunctionCallWrapper(&my_func);
//   wrapper->Run();  // my_func();
//   delete wrapper;
//
//   scoped_ptr<CallWrapper> wrapper(NewFunctionCallWrapper(&my_func, 10));
//   wrapper->Run();  // my_func(10);
//
//   MyObject obj;
//   scoped_ptr<CallWrapper> wrapper(
//      NewMethodCallWrapper(&obj, &MyObject::Foo, 1, 2));
//   wrapper->Run();  // obj->Foo(1, 2);

#include "base/tuple.h"

namespace base {

class CallWrapper {
 public:
  virtual ~CallWrapper() { }
  virtual void Run() = 0;
 protected:
  CallWrapper() { }
};

// Wraps a function / static method call.
template <typename FuncType, typename TupleType>
class FunctionCallWrapper : public CallWrapper {
 public:
  FunctionCallWrapper(FuncType func, TupleType params)
      : func_(func), params_(params) { }
  virtual void Run() {
    DispatchToFunction(func_, params_);
  }
 private:
  FuncType func_;
  // We copy from the const& constructor argument to our own copy here.
  TupleType params_;
};

// Wraps a method invocation on an object.
template <typename ObjType, typename MethType, typename TupleType>
class MethodCallWrapper : public CallWrapper {
 public:
  MethodCallWrapper(ObjType* obj, MethType meth, TupleType params)
      : obj_(obj), meth_(meth), params_(params) { }
  virtual void Run() {
    DispatchToMethod(obj_, meth_, params_);
  }
 private:
  ObjType* obj_;
  MethType meth_;
  // We copy from the const& constructor argument to our own copy here.
  TupleType params_;
};

// Factory functions, conveniently creating a Tuple for 0-5 arguments.
template <typename FuncType>
inline CallWrapper* NewFunctionCallWrapper(FuncType func) {
  typedef Tuple0 TupleType;
  return new FunctionCallWrapper<FuncType, TupleType>(
      func, MakeTuple());
}

template <typename FuncType, typename Arg0Type>
inline CallWrapper* NewFunctionCallWrapper(FuncType func,
                                           const Arg0Type& arg0) {
  typedef Tuple1<Arg0Type> TupleType;
  return new FunctionCallWrapper<FuncType, TupleType>(
      func, MakeTuple(arg0));
}

template <typename FuncType, typename Arg0Type, typename Arg1Type>
inline CallWrapper* NewFunctionCallWrapper(FuncType func,
                                           const Arg0Type& arg0,
                                           const Arg1Type& arg1) {
  typedef Tuple2<Arg0Type, Arg1Type> TupleType;
  return new FunctionCallWrapper<FuncType, TupleType>(
      func, MakeTuple(arg0, arg1));
}

template <typename FuncType, typename Arg0Type, typename Arg1Type,
          typename Arg2Type>
inline CallWrapper* NewFunctionCallWrapper(FuncType func,
                                           const Arg0Type& arg0,
                                           const Arg1Type& arg1,
                                           const Arg2Type& arg2) {
  typedef Tuple3<Arg0Type, Arg1Type, Arg2Type> TupleType;
  return new FunctionCallWrapper<FuncType, TupleType>(
      func, MakeTuple(arg0, arg1, arg2));
}

template <typename FuncType, typename Arg0Type, typename Arg1Type,
          typename Arg2Type, typename Arg3Type>
inline CallWrapper* NewFunctionCallWrapper(FuncType func,
                                           const Arg0Type& arg0,
                                           const Arg1Type& arg1,
                                           const Arg2Type& arg2,
                                           const Arg3Type& arg3) {
  typedef Tuple4<Arg0Type, Arg1Type, Arg2Type, Arg3Type> TupleType;
  return new FunctionCallWrapper<FuncType, TupleType>(
      func, MakeTuple(arg0, arg1, arg2, arg3));
}

template <typename FuncType, typename Arg0Type, typename Arg1Type,
          typename Arg2Type, typename Arg3Type, typename Arg4Type>
inline CallWrapper* NewFunctionCallWrapper(FuncType func,
                                           const Arg0Type& arg0,
                                           const Arg1Type& arg1,
                                           const Arg2Type& arg2,
                                           const Arg3Type& arg3,
                                           const Arg4Type& arg4) {
  typedef Tuple5<Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type> TupleType;
  return new FunctionCallWrapper<FuncType, TupleType>(
      func, MakeTuple(arg0, arg1, arg2, arg3, arg4));
}


template <typename ObjType, typename MethType>
inline CallWrapper* NewMethodCallWrapper(ObjType* obj, MethType meth) {
  typedef Tuple0 TupleType;
  return new MethodCallWrapper<ObjType, MethType, TupleType>(
      obj, meth, MakeTuple());
}

template <typename ObjType, typename MethType, typename Arg0Type>
inline CallWrapper* NewMethodCallWrapper(ObjType* obj, MethType meth,
                                         const Arg0Type& arg0) {
  typedef Tuple1<Arg0Type> TupleType;
  return new MethodCallWrapper<ObjType, MethType, TupleType>(
      obj, meth, MakeTuple(arg0));
}

template <typename ObjType, typename MethType, typename Arg0Type,
          typename Arg1Type>
inline CallWrapper* NewMethodCallWrapper(ObjType* obj, MethType meth,
                                         const Arg0Type& arg0,
                                         const Arg1Type& arg1) {
  typedef Tuple2<Arg0Type, Arg1Type> TupleType;
  return new MethodCallWrapper<ObjType, MethType, TupleType>(
      obj, meth, MakeTuple(arg0, arg1));
}

template <typename ObjType, typename MethType, typename Arg0Type,
          typename Arg1Type, typename Arg2Type>
inline CallWrapper* NewMethodCallWrapper(ObjType* obj, MethType meth,
                                         const Arg0Type& arg0,
                                         const Arg1Type& arg1,
                                         const Arg2Type& arg2) {
  typedef Tuple3<Arg0Type, Arg1Type, Arg2Type> TupleType;
  return new MethodCallWrapper<ObjType, MethType, TupleType>(
      obj, meth, MakeTuple(arg0, arg1, arg2));
}

template <typename ObjType, typename MethType, typename Arg0Type,
          typename Arg1Type, typename Arg2Type, typename Arg3Type>
inline CallWrapper* NewMethodCallWrapper(ObjType* obj, MethType meth,
                                         const Arg0Type& arg0,
                                         const Arg1Type& arg1,
                                         const Arg2Type& arg2,
                                         const Arg3Type& arg3) {
  typedef Tuple4<Arg0Type, Arg1Type, Arg2Type, Arg3Type> TupleType;
  return new MethodCallWrapper<ObjType, MethType, TupleType>(
      obj, meth, MakeTuple(arg0, arg1, arg2, arg3));
}

template <typename ObjType, typename MethType, typename Arg0Type,
          typename Arg1Type, typename Arg2Type, typename Arg3Type,
          typename Arg4Type>
inline CallWrapper* NewMethodCallWrapper(ObjType* obj, MethType meth,
                                         const Arg0Type& arg0,
                                         const Arg1Type& arg1,
                                         const Arg2Type& arg2,
                                         const Arg3Type& arg3,
                                         const Arg4Type& arg4) {
  typedef Tuple5<Arg0Type, Arg1Type, Arg2Type, Arg3Type, Arg4Type> TupleType;
  return new MethodCallWrapper<ObjType, MethType, TupleType>(
      obj, meth, MakeTuple(arg0, arg1, arg2, arg3, arg4));
}

}  // namespace base

#endif  // BASE_CALL_WRAPPER_H_
