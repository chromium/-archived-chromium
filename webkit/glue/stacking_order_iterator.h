// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides some utilities for iterating over a RenderObject graph in
// stacking order.

#ifndef WEBKIT_GLUE_STACKING_ORDER_ITERATOR_H__
#define WEBKIT_GLUE_STACKING_ORDER_ITERATOR_H__

#include <vector>
#include "IntRect.h"

namespace WebCore {
class RenderLayer;
class RenderObject;
}

// Iterates over a subtree of RenderLayers in stacking order, back to
// front.  Modifying the RenderObject graph invalidates this iterator.
//
// TODO(tulrich): this could go in webkit.
// TODO(tulrich): needs unittests.
class RenderLayerIterator {
 public:
  RenderLayerIterator();

  // Sets the RenderLayer subtree to iterate over, and the bounding
  // box we are interested in.  The bounds coordinates are relative to
  // the given layer.
  void Reset(const WebCore::IntRect& bounds, WebCore::RenderLayer* rl);

  // Returns the next RenderLayer in stacking order, back to front.
  WebCore::RenderLayer* Next();
 private:
  class Context {
   public:
    Context(WebCore::RenderLayer* layer);

    bool HasMoreNeg();
    Context NextNeg();
    bool HasSelf();
    WebCore::RenderLayer* NextSelf();
    bool HasMoreOverflow();
    Context NextOverflow();
    bool HasMorePos();
    Context NextPos();

    WebCore::RenderLayer* layer() const {
      return layer_;
    }

   private:
    WebCore::RenderLayer* layer_;
    size_t next_neg_;
    size_t next_self_;
    size_t next_overflow_;
    size_t next_pos_;
  };

  WebCore::IntRect bounds_;
  const WebCore::RenderLayer* root_layer_;
  std::vector<Context> context_stack_;
};

// Iterates over a subtree of RenderObjects below a given RenderLayer.
//
// TODO(tulrich): this could go in webkit.
// TODO(tulrich): needs unittests.
class StackingOrderIterator {
 public:
  StackingOrderIterator();
  void Reset(const WebCore::IntRect& bounds, WebCore::RenderLayer* rl);
  WebCore::RenderObject* Next();

 private:
  RenderLayerIterator layer_iterator_;
  WebCore::RenderObject* current_object_;
  WebCore::RenderObject* current_layer_root_;
};

#endif  // WEBKIT_GLUE_STACKING_ORDER_ITERATOR_H__
