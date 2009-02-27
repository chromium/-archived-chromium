// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_VIEWS_TREE_MODEL_H_
#define CHROME_VIEWS_TREE_MODEL_H_

#include <string>

#include "base/logging.h"

class SkBitmap;

namespace views {

class TreeModel;

// TreeModelNode --------------------------------------------------------------

// Type of class returned from the model.
class TreeModelNode {
 public:
  // Returns the title for the node.
  virtual std::wstring GetTitle() = 0;
};

// Observer for the TreeModel. Notified of significant events to the model.
class TreeModelObserver {
 public:
  // Notification that nodes were added to the specified parent.
  virtual void TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count) = 0;

  // Notification that nodes were removed from the specified parent.
  virtual void TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count) = 0;

  // Notification the children of |parent| have been reordered. Note, only
  // the direct children of |parent| have been reordered, not descendants.
  virtual void TreeNodeChildrenReordered(TreeModel* model,
                                         TreeModelNode* parent) = 0;

  // Notification that the contents of a node has changed.
  virtual void TreeNodeChanged(TreeModel* model, TreeModelNode* node) = 0;
};

// TreeModel ------------------------------------------------------------------

// The model for TreeView.
class TreeModel {
 public:
  // Returns the root of the tree. This may or may not be shown in the tree,
  // see SetRootShown for details.
  virtual TreeModelNode* GetRoot() = 0;

  // Returns the number of children in the specified node.
  virtual int GetChildCount(TreeModelNode* parent) = 0;

  // Returns the child node at the specified index.
  virtual TreeModelNode* GetChild(TreeModelNode* parent, int index) = 0;

  // Returns the parent of a node, or NULL if node is the root.
  virtual TreeModelNode* GetParent(TreeModelNode* node) = 0;

  // Sets the observer of the model.
  virtual void SetObserver(TreeModelObserver* observer) = 0;

  // Sets the title of the specified node.
  // This is only invoked if the node is editable and the user edits a node.
  virtual void SetTitle(TreeModelNode* node,
                        const std::wstring& title) {
    NOTREACHED();
  }

  // Returns the set of icons for the nodes in the tree. You only need override
  // this if you don't want to use the default folder icons.
  virtual void GetIcons(std::vector<SkBitmap>* icons) {}

  // Returns the index of the icon to use for |node|. Return -1 to use the
  // default icon. The index is relative to the list of icons returned from
  // GetIcons.
  virtual int GetIconIndex(TreeModelNode* node) { return -1; }
};

}  // namespace views

#endif  // CHROME_VIEWS_TREE_MODEL_H_
