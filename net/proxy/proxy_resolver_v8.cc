// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/proxy/proxy_resolver_v8.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_resolver_script.h"
#include "v8/include/v8.h"


namespace net {

namespace {

// Pseudo-name for the PAC script.
const char kPacResourceName[] = "proxy-pac-script.js";

// Convert a V8 String to a std::string.
std::string V8StringToStdString(v8::Handle<v8::String> s) {
  int len = s->Utf8Length();
  std::string result;
  s->WriteUtf8(WriteInto(&result, len + 1), len);
  return result;
}

// Convert a std::string to a V8 string.
v8::Local<v8::String> StdStringToV8String(const std::string& s) {
  return v8::String::New(s.data(), s.size());
}

// String-ize a V8 object by calling its toString() method. Returns true
// on success. This may fail if the toString() throws an exception.
bool V8ObjectToString(v8::Handle<v8::Value> object, std::string* result) {
  if (object.IsEmpty())
    return false;

  v8::HandleScope scope;
  v8::Local<v8::String> str_object = object->ToString();
  if (str_object.IsEmpty())
    return false;
  *result = V8StringToStdString(str_object);
  return true;
}

// JSBIndings implementation.
class DefaultJSBindings : public ProxyResolverV8::JSBindings {
 public:
  // Handler for "alert(message)".
  virtual void Alert(const std::string& message) {
    LOG(INFO) << "PAC-alert: " << message;
  }

  // Handler for "myIpAddress()". Returns empty string on failure.
  virtual std::string MyIpAddress() {
    // TODO(eroman):
    NOTIMPLEMENTED();
    return "127.0.0.1";
  }

  // Handler for "dnsResolve(host)". Returns empty string on failure.
  virtual std::string DnsResolve(const std::string& host) {
    // Try to resolve synchronously.
    net::AddressList address_list;
    const int kPort = 80;  // Doesn't matter what this is.
    int result = host_resolver_.Resolve(host, kPort, &address_list, NULL);

    if (result != OK)
      return std::string();  // Failed.

    if (!address_list.head())
      return std::string();

    // There may be multiple results; we will just use the first one.
    // This returns empty string on failure.
    return net::NetAddressToString(address_list.head());
  }

  // Handler for when an error is encountered. |line_number| may be -1.
  virtual void OnError(int line_number, const std::string& message) {
    if (line_number == -1)
      LOG(INFO) << "PAC-error: " << message;
    else
      LOG(INFO) << "PAC-error: " << "line: " << line_number << ": " << message;
  }

 private:
  HostResolver host_resolver_;
};

}  // namespace

// ProxyResolverV8::Context ---------------------------------------------------

class ProxyResolverV8::Context {
 public:
  Context(JSBindings* js_bindings, const std::string& pac_data)
       : js_bindings_(js_bindings) {
    DCHECK(js_bindings != NULL);
    InitV8(pac_data);
  }

  ~Context() {
    v8::Locker locked;

    v8_this_.Dispose();
    v8_context_.Dispose();
  }

  int ResolveProxy(const GURL& query_url, ProxyInfo* results) {
    v8::Locker locked;
    v8::HandleScope scope;

    v8::Context::Scope function_scope(v8_context_);

    v8::Local<v8::Value> function =
        v8_context_->Global()->Get(v8::String::New("FindProxyForURL"));
    if (!function->IsFunction()) {
      js_bindings_->OnError(-1, "FindProxyForURL() is undefined.");
      return ERR_PAC_SCRIPT_FAILED;
    }

    v8::Handle<v8::Value> argv[] = {
      StdStringToV8String(query_url.spec()),
      StdStringToV8String(query_url.host()),
    };

    v8::TryCatch try_catch;
    v8::Local<v8::Value> ret = v8::Function::Cast(*function)->Call(
        v8_context_->Global(), arraysize(argv), argv);

    if (try_catch.HasCaught()) {
      HandleError(try_catch.Message());
      return ERR_PAC_SCRIPT_FAILED;
    }

    if (!ret->IsString()) {
      js_bindings_->OnError(-1, "FindProxyForURL() did not return a string.");
      return ERR_PAC_SCRIPT_FAILED;
    }

    std::string ret_str = V8StringToStdString(ret->ToString());

    results->UsePacString(ret_str);

    return OK;
  }

