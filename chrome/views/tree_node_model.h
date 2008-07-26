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

#ifndef CHROME_VIEWS_TREE_NODE_MODEL_H__
#define CHROME_VIEWS_TREE_NODE_MODEL_H__

#include <vector>

#include "base/basictypes.h"
#include "chrome/common/scoped_vector.h"
#include "chrome/views/tree_view.h"

namespace ChromeViews {

// TreeNodeModel and TreeNodes provide an implementation of TreeModel around
// TreeNodes. TreeNodes form a directed acyclic graph.
//
// TreeNodes own their children, so that deleting a node deletes all
// descendants.
//
// TreeNodes do NOT maintain a pointer back to the model. As such, if you
// are using TreeNodes with a TreeNodeModel you will need to notify the observer
// yourself any time you make any change directly to the TreeNodes. For example,
// if you directly invoke SetTitle on a node it does not notify the
// observer, you will need to do it yourself. This includes the following
// methods: SetTitle, Remove and Add. TreeNodeModel provides cover
// methods that mutate the TreeNodes and notify the observer. If you are using
// TreeNodes with a TreeNodeModel use the cover methods to save yourself the
// headache.
//
// The following example creates a TreeNode with two children and then
// creates a TreeNodeModel from it:
//
// TreeNodeWithValue<int> root = new TreeNodeWithValue<int>(0, L"root");
// root.add(new TreeNodeWithValue<int>(1, L"child 1"));
// root.add(new TreeNodeWithValue<int>(1, L"child 2"));
// TreeNodeModel<TreeNodeWithValue<int>>* model =
//     new TreeNodeModel<TreeNodeWithValue<int>>(root);
//
// Two variants of TreeNode are provided here:
//
// . TreeNode itself is intended for subclassing. It has one type parameter
//   that corresponds to the type of the node. When subclassing use your class
//   name as the type parameter, eg:
//   class MyTreeNode : public TreeNode<MyTreeNode> .
// . TreeNodeWithValue is a trivial subclass of TreeNode that has one type
//   type parameter: a value type that is associated with the node.
//
// Which you use depends upon the situation. If you want to subclass and add
// methods, then use TreeNode. If you don't need any extra methods and just
// want to associate a value with each node, then use TreeNodeWithValue.
//
// Regardless of which TreeNode you use, if you are using the nodes with a
// TreeView take care to notify the observer when mutating the nodes.

template <class NodeType>
class TreeNodeModel;

// TreeNode -------------------------------------------------------------------

template <class NodeType>
class TreeNode : public TreeModelNode {
 public:
  TreeNode() : parent_(NULL) { }

  explicit TreeNode(const std::wstring& title)
      : title_(title), parent_(NULL) {}

  virtual ~TreeNode() {
  }

  // Adds the specified child node.
  virtual void Add(int index, NodeType* child) {
    DCHECK(child && index >= 0 && index <= GetChildCount());
    // If the node has a parent, remove it from its parent.
    NodeType* node_parent = child->GetParent();
    if (node_parent)
      node_parent->Remove(node_parent->IndexOfChild(child));
    child->parent_ = static_cast<NodeType*>(this);
    children_->insert(children_->begin() + index, child);
  }

  // Removes the node by index. This does NOT delete the specified node, it is
  // up to the caller to delete it when done.
  virtual NodeType* Remove(int index) {
    DCHECK(index >= 0 && index < GetChildCount());
    NodeType* node = GetChild(index);
    node->parent_ = NULL;
    children_->erase(index + children_->begin());
    return node;
  }

  // Returns the children.
  std::vector<NodeType*> GetChildren() {
    return children_->v;
  }

  // Returns the number of children.
  int GetChildCount() {
    return static_cast<int>(children_->size());
  }

  // Returns a child by index.
  NodeType* GetChild(int index) {
    DCHECK(index >= 0 && index < GetChildCount());
    return children_[index];
  }

  // Returns the parent.
  NodeType* GetParent() {
    return parent_;
  }

  // Returns the index of the specified child, or -1 if node is a not a child.
  int IndexOfChild(const NodeType* node) {
    DCHECK(node);
    std::vector<NodeType*>::iterator i =
        find(children_->begin(), children_->end(), node);
    if (i != children_->end())
      return static_cast<int>(i - children_->begin());
    return -1;
  }

