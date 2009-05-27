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


// This file contains the declaration of the RenderNode class.

#ifndef O3D_CORE_CROSS_RENDER_NODE_H_
#define O3D_CORE_CROSS_RENDER_NODE_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/draw_context.h"
#include "core/cross/render_context.h"

namespace o3d {

class Effect;
class State;

// The base class for all nodes that live in the render graph It sorts all its
// children by priority (lower priority first) and renders them in that order.
class RenderNode : public ParamObject {
  friend class Client;
 public:
  typedef SmartPointer<RenderNode> Ref;

  typedef std::vector<RenderNode::Ref> RenderNodeRefArray;

  static const char* kPriorityParamName;
  static const char* kActiveParamName;

  virtual ~RenderNode();

  // Renders ourself when called and our children if active.
  virtual void Render(RenderContext* render_context) { }

  // Renders this render node and all children.
  virtual void RenderTree(RenderContext* render_context);

  // Called after render and rendering children.
  virtual void PostRender(RenderContext* render_context) { }

  // Gets the priority.
  float priority() const {
    return priority_param_->value();
  }

  // Sets the priority.
  void set_priority(float value) {
    priority_param_->set_value(value);
  }

  // Gets the draw context.
  bool active() const {
    return active_param_->value();
  }

  // Sets the draw context.
  void set_active(bool value) {
    active_param_->set_value(value);
  }

  // The actual array of children.
  const RenderNodeRefArray& children() const {
    return child_array_;
  }

  // Sets the parent of the render node by re-parenting the node under
  // parent_render_node. Setting parent_render_node to NULL removes the
  // render node and the entire subtree below it from the render graph.
  // The routine will fail if assigning the parent creates a cycle in the
  // parent-child RenderNode graph.
  //
  // NOTE!: When setting the parent to NULL, if results in there being no more
  // references to this render node then this render node will be deleted.
  //
  // Paremeters:
  //  parent_render_node: The new parent for the render node or NULL to
  //                      un-parent the render node.
  void SetParent(RenderNode *parent_render_node);

  // Returns the render node's parent.
  // Parameters:
  //  None.
  // Returns
  //  Pointer to the parent of the rende rnode.
  inline RenderNode* parent() const;

  // Returns the immediate children of the render node.
  RenderNodeArray GetChildren() const;

  // Returns all the rendernodes in a subtree.
  // Returns this rendernode and all its descendants. Note that this rendernode
  // does not have to be in the rendergraph.
  // Parameters:
  //  RenderNodeArray to receive list of nodes. If anything is in
  //                the array it will be cleared.
  void GetRenderNodesInTreeFast(RenderNodeArray* render_nodes) const;

  // Returns all the rendernodes in a subtree.
  // Returns this rendernode and all its descendants. Note that this
  // rendernode does not have to be in the rendergraph.
  //
  // This is a slower version for javascript. If you are using C++ prefer the
  // version above. This one creates the array on the stack and passes it back
  // by value which means the entire array will get copied twice and in making
  // the copies memory is allocated and deallocated twice.
  //
  // Returns:
  //  An array containing pointers to the rendernodes of the subtree.
  RenderNodeArray GetRenderNodesInTree() const;

  // Searches for rendernodes that match the given name in the hierarchy under
  // and including this node. Since there can be more than one rendernode with a
  // given name the results are returned in an array.
  // Parameters:
  //  name: Rendernode name to look for.
  //  render_nodes: RenderNodeArray to receive list of nodes. If anything is in
  //                the array it will be cleared.
  void GetRenderNodesByNameInTreeFast(const String& name,
                                      RenderNodeArray* render_nodes) const;

  // Searches for rendernodes that match the given name in the hierarchy under
  // and including this render node. Since there can be more than one rendernode
  // with a given name, results are returned in an array.
  //
  // This is a slower version for javascript. If you are using C++ prefer the
  // version above. This one creates the array on the stack and passes it back
  // by value which means the entire array will get copied twice and in making
  // the copies memory is allocated and deallocated twice.
  //
  // Parameters:
  //  name: Rendernode name to look for.
  // Returns:
  //  An array of pointers to render nodes matching the given name.
  RenderNodeArray GetRenderNodesByNameInTree(const String& name) const;

  // Searches for render nodes that match the given class in the hierarchy under
  // and including this render node.
  // Parameters:
  //  class_type: class to look for.
  // Returns:
  //  An array of pointers to render nodes matching the given class
  RenderNodeArray GetRenderNodesByClassInTree(
      const ObjectBase::Class* class_type) const;

  // Searches for render nodes that match the given class name in the hierarchy
  // under and including this render node.
  // Parameters:
  //  class_type_name: name of class to look for
  // Returns:
  //  An array of pointers to render nodes matching the given class name
  RenderNodeArray GetRenderNodesByClassNameInTree(
      const String& class_type_name) const;

 protected:
  explicit RenderNode(ServiceLocator* service_locator);

  // Removes a child render node from the rendernode child list. It does not
  // update the parent of child_node. Note this method is protected and not
  // exposed to the external API as it should not be used to change
  // parent-child relationships. The proper way to re-parent a node is to call
  // RenderNode::SetParent().
  virtual bool RemoveChild(RenderNode *child_node);

  // Adds a child render node to the rendernode child list. It does not update
  // the parent of the child. This method is not exposed to the external API and
  // should not be used to change parent-child relationships.  The proper way to
  // re-parent a node is to call RenderNode::SetParent().
  virtual bool AddChild(RenderNode *child_node);

 private:
  // Renders the children of this render node.
  void RenderChildren(RenderContext* render_context);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat::Ref priority_param_;  // For priority sorting.
  ParamBoolean::Ref active_param_;  // Should this RenderNode be processed.

  RenderNodeRefArray  child_array_;  // Array of children.
  RenderNode*         parent_;

  O3D_DECL_CLASS(RenderNode, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(RenderNode);
};

inline RenderNode* RenderNode::parent() const {
  return parent_;
}

typedef RenderNode::RenderNodeRefArray RenderNodeRefArray;

typedef RenderNodeRefArray::const_iterator RenderNodeRefArrayConstIterator;
typedef RenderNodeRefArray::iterator RenderNodeRefArrayIterator;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_RENDER_NODE_H_
