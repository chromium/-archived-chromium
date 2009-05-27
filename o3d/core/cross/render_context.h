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


// This file contains the declaration of the RenderContext class.

#ifndef O3D_CORE_CROSS_RENDER_CONTEXT_H_
#define O3D_CORE_CROSS_RENDER_CONTEXT_H_

#include <vector>

namespace o3d {

class RenderNode;
class Renderer;

// Array container for RenderNodes.
typedef std::vector<RenderNode*> RenderNodeArray;

// Iterators for RenderNodeArray
typedef RenderNodeArray::const_iterator RenderNodeArrayConstIterator;
typedef RenderNodeArray::iterator RenderNodeArrayIterator;

// The RenderContext class passed down the render graph
// as the render graph is walked to hold state information
// and other context needed while walking the render graph.
class RenderContext {
 public:
  explicit RenderContext(Renderer* renderer);

  // Adds a Render Node to the current render list.
  void AddToRenderList(RenderNode* render_node);

  // Sets the renderlist that will be used when AddToRenderList is called.
  void set_render_list(RenderNodeArray* render_list);

  Renderer* renderer();

 private:
  // The current list used by AddToRenderList.
  RenderNodeArray* render_list_;
  Renderer*        renderer_;
};

inline Renderer* RenderContext::renderer() {
  return renderer_;
}

inline void RenderContext::AddToRenderList(RenderNode* render_node) {
  if (render_list_) {
    render_list_->push_back(render_node);
  }
}

inline void RenderContext::set_render_list(RenderNodeArray* render_list) {
  render_list_ = render_list;
}

}  // namespace o3d

#endif  // O3D_CORE_CROSS_RENDER_CONTEXT_H_




