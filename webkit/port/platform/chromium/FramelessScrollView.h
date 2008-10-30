// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FramelessScrollView_h
#define FramelessScrollView_h

#include "ScrollView.h"

namespace WebCore {
    class FramelessScrollViewClient;
    class PlatformKeyboardEvent;
    class PlatformMouseEvent;
    class PlatformWheelEvent;

    // A FramelessScrollView is a ScrollView that can be used to render custom
    // content, which does not have an associated Frame.
    //
    // TODO: It may be better to just develop a custom subclass of Widget that
    // can have scroll bars for this instead of trying to reuse ScrollView.
    //
    class FramelessScrollView : public ScrollView {
    public:
        FramelessScrollView() : m_client(0) {}
        ~FramelessScrollView();

        FramelessScrollViewClient* client() const { return m_client; }
        void setClient(FramelessScrollViewClient* client) { m_client = client; }

        // Event handlers that subclasses must implement.
        virtual bool handleMouseDownEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleMouseMoveEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleMouseReleaseEvent(const PlatformMouseEvent&) = 0;
        virtual bool handleWheelEvent(const PlatformWheelEvent&) = 0;
        virtual bool handleKeyEvent(const PlatformKeyboardEvent&) = 0;

        // ScrollbarClient public methods:
        virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
        virtual bool isActive() const;

        // Widget public methods:
        virtual void invalidateRect(const IntRect&);

        // ScrollView public methods:
        virtual HostWindow* hostWindow() const;
        virtual IntRect windowClipRect(bool clipToContents = true) const;

    protected:
        // ScrollView protected methods:
        virtual void paintContents(GraphicsContext*, const IntRect& damageRect);
        virtual void contentsResized();
        virtual void visibleContentsResized();

    private:
        FramelessScrollViewClient* m_client;
    };
}

#endif // FramelessScrollView_h
