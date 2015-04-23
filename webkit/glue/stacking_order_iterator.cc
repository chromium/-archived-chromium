// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "RenderLayer.h"
#include "RenderObject.h"
MSVC_POP_WARNING();
#undef LOG

#include "webkit/glue/stacking_order_iterator.h"

//
// RenderLayerIterator
//

RenderLayerIterator::RenderLayerIterator() {
}

void RenderLayerIterator::Reset(const WebCore::IntRect& bounds,
                                WebCore::RenderLayer* rl) {
  bounds_ = bounds;
  root_layer_ = rl;
  if (rl) {
    context_stack_.push_back(Context(rl));
  }
}

WebCore::RenderLayer* RenderLayerIterator::Next() {
  while (context_stack_.size()) {
    Context* ctx = &(context_stack_.back());
    if (!ctx->layer()->boundingBox(root_layer_).intersects(bounds_)) {
      // Doesn't overlap bounds; skip this layer.
      context_stack_.pop_back();
    } else if (ctx->HasMoreNeg()) {
      context_stack_.push_back(ctx->NextNeg());
    } else if (ctx->HasSelf()) {
      // Emit self.
      return ctx->NextSelf();
    } else if (ctx->HasMoreOverflow()) {
      context_stack_.push_back(ctx->NextOverflow());
    } else if (ctx->HasMorePos()) {
      context_stack_.push_back(ctx->NextPos());
    } else {
      // Nothing left in this context.  Pop.
      context_stack_.pop_back();
    }
  }
  return NULL;
}

RenderLayerIterator::Context::Context(WebCore::RenderLayer* layer)
   :  layer_(layer),
      next_neg_(0),
      next_self_(0),
      next_overflow_(0),
      next_pos_(0) {
  ASSERT(layer_);
  layer_->updateZOrderLists();
  layer_->updateNormalFlowList();
}

bool RenderLayerIterator::Context::HasMoreNeg() {
  return layer_->negZOrderList() &&
    next_neg_ < layer_->negZOrderList()->size();
}

RenderLayerIterator::Context RenderLayerIterator::Context::NextNeg() {
  ASSERT(HasMoreNeg());
  return Context(layer_->negZOrderList()->at(next_neg_++));
}

bool RenderLayerIterator::Context::HasSelf() {
  return next_self_ < 1;
}

WebCore::RenderLayer* RenderLayerIterator::Context::NextSelf() {
  ASSERT(HasSelf());
  next_self_ = 1;
  return layer_;
}

bool RenderLayerIterator::Context::HasMoreOverflow() {
  return layer_->normalFlowList() &&
    next_overflow_ >= 0 &&
    next_overflow_ < layer_->normalFlowList()->size();
}

RenderLayerIterator::Context RenderLayerIterator::Context::NextOverflow() {
  ASSERT(HasMoreOverflow());
  return Context(layer_->normalFlowList()->at(next_overflow_++));
}

bool RenderLayerIterator::Context::HasMorePos() {
  return layer_->posZOrderList() &&
    next_pos_ < layer_->posZOrderList()->size();
}

RenderLayerIterator::Context RenderLayerIterator::Context::NextPos() {
  ASSERT(HasMorePos());
  return Context(layer_->posZOrderList()->at(next_pos_++));
}

//
// StackingOrderIterator
//

StackingOrderIterator::StackingOrderIterator() {
  Reset(WebCore::IntRect(0, 0, 0, 0), NULL);
}

void StackingOrderIterator::Reset(const WebCore::IntRect& bounds,
                                  WebCore::RenderLayer* rl) {
  layer_iterator_.Reset(bounds, rl);
  current_object_ = NULL;
  current_layer_root_ = NULL;
}

WebCore::RenderObject* StackingOrderIterator::Next() {
  if (current_object_) {
    // Get the next object inside the current layer.
    current_object_ = current_object_->nextInPreOrder(current_layer_root_);

    // Skip any sub-layers we encounter along the way; they are
    // visited (in the correct stacking order) by layer_iterator_.
    while (current_object_ && current_object_->hasLayer()) {
      current_object_ = current_object_->
                        nextInPreOrderAfterChildren(current_layer_root_);
    }
  }

  if (!current_object_) {
    // Start the next layer.
    WebCore::RenderLayer* layer = layer_iterator_.Next();
    if (layer) {
      current_object_ = layer->renderer();
      current_layer_root_ = current_object_;
    }
  }
  return current_object_;
}
