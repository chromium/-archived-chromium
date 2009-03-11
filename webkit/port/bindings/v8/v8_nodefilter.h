// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NODEFILTER_H__
#define V8_NODEFILTER_H__

#include <v8.h>
#include "NodeFilterCondition.h"

// NodeFilter is a JavaScript function that takes a Node as parameter
// and returns a short (ACCEPT, SKIP, REJECT) as the result.
namespace WebCore {
class Node;
class ScriptState;

// NodeFilterCondition is a wrapper around a NodeFilter JS function.
class V8NodeFilterCondition : public NodeFilterCondition {
 public:
  explicit V8NodeFilterCondition(v8::Handle<v8::Value> filter);
  virtual ~V8NodeFilterCondition();

  virtual short acceptNode(ScriptState*, Node*) const;

 private:
  mutable v8::Persistent<v8::Value> m_filter;
};

}  // namesapce WebCore
#endif  // V8_NODEFILTER_H__
