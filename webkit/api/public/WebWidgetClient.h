/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebWidgetClient_h
#define WebWidgetClient_h

#error "This header file is still a work in progress; do not include!"

#include "WebCommon.h"
#include "WebNavigationPolicy.h"

namespace WebKit {
    class WebWidget;
    struct WebCursorInfo;
    struct WebPluginGeometry;
    struct WebRect;
    struct WebScreenInfo;

    class WebWidgetClient {
    public:
        // Called when a region of the WebWidget needs to be re-painted.
        virtual void didInvalidateRect(WebWidget*, const WebRect&) = 0;

        // Called when a region of the WebWidget, given by clipRect, should be
        // scrolled by the specified dx and dy amounts.
        virtual void didScrollRect(WebWidget*, int dx, int dy, const WebRect& clipRect) = 0;

        // Called when a plugin is moved relative to its containing window.
        // This typically happens as a result of scrolling the page.
        virtual void didMovePlugin(WebWidget*, const WebPluginGeometry&) = 0;

        // Called when the widget acquires or loses focus, respectively.
        virtual void didFocus(WebWidget*) = 0;
        virtual void didBlur(WebWidget*) = 0;

        // Called when the cursor for the widget changes.
        virtual void didChangeCursor(WebWidget*, const WebCursorInfo&) = 0;

        // Called when the widget should be closed.  WebWidget::close() should
        // be called asynchronously as a result of this notification.
        virtual void closeWidgetSoon(WebWidget*) = 0;

        // Called to show the widget according to the given policy.
        virtual void show(WebWidget*, WebNavigationPolicy) = 0;

        // Called to block execution of the current thread until the widget is
        // closed.
        virtual void runModal(WebWidget*) = 0;

        // Called to get/set the position of the widget in screen coordinates.
        virtual WebRect windowRect(WebWidget*) = 0;
        virtual void setWindowRect(WebWidget*, const WebRect&) = 0;

        // Called to get the position of the resizer rect in window coordinates.
        virtual WebRect windowResizerRect(WebWidget*) = 0;

        // Called to get the position of the root window containing the widget
        // in screen coordinates.
        virtual WebRect rootWindowRect(WebWidget*) = 0;

        // Called to query information about the screen where this widget is
        // displayed.
        virtual WebScreenInfo screenInfo(WebWidget*) = 0;
    };

} // namespace WebKit

#endif