 private:
  void InitV8(const std::string& pac_data) {
    v8::Locker locked;
    v8::HandleScope scope;

    v8_this_ = v8::Persistent<v8::External>::New(v8::External::New(this));
    v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();

    // Attach the javascript bindings.
    v8::Local<v8::FunctionTemplate> alert_template =
        v8::FunctionTemplate::New(&AlertCallback, v8_this_);
    global_template->Set(v8::String::New("alert"), alert_template);

    v8::Local<v8::FunctionTemplate> my_ip_address_template =
        v8::FunctionTemplate::New(&MyIpAddressCallback, v8_this_);
    global_template->Set(v8::String::New("myIpAddress"),
        my_ip_address_template);

    v8::Local<v8::FunctionTemplate> dns_resolve_template =
        v8::FunctionTemplate::New(&DnsResolveCallback, v8_this_);
    global_template->Set(v8::String::New("dnsResolve"),
        dns_resolve_template);

    v8_context_ = v8::Context::New(NULL, global_template);

    v8::Context::Scope ctx(v8_context_);

    v8::TryCatch try_catch;

    // Compile the script, including the PAC library functions.
    std::string text_raw = pac_data + PROXY_RESOLVER_SCRIPT;
    v8::Local<v8::String> text = StdStringToV8String(text_raw);
    v8::ScriptOrigin origin = v8::ScriptOrigin(
        v8::String::New(kPacResourceName));
    v8::Local<v8::Script> code = v8::Script::Compile(text, &origin);

    // Execute.
    if (!code.IsEmpty())
      code->Run();

    if (try_catch.HasCaught())
      HandleError(try_catch.Message());
  }

  // Handle an exception thrown by V8.
  void HandleError(v8::Handle<v8::Message> message) {
    if (message.IsEmpty())
      return;

    // Otherwise dispatch to the bindings.
    int line_number = message->GetLineNumber();
    std::string error_message;
    V8ObjectToString(message->Get(), &error_message);
    js_bindings_->OnError(line_number, error_message);
  }

  // V8 callback for when "alert()" is invoked by the PAC script.
  static v8::Handle<v8::Value> AlertCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // Like firefox we assume "undefined" if no argument was specified, and
    // disregard any arguments beyond the first.
    std::string message;
    if (args.Length() == 0) {
      message = "undefined";
    } else {
      if (!V8ObjectToString(args[0], &message))
        return v8::Undefined();  // toString() threw an exception.
    }

    context->js_bindings_->Alert(message);
    return v8::Undefined();
  }

  // V8 callback for when "myIpAddress()" is invoked by the PAC script.
  static v8::Handle<v8::Value> MyIpAddressCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // We shouldn't be called with any arguments, but will not complain if
    // we are.
    std::string result = context->js_bindings_->MyIpAddress();
    return StdStringToV8String(result);
  }

  // V8 callback for when "dnsResolve()" is invoked by the PAC script.
  static v8::Handle<v8::Value> DnsResolveCallback(const v8::Arguments& args) {
    Context* context =
        static_cast<Context*>(v8::External::Cast(*args.Data())->Value());

    // We need at least one argument.
    std::string host;
    if (args.Length() == 0) {
      host = "undefined";
    } else {
      if (!V8ObjectToString(args[0], &host))
        return v8::Undefined();
    }

    std::string result = context->js_bindings_->DnsResolve(host);

    // DoDnsResolve() returns empty string on failure.
    return result.empty() ? v8::Null() : StdStringToV8String(result);
  }

  JSBindings* js_bindings_;
  HostResolver host_resolver_;
  v8::Persistent<v8::External> v8_this_;
  v8::Persistent<v8::Context> v8_context_;
};

// ProxyResolverV8 ------------------------------------------------------------

// the |false| argument to ProxyResolver means the ProxyService will handle
// downloading of the PAC script, and notify changes through SetPacScript().
ProxyResolverV8::ProxyResolverV8()
    : ProxyResolver(false /*does_fetch*/),
      js_bindings_(new DefaultJSBindings()) {
}

ProxyResolverV8::ProxyResolverV8(
    ProxyResolverV8::JSBindings* custom_js_bindings)
    : ProxyResolver(false), js_bindings_(custom_js_bindings) {
}

ProxyResolverV8::~ProxyResolverV8() {}

int ProxyResolverV8::GetProxyForURL(const GURL& query_url,
                                    const GURL& /*pac_url*/,
                                    ProxyInfo* results) {
  // If the V8 instance has not been initialized (either because SetPacScript()
  // wasn't called yet, or because it was called with empty string).
  if (!context_.get())
    return ERR_FAILED;

  // Otherwise call into V8.
  return context_->ResolveProxy(query_url, results);
}

void ProxyResolverV8::SetPacScript(const std::string& data) {
  context_.reset();
  if (!data.empty())
    context_.reset(new Context(js_bindings_.get(), data));
}

}  // namespace net
