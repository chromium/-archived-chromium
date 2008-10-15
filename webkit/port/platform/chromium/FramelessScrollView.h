// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FramelessScrollView_h
#define FramelessScrollView_h

#include "FrameView.h"

namespace WebCore {
    
    class PlatformKeyboardEvent;

    // A FramelessScrollView is a ScrollView that can be used to render custom
    // content, which does not have an associated Frame.  This extends from
    // FrameView because much of WebCore unfortunately assumes that a
    // ScrollView is a FrameView.
    //
    // TODO: It may be better to just develop a custom subclass of Widget that
    // can have scroll bars for this instead of trying to reuse ScrollView.
    //
    class FramelessScrollView : public FrameView {
    public:
        FramelessScrollView() : FrameView(0) {}

        virtual IntRect windowClipRect() const;

        // Event handlers
        virtual bool handleMouseDownEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleMouseMoveEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleWheelEvent(const PlatformWheelEvent&) = 0;
        virtual bool handleKeyEvent(const PlatformKeyboardEvent&) = 0;
    };
}

#endif // FramelessScrollView_h

