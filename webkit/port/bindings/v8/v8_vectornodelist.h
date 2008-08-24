// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VECTORNODELIST_H__
#define V8_VECTORNODELIST_H__

#include "config.h"
#include <v8.h>
#include "Node.h"
#include "NodeList.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

// A NodeList backed by a Vector of Nodes.
class V8VectorNodeList : public NodeList {
 public:
  explicit V8VectorNodeList(const Vector<RefPtr<Node> >& nodes)
    : m_nodes(nodes) { }
  virtual unsigned length() const;
  virtual Node* item(unsigned index) const;
  virtual Node* itemWithName(const AtomicString& name) const;

 private:
  Vector<RefPtr<Node> > m_nodes;
};


}  // namespace WebCore

#endif  // V8_VECTORNODELIST_H__

