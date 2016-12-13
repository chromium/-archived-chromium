// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Additional set of macros for the JS RPC.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_
#define WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_

#include <string>

#include <wtf/OwnPtr.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/values.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/devtools/devtools_rpc.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe.h"

///////////////////////////////////////////////////////
// JS RPC binds and stubs

template<typename T>
struct RpcJsTypeTrait {
};

template<>
struct RpcJsTypeTrait<bool> {
  static bool Pass(const CppVariant& var) {
    return var.ToBoolean();
  }
};

template<>
struct RpcJsTypeTrait<int> {
  static int Pass(const CppVariant& var) {
    return var.ToInt32();
  }
};

template<>
struct RpcJsTypeTrait<String> {
  static String Pass(const CppVariant& var) {
    return webkit_glue::StdStringToString(var.ToString());
  }
};

template<>
struct RpcJsTypeTrait<std::string> {
  static std::string Pass(const CppVariant& var) {
    return var.ToString();
  }
};

#define TOOLS_RPC_JS_BIND_METHOD0(Method) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD1(Method, T1) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD2(Method, T1, T2) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD3(Method, T1, T2, T3) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_BIND_METHOD4(Method, T1, T2, T3, T4) \
  BindMethod(#Method, &OCLASS::Js##Method);

#define TOOLS_RPC_JS_STUB_METHOD0(Method) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    InvokeAsync(class_name, #Method); \
    result->SetNull(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD1(Method, T1) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    T1 t1 = RpcJsTypeTrait<T1>::Pass(args[0]); \
    InvokeAsync(class_name, #Method, &t1); \
    result->SetNull(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD2(Method, T1, T2) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    T1 t1 = RpcJsTypeTrait<T1>::Pass(args[0]); \
    T2 t2 = RpcJsTypeTrait<T2>::Pass(args[1]); \
    InvokeAsync(class_name, #Method, &t1, &t2); \
    result->SetNull(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD3(Method, T1, T2, T3) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    T1 t1 = RpcJsTypeTrait<T1>::Pass(args[0]); \
    T2 t2 = RpcJsTypeTrait<T2>::Pass(args[1]); \
    T3 t3 = RpcJsTypeTrait<T3>::Pass(args[2]); \
    InvokeAsync(class_name, #Method, &t1, &t2, &t3); \
    result->SetNull(); \
  }

#define TOOLS_RPC_JS_STUB_METHOD4(Method, T1, T2, T3, T4) \
  void Js##Method(const CppArgumentList& args, CppVariant* result) { \
    T1 t1 = RpcJsTypeTrait<T1>::Pass(args[0]); \
    T2 t2 = RpcJsTypeTrait<T2>::Pass(args[1]); \
    T3 t3 = RpcJsTypeTrait<T3>::Pass(args[2]); \
    T4 t4 = RpcJsTypeTrait<T4>::Pass(args[3]); \
    InvokeAsync(class_name, #Method, &t1, &t2, &t3, &t4); \
    result->SetNull(); \
  }

///////////////////////////////////////////////////////
// JS RPC main obj macro

#define DEFINE_RPC_JS_BOUND_OBJ(Class, STRUCT, DClass, DELEGATE_STRUCT) \
class Js##Class##BoundObj : public Class##Stub, \
                            public CppBoundClass { \
 public: \
  Js##Class##BoundObj(Delegate* rpc_delegate, WebFrame* frame, \
      const std::wstring& classname) \
      : Class##Stub(rpc_delegate) { \
    BindToJavascript(frame, classname); \
    STRUCT( \
        TOOLS_RPC_JS_BIND_METHOD0, \
        TOOLS_RPC_JS_BIND_METHOD1, \
        TOOLS_RPC_JS_BIND_METHOD2, \
        TOOLS_RPC_JS_BIND_METHOD3, \
        TOOLS_RPC_JS_BIND_METHOD4) \
  } \
  virtual ~Js##Class##BoundObj() {} \
  typedef Js##Class##BoundObj OCLASS; \
  STRUCT( \
      TOOLS_RPC_JS_STUB_METHOD0, \
      TOOLS_RPC_JS_STUB_METHOD1, \
      TOOLS_RPC_JS_STUB_METHOD2, \
      TOOLS_RPC_JS_STUB_METHOD3, \
      TOOLS_RPC_JS_STUB_METHOD4) \
 private: \
  DISALLOW_COPY_AND_ASSIGN(Js##Class##BoundObj); \
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_JS_H_