  // Sets the title of the node.
  void SetTitle(const std::wstring& string) {
    title_ = string;
  }

  // Returns the title of the node.
  std::wstring GetTitle() {
    return title_;
  }

  // Returns true if this is the root.
  bool IsRoot() { return (parent_ == NULL); }

  // Returns true if this == ancestor, or one of this nodes parents is
  // ancestor.
  bool HasAncestor(NodeType* ancestor) const {
    if (ancestor == this)
      return true;
    if (!ancestor)
      return false;
    return parent_ ? parent_->HasAncestor(ancestor) : false;
  }

 private:
  // Title displayed in the tree.
  std::wstring title_;

  NodeType* parent_;

  // Children.
  ScopedVector<NodeType> children_;

  DISALLOW_EVIL_CONSTRUCTORS(TreeNode);
};

// TreeNodeWithValue ----------------------------------------------------------

template <class ValueType>
class TreeNodeWithValue : public TreeNode<TreeNodeWithValue<ValueType>> {
 public:
  TreeNodeWithValue() { }

  TreeNodeWithValue(const ValueType& value)
      : TreeNode(std::wstring()), value(value) { }

  TreeNodeWithValue(const std::wstring& title, const ValueType& value)
      : TreeNode(title), value(value) { }

  ValueType value;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TreeNodeWithValue);
};

// TreeNodeModel --------------------------------------------------------------

// TreeModel implementation intended to be used with TreeNodes.
template <class NodeType>
class TreeNodeModel : public TreeModel {
 public:
  // Creates a TreeNodeModel with the specified root node. The root is owned
  // by the TreeNodeModel.
  explicit TreeNodeModel(NodeType* root)
      : observer_(NULL),
        root_(root) {
  }

  virtual ~TreeNodeModel() {}

  virtual void SetObserver(TreeModelObserver* observer) {
    observer_ = observer;
  }

  TreeModelObserver* GetObserver() {
    return observer_;
  }

  // TreeModel methods, all forward to the nodes.
  virtual NodeType* GetRoot() { return root_.get(); }

  virtual int GetChildCount(TreeModelNode* parent) {
    DCHECK(parent);
    return AsNode(parent)->GetChildCount();
  }

  virtual NodeType* GetChild(TreeModelNode* parent, int index) {
    DCHECK(parent);
    return AsNode(parent)->GetChild(index);
  }

  virtual TreeModelNode* GetParent(TreeModelNode* node) {
    DCHECK(node);
    return AsNode(node)->GetParent();
  }

  NodeType* AsNode(TreeModelNode* model_node) {
    return reinterpret_cast<NodeType*>(model_node);
  }

  // Sets the title of the specified node.
  virtual void SetTitle(TreeModelNode* node,
                        const std::wstring& title) {
    DCHECK(node);
    AsNode(node)->SetTitle(title);
    NotifyObserverTreeNodeChanged(node);
  }

  void Add(NodeType* parent, int index, NodeType* child) {
    DCHECK(parent && child);
    parent->Add(index, child);
    NotifyObserverTreeNodesAdded(parent, index, 1);
  }

  NodeType* Remove(NodeType* parent, int index) {
    DCHECK(parent);
    parent->Remove(index);
    NotifyObserverTreeNodesRemoved(parent, index, 1);
  }

  void NotifyObserverTreeNodesAdded(NodeType* parent, int start, int count) {
    if (observer_)
      observer_->TreeNodesAdded(this, parent, start, count);
  }

  void NotifyObserverTreeNodesRemoved(NodeType* parent, int start, int count) {
    if (observer_)
      observer_->TreeNodesRemoved(this, parent, start, count);
  }

  virtual void NotifyObserverTreeNodeChanged(TreeModelNode* node) {
    if (observer_)
      observer_->TreeNodeChanged(this, node);
  }

 private:
  // The root.
  scoped_ptr<NodeType> root_;

  // The observer.
  TreeModelObserver* observer_;

  DISALLOW_EVIL_CONSTRUCTORS(TreeNodeModel);
};

} // namespace

#endif  // CHROME_VIEWS_TREE_NODE_MODEL_H__
