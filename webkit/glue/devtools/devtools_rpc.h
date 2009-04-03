// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DevTools RPC subsystem is a simple string serialization-based rpc
// implementation. The client is responsible for defining the Rpc-enabled
// interface in terms of its macros:
//
// #define MYAPI_STRUCT(METHOD0, METHOD1, METHOD2, METHOD3, METHOD4)
//   METHOD0(Method1)
//   METHOD3(Method2, int, String, Value)
//   METHOD1(Method3, int)
// (snippet above should be multiline macro, add trailing backslashes)
//
// DEFINE_RPC_CLASS(MyApi, MYAPI_STRUCT)
//
// The snippet above will generate three classes: MyApi, MyApiStub and
// MyApiDispatch.
//
// 1. For each method defined in the marco MyApi will have a
// pure virtual function generated, so that MyApi would look like:
//
// class MyApi {
//  private:
//   MyApi() {}
//   ~MyApi() {}
//   virtual void Method1() = 0;
//   virtual void Method2(
//       int param1,
//       const String& param2,
//       const Value& param3) = 0;
//   virtual void Method3(int param1) = 0;
// };
//
// 2. MyApiStub will implement MyApi interface and would serialize all calls
// into the string-based calls of the underlying transport:
//
// DevToolsRpc::Delegate* transport;
// my_api = new MyApiStub(transport);
// my_api->Method1();
// my_api->Method3(2);
//
// 3. MyApiDelegate is capable of dispatching the calls and convert them to the
// calls to the underlying MyApi methods:
//
// MyApi* real_object;
// MyApiDispatch::Dispatch(real_object, raw_string_call_generated_by_stub);
//
// will make corresponding calls to the real object.

#ifndef WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_H_
#define WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_H_

#include <string>

#include <wtf/OwnPtr.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/values.h"

namespace WebCore {
class String;
}

using WebCore::String;

///////////////////////////////////////////////////////
// RPC dispatch macro

template<typename T>
struct RpcTypeTrait {
  typedef T ApiType;
  typedef T DispatchType;
  static const DispatchType& Pass(const DispatchType& t) {
    return t;
  }
};

template<>
struct RpcTypeTrait<Value> {
  typedef const Value& ApiType;
  typedef Value* DispatchType;
  static const Value& Pass(Value* t) {
    return *t;
  }
};

template<>
struct RpcTypeTrait<String> {
  typedef const String& ApiType;
  typedef String DispatchType;
  static const DispatchType& Pass(const DispatchType& t) {
    return t;
  }
};

template<>
struct RpcTypeTrait<std::string> {
  typedef const std::string& ApiType;
  typedef std::string DispatchType;
  static const DispatchType& Pass(const DispatchType& t) {
    return t;
  }
};

///////////////////////////////////////////////////////
// RPC Api method declarations

#define TOOLS_RPC_API_METHOD0(Method) \
  virtual void Method() = 0;

#define TOOLS_RPC_API_METHOD1(Method, T1) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1) = 0;

#define TOOLS_RPC_API_METHOD2(Method, T1, T2) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2) = 0;

#define TOOLS_RPC_API_METHOD3(Method, T1, T2, T3) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3) = 0;

#define TOOLS_RPC_API_METHOD4(Method, T1, T2, T3, T4) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3, \
                      RpcTypeTrait<T4>::ApiType t4) = 0;

///////////////////////////////////////////////////////
// RPC stub method implementations

