// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/bookmark_bar_bridge.h"
#include "chrome/browser/cocoa/bookmark_bar_controller.h"

BookmarkBarBridge::BookmarkBarBridge(BookmarkBarController* controller,
                                     BookmarkModel* model)
    : controller_(controller), model_(model) {
  model_->AddObserver(this);

  // Bookmark loading is async; it may may not have happened yet.
  // We will be notified when that happens with the AddObserver() call.
  if (model->IsLoaded())
    Loaded(model);
}

BookmarkBarBridge::~BookmarkBarBridge() {
  model_->RemoveObserver(this);
}

void BookmarkBarBridge::Loaded(BookmarkModel* model) {
  [controller_ loaded:model];
}

void BookmarkBarBridge::BookmarkModelBeingDeleted(BookmarkModel* model) {
  [controller_ beingDeleted:model];
}

void BookmarkBarBridge::BookmarkNodeMoved(BookmarkModel* model,
                                          const BookmarkNode* old_parent,
                                          int old_index,
                                          const BookmarkNode* new_parent,
                                          int new_index) {
  [controller_ nodeMoved:model
               oldParent:old_parent oldIndex:old_index
               newParent:new_parent newIndex:new_index];
}

void BookmarkBarBridge::BookmarkNodeAdded(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int index) {
  [controller_ nodeAdded:model parent:parent index:index];
}

void BookmarkBarBridge::BookmarkNodeChanged(BookmarkModel* model,
                                            const BookmarkNode* node) {
  [controller_ nodeChanged:model node:node];
}

void BookmarkBarBridge::BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                                  const BookmarkNode* node) {
  [controller_ nodeFavIconLoaded:model node:node];
}

void BookmarkBarBridge::BookmarkNodeChildrenReordered(
    BookmarkModel* model, const BookmarkNode* node) {
  [controller_ nodeChildrenReordered:model node:node];
}
