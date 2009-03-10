// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
