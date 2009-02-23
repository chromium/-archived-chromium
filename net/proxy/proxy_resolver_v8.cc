// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/proxy/proxy_resolver_v8.h"

#include "net/base/net_errors.h"
#include "net/proxy/proxy_resolver_script.h"
#include "v8/include/v8.h"

// TODO(eroman):
//  - javascript binding for "alert()"
//  - javascript binding for "myIpAddress()"
//  - javascript binding for "dnsResolve()"
//  - log errors for PAC authors (like if "FindProxyForURL" is missing, or
//    the javascript was malformed, etc..

namespace net {

namespace {

std::string V8StringToStdString(v8::Handle<v8::String> s) {
  int len = s->Utf8Length();
  std::string result;
  s->WriteUtf8(WriteInto(&result, len + 1), len);
  return result;
}

v8::Local<v8::String> StdStringToV8String(const std::string& s) {
  return v8::String::New(s.data(), s.size());
}

}  // namespace

// ProxyResolverV8::Context ---------------------------------------------------

class ProxyResolverV8::Context {
 public:
  explicit Context(const std::string& pac_data) {
    InitV8(pac_data);
  }

  ~Context() {
    v8::Locker locked;
    v8::HandleScope scope;

    v8_context_.Dispose();
  }

  int ResolveProxy(const GURL& query_url, ProxyInfo* results) {
    v8::Locker locked;
    v8::HandleScope scope;

    v8::Context::Scope function_scope(v8_context_);

    v8::Local<v8::Value> function =
        v8_context_->Global()->Get(v8::String::New("FindProxyForURL"));
    if (!function->IsFunction())
      return ERR_PAC_SCRIPT_FAILED;

    v8::Handle<v8::Value> argv[] = {
      StdStringToV8String(query_url.spec()),
      StdStringToV8String(query_url.host()),
    };

    v8::Local<v8::Value> ret = v8::Function::Cast(*function)->Call(
        v8_context_->Global(), arraysize(argv), argv);

    // The handle is empty if an exception was thrown from "FindProxyForURL".
    if (ret.IsEmpty())
      return ERR_PAC_SCRIPT_FAILED;

    if (!ret->IsString())
      return ERR_PAC_SCRIPT_FAILED;

    std::string ret_str = V8StringToStdString(ret->ToString());

    results->UsePacString(ret_str);

    return OK;
  }

 private:
  void InitV8(const std::string& pac_data) {
    v8::Locker locked;
    v8::HandleScope scope;

    v8::Local<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();

    v8_context_ = v8::Context::New(NULL, global_template);
    v8::Context::Scope ctx(v8_context_);

    std::string text_raw = pac_data + PROXY_RESOLVER_SCRIPT;

    v8::Local<v8::String> text = StdStringToV8String(text_raw);
    v8::ScriptOrigin origin = v8::ScriptOrigin(v8::String::New(""));
    v8::Local<v8::Script> code = v8::Script::Compile(text, &origin);
    if (!code.IsEmpty())
      code->Run();
  }

  v8::Persistent<v8::Context> v8_context_;
};

// ProxyResolverV8 ------------------------------------------------------------

// the |false| argument to ProxyResolver means the ProxyService will handle
// downloading of the PAC script, and notify changes through SetPacScript().
ProxyResolverV8::ProxyResolverV8() : ProxyResolver(false /*does_fetch*/) {}

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
    context_.reset(new Context(data));
}

}  // namespace net
