// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_VIEW_CONSTANTS_H_
#define VIEWS_VIEW_CONSTANTS_H_

namespace views {

// Size (width or height) within which the user can hold the mouse and the
// view should scroll.
extern const int kAutoscrollSize;

// Time in milliseconds to autoscroll by a row. This is used during drag and
// drop.
extern const int kAutoscrollRowTimerMS;

// Used to determine whether a drop is on an item or before/after it. If a drop
// occurs kDropBetweenPixels from the top/bottom it is considered before/after
// the item, otherwise it is on the item.
extern const int kDropBetweenPixels;

}  // namespace views

#endif  // VIEWS_VIEW_CONSTANTS_H_
