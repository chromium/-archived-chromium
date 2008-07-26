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

/*
  ExternalJSObject class:
  Bound to a JavaScript window.external object using
  CppBoundClass::BindToJavascript(), this adds methods accessible from JS for
  compatibility with other browsers.
*/

#ifndef CHROME_RENDERER_EXTERNAL_JS_OBJECT_H__
#define CHROME_RENDERER_EXTERNAL_JS_OBJECT_H__

#include "base/basictypes.h"
#include "webkit/glue/cpp_bound_class.h"

class RenderView;

class ExternalJSObject : public CppBoundClass {
 public:
  // Builds the property and method lists needed to bind this class to a JS
  // object.
  ExternalJSObject();

  // A RenderView must be set before AddSearchProvider is called, or the call
  // will do nothing.
  void set_render_view(RenderView* rv) { render_view_ = rv; }

  // Given a URL to an OpenSearch document in the first argument, adds the
  // corresponding search provider as a keyword search.  The nonstandard
  // capitalization is for compatibility with Firefox and IE.
  void AddSearchProvider(const CppArgumentList& args, CppVariant* result);

 private:
  RenderView* render_view_;

  DISALLOW_EVIL_CONSTRUCTORS(ExternalJSObject);
};

#endif  // CHROME_RENDERER_EXTERNAL_JS_OBJECT_H__
