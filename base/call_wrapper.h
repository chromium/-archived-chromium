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

// WARNING: This is currently in competition with Task related callbacks, see
//          task.h, you should maybe be using those interfaces instead.
//
// A function / method call invocation wrapper.  This creates a "closure" of
// sorts, storing a function pointer (or method pointer), along with a possible
// list of arguments.  The objects have a single method Run(), which will call
// the function / method with the arguments unpacked.  Memory is automatically
// managed, a Wrapper is only good for 1 Run(), and will delete itself after.
//
// All wrappers should be constructed through the two factory methods:
//   CallWrapper* NewFunctionCallWrapper(funcptr, ...);
//   CallWrapper* NewMethodCallWrapper(obj, &Klass::Method, ...);
//
// The arguments are copied by value, and kept within the CallWrapper object.
// The parameters should be simple types, or pointers to more complex types.
// The copied parameters will be automatically destroyed, but any pointers,
// or other objects that need to be managed should be managed by the callback.
// If your Run method does custom cleanup, and Run is never called, this could
// of course leak that manually managed memory.
//
// Some example usage:
//   CallWrapper* wrapper = NewFunctionCallWrapper(&my_func);
//   wrapper->Run();  // my_func(); delete wrapper;
//   // wrapper is no longer valid.
//
//   CallWrapper* wrapper = NewFunctionCallWrapper(&my_func, 10);
//   wrapper->Run();  // my_func(10); delete wrapper;
//   // wrapper is no longer valid.
//
//   MyObject obj;
//   CallWrapper* wrapper = NewMethodCallWrapper(&obj, &MyObject::Foo, 1, 2);
//   wrapper->Run();  // obj->Foo(1, 2); delete wrapper;
//   // wrapper is no longer valid.

#include "base/tuple.h"

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
    delete this;
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
    delete this;
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

#endif  // BASE_CALL_WRAPPER_H_