#define TOOLS_RPC_STUB_METHOD0(Method) \
  virtual void Method() { \
    InvokeAsync(class_name, #Method); \
  }

#define TOOLS_RPC_STUB_METHOD1(Method, T1) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1) { \
    InvokeAsync(class_name, #Method, &t1); \
  }

#define TOOLS_RPC_STUB_METHOD2(Method, T1, T2) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2) { \
    InvokeAsync(class_name, #Method, &t1, &t2); \
  }

#define TOOLS_RPC_STUB_METHOD3(Method, T1, T2, T3) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3) { \
    InvokeAsync(class_name, #Method, &t1, &t2, &t3); \
  }

#define TOOLS_RPC_STUB_METHOD4(Method, T1, T2, T3, T4) \
  virtual void Method(RpcTypeTrait<T1>::ApiType t1, \
                      RpcTypeTrait<T2>::ApiType t2, \
                      RpcTypeTrait<T3>::ApiType t3, \
                      RpcTypeTrait<T4>::ApiType t4) { \
    InvokeAsync(class_name, #Method, &t1, &t2, &t3, &t4); \
  }

///////////////////////////////////////////////////////
// RPC dispatch method implementations

#define TOOLS_RPC_DISPATCH0(Method) \
if (method_name == #Method) { \
  delegate->Method(); \
  return true; \
}

#define TOOLS_RPC_DISPATCH1(Method, T1) \
if (method_name == #Method) { \
  RpcTypeTrait<T1>::DispatchType t1; \
  DevToolsRpc::GetListValue(message, 2, &t1); \
  delegate->Method( \
      RpcTypeTrait<T1>::Pass(t1)); \
  return true; \
}

#define TOOLS_RPC_DISPATCH2(Method, T1, T2) \
if (method_name == #Method) { \
  RpcTypeTrait<T1>::DispatchType t1; \
  RpcTypeTrait<T2>::DispatchType t2; \
  DevToolsRpc::GetListValue(message, 2, &t1); \
  DevToolsRpc::GetListValue(message, 3, &t2); \
  delegate->Method( \
      RpcTypeTrait<T1>::Pass(t1), \
      RpcTypeTrait<T2>::Pass(t2) \
  ); \
  return true; \
}

#define TOOLS_RPC_DISPATCH3(Method, T1, T2, T3) \
if (method_name == #Method) { \
  RpcTypeTrait<T1>::DispatchType t1; \
  RpcTypeTrait<T2>::DispatchType t2; \
  RpcTypeTrait<T3>::DispatchType t3; \
  DevToolsRpc::GetListValue(message, 2, &t1); \
  DevToolsRpc::GetListValue(message, 3, &t2); \
  DevToolsRpc::GetListValue(message, 4, &t3); \
  delegate->Method( \
      RpcTypeTrait<T1>::Pass(t1), \
      RpcTypeTrait<T2>::Pass(t2), \
      RpcTypeTrait<T3>::Pass(t3) \
  ); \
  return true; \
}

#define TOOLS_RPC_DISPATCH4(Method, T1, T2, T3, T4) \
if (method_name == #Method) { \
  RpcTypeTrait<T1>::DispatchType t1; \
  RpcTypeTrait<T2>::DispatchType t2; \
  RpcTypeTrait<T3>::DispatchType t3; \
  RpcTypeTrait<T4>::DispatchType t4; \
  DevToolsRpc::GetListValue(message, 2, &t1); \
  DevToolsRpc::GetListValue(message, 3, &t2); \
  DevToolsRpc::GetListValue(message, 4, &t3); \
  DevToolsRpc::GetListValue(message, 5, &t4); \
  delegate->Method( \
      RpcTypeTrait<T1>::Pass(t1), \
      RpcTypeTrait<T2>::Pass(t2), \
      RpcTypeTrait<T3>::Pass(t3), \
      RpcTypeTrait<T4>::Pass(t4) \
  ); \
  return true; \
}

#define TOOLS_END_RPC_DISPATCH() \
}

// This macro defines three classes: Class with the Api, ClassStub that is
// serializing method calls and ClassDispatch that is capable of dispatching
// the serialized message into its delegate.
#define DEFINE_RPC_CLASS(Class, STRUCT) \
class Class {\
 public: \
  Class() { \
    class_name = #Class; \
  } \
  ~Class() {} \
  \
  STRUCT( \
      TOOLS_RPC_API_METHOD0, \
      TOOLS_RPC_API_METHOD1, \
      TOOLS_RPC_API_METHOD2, \
      TOOLS_RPC_API_METHOD3, \
      TOOLS_RPC_API_METHOD4) \
  std::string class_name; \
 private: \
  DISALLOW_COPY_AND_ASSIGN(Class); \
}; \
\
class Class##Stub : public Class, public DevToolsRpc { \
 public: \
  explicit Class##Stub(Delegate* delegate) : DevToolsRpc(delegate) {} \
  virtual ~Class##Stub() {} \
  typedef Class CLASS; \
  STRUCT( \
      TOOLS_RPC_STUB_METHOD0, \
      TOOLS_RPC_STUB_METHOD1, \
      TOOLS_RPC_STUB_METHOD2, \
      TOOLS_RPC_STUB_METHOD3, \
      TOOLS_RPC_STUB_METHOD4) \
 private: \
  DISALLOW_COPY_AND_ASSIGN(Class##Stub); \
}; \
\
class Class##Dispatch { \
 public: \
  Class##Dispatch() {} \
  virtual ~Class##Dispatch() {} \
  \
  static bool Dispatch(Class* delegate, const std::string& raw_msg) { \
    OwnPtr<ListValue> message( \
        static_cast<ListValue*>(DevToolsRpc::ParseMessage(raw_msg))); \
    return Dispatch(delegate, *message.get()); \
  } \
  \
  static bool Dispatch(Class* delegate, const ListValue& message) { \
    std::string class_name; \
    message.GetString(0, &class_name); \
    if (class_name != #Class) { \
      return false; \
    } \
    std::string method_name; \
    message.GetString(1, &method_name); \
    typedef Class CLASS; \
    STRUCT( \
        TOOLS_RPC_DISPATCH0, \
        TOOLS_RPC_DISPATCH1, \
        TOOLS_RPC_DISPATCH2, \
        TOOLS_RPC_DISPATCH3, \
        TOOLS_RPC_DISPATCH4) \
    return false; \
  } \
 private: \
  DISALLOW_COPY_AND_ASSIGN(Class##Dispatch); \
};

///////////////////////////////////////////////////////
// RPC base class
class DevToolsRpc {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    virtual void SendRpcMessage(const std::string& msg) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  explicit DevToolsRpc(Delegate* delegate);
  virtual ~DevToolsRpc();

  void InvokeAsync(
      const std::string& class_name,
      const std::string& method_name) {
    ListValue message;
    message.Append(CreateValue(&class_name));
    message.Append(CreateValue(&method_name));
    SendValueMessage(message);
  }

  template<typename T1>
  void InvokeAsync(
      const std::string& class_name,
      const std::string& method_name,
      T1 t1) {
    ListValue message;
    message.Append(CreateValue(&class_name));
    message.Append(CreateValue(&method_name));
    message.Append(CreateValue(t1));
    SendValueMessage(message);
  }

  template<typename T1, typename T2>
  void InvokeAsync(
      const std::string& class_name,
      const std::string& method_name,
      T1 t1, T2 t2) {
    ListValue message;
    message.Append(CreateValue(&class_name));
    message.Append(CreateValue(&method_name));
    message.Append(CreateValue(t1));
    message.Append(CreateValue(t2));
    SendValueMessage(message);
  }

  template<typename T1, typename T2, typename T3>
  void InvokeAsync(
      const std::string& class_name,
      const std::string& method_name,
      T1 t1, T2 t2, T3 t3) {
    ListValue message;
    message.Append(CreateValue(&class_name));
    message.Append(CreateValue(&method_name));
    message.Append(CreateValue(t1));
    message.Append(CreateValue(t2));
    message.Append(CreateValue(t3));
    SendValueMessage(message);
  }

  template<typename T1, typename T2, typename T3, typename T4>
  void InvokeAsync(
      const std::string& class_name,
      const std::string& method_name,
      T1 t1, T2 t2, T3 t3, T4 t4) {
    ListValue message;
    message.Append(CreateValue(&class_name));
    message.Append(CreateValue(&method_name));
    message.Append(CreateValue(t1));
    message.Append(CreateValue(t2));
    message.Append(CreateValue(t3));
    message.Append(CreateValue(t4));
    SendValueMessage(message);
  }

  static Value* ParseMessage(const std::string& raw_msg);
  static std::string Serialize(const Value& value);
  static void GetListValue(const ListValue& message, int index, bool* value);
  static void GetListValue(const ListValue& message, int index, int* value);
  static void GetListValue(
      const ListValue& message,
      int index,
      String* value);
  static void GetListValue(
      const ListValue& message,
      int index,
      std::string* value);
  static void GetListValue(const ListValue& message, int index, Value** value);

 protected:
  // Primarily for unit testing.
  void set_delegate(Delegate* delegate) { this->delegate_ = delegate; }

 private:
  // Value adapters for supported Rpc types.
  static Value* CreateValue(const String* value);
  static Value* CreateValue(const std::string* value);
  static Value* CreateValue(int* value);
  static Value* CreateValue(bool* value);
  static Value* CreateValue(const Value* value);

  void SendValueMessage(const Value& value);

  Delegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsRpc);
};

#endif  // WEBKIT_GLUE_DEVTOOLS_DEVTOOLS_RPC_H_
