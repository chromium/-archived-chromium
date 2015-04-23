// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_TREE_NODE_ITERATOR_H_
#define APP_TREE_NODE_ITERATOR_H_

#include <stack>

#include "base/basictypes.h"
#include "base/logging.h"

// Iterator that iterates over the descendants of a node. The iteration does
// not include the node itself, only the descendants. The following illustrates
// typical usage:
// while (iterator.has_next()) {
//   Node* node = iterator.Next();
//   // do something with node.
// }
template <class NodeType>
class TreeNodeIterator {
 public:
  explicit TreeNodeIterator(NodeType* node) {
    if (node->GetChildCount() > 0)
      positions_.push(Position<NodeType>(node, 0));
  }

  // Returns true if there are more descendants.
  bool has_next() const { return !positions_.empty(); }

  // Returns the next descendant.
  NodeType* Next() {
    if (!has_next()) {
      NOTREACHED();
      return NULL;
    }

    NodeType* result = positions_.top().node->GetChild(positions_.top().index);

    // Make sure we don't attempt to visit result again.
    positions_.top().index++;

    // Iterate over result's children.
    positions_.push(Position<NodeType>(result, 0));

    // Advance to next position.
    while (!positions_.empty() && positions_.top().index >=
           positions_.top().node->GetChildCount()) {
      positions_.pop();
    }

    return result;
  }

 private:
  template <class PositionNodeType>
  struct Position {
    Position(PositionNodeType* node, int index) : node(node), index(index) {}
    Position() : node(NULL), index(-1) {}

    PositionNodeType* node;
    int index;
  };

  std::stack<Position<NodeType> > positions_;

  DISALLOW_COPY_AND_ASSIGN(TreeNodeIterator);
};

#endif  // APP_TREE_NODE_ITERATOR_H_
