/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "core/cross/precompile.h"
#include "core/cross/render_node.h"
#include "core/cross/error.h"

namespace o3d {

namespace {

// Returns true if parenting parent to child introduces a cycle in the
// rendergraph hierarchy.
// Note:  Assumes that the graph is acyclic.  Will infinite loop if cycles
// are present.
bool ParentingIntroducesCycle(
    const RenderNode* parent,
    const RenderNode* child) {
  if (child == parent)
    return true;

  // Ensure that the parent node is not reachable from any of the descendents
  // of the child node.
  const RenderNodeArray& children(child->GetChildren());
  RenderNodeArray::const_iterator iter(children.begin()), end(children.end());
  for (; iter != end; ++iter) {
    if (ParentingIntroducesCycle(parent, *iter)) {
      return true;
    }
  }

  return false;
}

bool CompareByPriority(const RenderNode* lhs, const RenderNode* rhs) {
  return lhs->priority() < rhs->priority();
}

}  // end unnamed namespace

O3D_DEFN_CLASS(RenderNode, ParamObject);

const char* RenderNode::kPriorityParamName =
    O3D_STRING_CONSTANT("priority");
const char* RenderNode::kActiveParamName =
    O3D_STRING_CONSTANT("active");

RenderNode::RenderNode(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      parent_(NULL) {
  RegisterParamRef(kPriorityParamName, &priority_param_);
  RegisterParamRef(kActiveParamName, &active_param_);

  priority_param_->set_value(0.0f);
  active_param_->set_value(true);
}

RenderNode::~RenderNode() {
  // Sets any children to have no parent.
  //
  // We can not use child_array_ directly because as each child has
  // SetParent(NULL) called on it it will remove itself from child_array_.
  RenderNodeArray children(GetChildren());
  RenderNodeArray::iterator end(children.end());
  for (RenderNodeArray::iterator iter(children.begin());
       iter != end;
       ++iter) {
    (*iter)->SetParent(NULL);
  }
}

void RenderNode::RenderTree(RenderContext* render_context) {
  if (active()) {
    Render(render_context);
    if (!child_array_.empty()) {
      RenderChildren(render_context);
    }
    PostRender(render_context);
  }
}

void RenderNode::RenderChildren(RenderContext* render_context) {
  std::sort(child_array_.begin(), child_array_.end(), CompareByPriority);
  RenderNodeRefArray::const_iterator child_end(child_array_.end());
  for (RenderNodeRefArray::const_iterator child_iter(child_array_.begin());
       child_iter != child_end;
       ++child_iter) {
    RenderNode* child = child_iter->Get();
    child->RenderTree(render_context);
  }
}

// Returns a array of immediate children for the rendernode.
RenderNodeArray RenderNode::GetChildren() const {
  RenderNodeArray children_array;
  children_array.reserve(child_array_.size());
  RenderNodeRefArray::const_iterator child_end(child_array_.end());
  for (RenderNodeRefArray::const_iterator child_iter(child_array_.begin());
       child_iter != child_end;
       ++child_iter) {
    children_array.push_back(child_iter->Get());
  }

  return children_array;
}

void RenderNode::SetParent(RenderNode* parent_render_node) {
  // Explicitly disallow parenting that generates a cycle.
  if (ParentingIntroducesCycle(parent_render_node, this)) {
    O3D_ERROR(service_locator())
        << "Cannot set parent as it creates a cycle";
    return;
  }

  // Creates a temporary reference to ourselves because if our current parent
  // holds the only reference to us then we'll get deleted the moment we call
  // RemoveChild. This temporary reference will let go automatically when
  // the function exits.
  RenderNode::Ref temp_reference(this);

  // Checks if the rendernode already has a parent.  If it does then remove it
  // from its current parent first.
  if (parent_ != NULL) {
    bool removed = parent_->RemoveChild(this);

    DLOG_ASSERT(removed);

    if (!removed)
      return;
  }

  // If we are just unparenting the node then we are done.
  if (parent_render_node == NULL) {
    parent_ = NULL;
    return;
  }

  // Adds the rendernode as a child of its new parent.
  parent_ = parent_render_node;

  bool added = parent_render_node->AddChild(this);

  DLOG_ASSERT(added);

  // If we failed to add the child to the parent then we leave the child node
  // an orphan in order to avoid any inconsistencies in the rendergraph.
  if (!added)
    parent_ = NULL;
}

// Removes a child node from the child_array_.  Returns true if the operation
// succeeds.
bool RenderNode::RemoveChild(RenderNode *child_node) {
  RenderNodeRefArrayIterator end = std::remove(child_array_.begin(),
                                               child_array_.end(),
                                               RenderNode::Ref(child_node));

  // Child_node should never be in the child array more than once.
  DLOG_ASSERT(child_array_.end() - end <= 1);

  // The child was never found.
  if (end == child_array_.end())
    return true;

  // This test will tell us if the child_node was found.
  if (child_array_.end() - end != 1)
    return false;

  // Actually removes the child_node from the child array.
  child_array_.erase(end, child_array_.end());
  return true;
}

bool RenderNode::AddChild(RenderNode *child_node) {
  child_array_.push_back(RenderNode::Ref(child_node));
  return true;
};

// Returns the rendernodes in a subtree starting with this rendernode
void RenderNode::GetRenderNodesInTreeFast(RenderNodeArray* render_nodes) const {
  render_nodes->clear();

  // First push this rendernode into the array.
  render_nodes->push_back(const_cast<RenderNode*>(this));

  // Iterate through all the elements of the array and append all the child
  // rendernodes at the end.
  for (unsigned int index = 0;index < render_nodes->size();index++) {
    RenderNode* render_node = (*render_nodes)[index];
    copy(render_node->child_array_.begin(),
         render_node->child_array_.end(),
         back_inserter(*render_nodes));
  }
}

// Slower version of function above for Javascript.
RenderNodeArray RenderNode::GetRenderNodesInTree() const {
  RenderNodeArray render_node_array;
  GetRenderNodesInTreeFast(&render_node_array);
  return render_node_array;
};

// Functor object for comparing render node names.
namespace {
class CompareRenderNodeName {
 public:
  explicit CompareRenderNodeName(const String& n) : test_name_(n) {}
  bool operator()(RenderNode*& render_node) {
    return render_node->name() == test_name_;
  }
 private:
  String test_name_;
};
}

// Search for render nodes by name in the subtree below this rendernode.
void RenderNode::GetRenderNodesByNameInTreeFast(
    const String& name,
    RenderNodeArray* matching_nodes) const {
  matching_nodes->clear();

  // Get all the render nodes in the subtree.
  GetRenderNodesInTreeFast(matching_nodes);

  // Partition the container into two sections depending on whether the render
  // node name matches the desired name.
  RenderNodeArray::iterator i =
      std::partition(matching_nodes->begin(),
                     matching_nodes->end(),
                     CompareRenderNodeName(name));
}

// Slower version of function above for Javascript.
RenderNodeArray RenderNode::GetRenderNodesByNameInTree(
    const String& name) const {
  RenderNodeArray matching_nodes;
  GetRenderNodesByNameInTreeFast(name, &matching_nodes);
  return matching_nodes;
}

// Functor object for comparing render node classes.
namespace {
class CompareRenderNodeClass {
 public:
  explicit CompareRenderNodeClass(const ObjectBase::Class* class_type)
      : class_type_(class_type) {
  }
  bool operator()(RenderNode*& render_node) {
    return render_node->IsA(class_type_);
  }
 private:
  const ObjectBase::Class* class_type_;
};
}

// Search for rendernodes by class in the subtree below this rendernode
RenderNodeArray RenderNode::GetRenderNodesByClassInTree(
    const ObjectBase::Class* class_type) const {
  // Get all the render nodes in the subtree
  RenderNodeArray nodes = GetRenderNodesInTree();
  // Partition the container into two sections depending on whether the render
  // node matches the desired class.
  RenderNodeArray::iterator i =
      std::partition(nodes.begin(),
                     nodes.end(),
                     CompareRenderNodeClass(class_type));
  // delete the nodes that don't match the name.
  nodes.erase(i, nodes.end());
  return nodes;
}

// Functor object for comparing render node class names.
namespace {
class CompareRenderNodeClassName {
 public:
  explicit CompareRenderNodeClassName(const String& class_type_name)
      : class_type_name_(class_type_name) {
  }
  bool operator()(RenderNode*& render_node) {
    return render_node->IsAClassName(class_type_name_);
  }
 private:
  const String& class_type_name_;
};
}

// Search for render nodes by class name in the subtree below this render node
RenderNodeArray RenderNode::GetRenderNodesByClassNameInTree(
    const String& class_type_name) const {
  // Get all the render nodes in the subtree
  RenderNodeArray nodes = GetRenderNodesInTree();
  // Partition the container into two sections depending on whether the render
  // node class name matches the desired class name.
  RenderNodeArray::iterator i =
      std::partition(nodes.begin(),
                     nodes.end(),
                     CompareRenderNodeClassName(class_type_name));
  // delete the nodes that don't match the name.
  nodes.erase(i, nodes.end());
  return nodes;
}

ObjectBase::Ref RenderNode::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new RenderNode(service_locator));
}

}  // namespace o3d
