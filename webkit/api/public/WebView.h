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

#ifndef WebView_h
#define WebView_h

#error "This header file is still a work in progress; do not include!"

#include "WebCommon.h"
#include "WebWidget.h"

namespace WebKit {
    class WebPreferences;
    class WebString;
    class WebViewClient;
    struct WebPoint;

    class WebView : public WebWidget {
    public:
        WEBKIT_API WebView* create(WebViewClient*);


        // Preferences ---------------------------------------------------------

        virtual WebPreferences* preferences() = 0;

        // Corresponds to the encoding of the main frame.  Setting the page
        // encoding may cause the main frame to reload.
        virtual WebString pageEncoding() const = 0;
        virtual void setPageEncoding(const WebString&) = 0;


        // Closing -------------------------------------------------------------

        // Returns false if any handler suppressed unloading.
        virtual bool dispatchBeforeUnloadEvent() = 0;

        virtual void dispatchUnloadEvent() = 0;


        // Frames --------------------------------------------------------------

        virtual WebFrame* mainFrame() = 0;

        // Returns the frame identified by the given name.  This method
        // supports pseudo-names like _self, _top, and _blank.  It traverses
        // the entire frame tree containing this tree looking for a frame that
        // matches the given name.
        virtual WebFrame* findFrameByName(const WebString& name) = 0;


        // Focus ---------------------------------------------------------------

        virtual WebFrame* focusedFrame() = 0;
        virtual void setFocusedFrame(WebFrame*) = 0;

        // Restores focus to the previously focused frame and element.  This
        // method is invoked when the WebView is shown after being hidden, and
        // focus is to be restored.  When a WebView loses focus, it remembers
        // the frame and element that had focus.
        virtual void restoreFocus() = 0;

        // Focus the first (last if reverse is true) focusable node.
        virtual void setInitialFocus(bool reverse) = 0;

        // Clears the focused node (and selection if a text field is focused)
        // to ensure that a text field on the page is not eating keystrokes we
        // send it.
        virtual void clearFocusedNode() = 0;


        // Capture -------------------------------------------------------------

        // Fills the contents of this WebView's frames into the given string.
        // If the text is longer than maxCharacters, it will be clipped to that
        // length.  Warning: this function may be slow depending on the number
        // of characters retrieved and page complexity.  For a typically sized
        // page, expect it to take on the order of milliseconds.
        //
        // If there is room, subframe text will be recursively appended.  Each
        // frame will be separated by an empty line.
        virtual WebString captureAsText(unsigned maxCharacters) = 0;


        // Zoom ----------------------------------------------------------------

        // Change the text zoom level.  It will make the zoom level 20% larger
        // or smaller.  If textOnly is set, the text size will be changed.
        // When unset, the entire page's zoom factor will be changed.
        //
        // You can only have either text zoom or full page zoom at one time.
        // Changing the mode will change things in weird ways.  Generally the
        // app should only support text zoom or full page zoom, and not both.
        //
        // zoomDefault will reset both full page and text zoom.
        virtual void zoomIn(bool textOnly) = 0;
        virtual void zoomOut(bool textOnly) = 0;
        virtual void zoomDefault() = 0;


        // Data exchange -------------------------------------------------------

        // Copy to the clipboard the image located at a particular point in the
        // WebView (if there is such an image)
        virtual void copyImageAt(const WebPoint&) = 0;

        // Notifies the WebView that a drag has terminated.
        virtual void dragSourceEndedAt(const WebPoint& clientPoint,
                                       const WebPoint& screenPoint) = 0;

        // Notifies the WebView that a drag and drop operation is in progress, with
        // dropable items over the view.
        virtual void dragSourceMovedTo(const WebPoint& clientPoint,
                                       const WebPoint& screenPoint) = 0;

        // Notfies the WebView that the system drag and drop operation has ended.
        virtual void dragSourceSystemDragEnded() = 0;

        // Callback methods when a drag-and-drop operation is trying to drop
        // something on the WebView.
        virtual bool dragTargetDragEnter(const WebDragData&, int identity,
                                         const WebPoint& clientPoint,
                                         const WebPoint& screenPoint) = 0;
        virtual bool dragTargetDragOver(const WebPoint& clientPoint,
                                        const WebPoint& screenPoint) = 0;
        virtual void dragTargetDragLeave() = 0;
        virtual void dragTargetDrop(const WebPoint& clientPoint,
                                    const WebPoint& screenPoint) = 0;

        virtual int dragIdentity() = 0;


        // Developer tools -----------------------------------------------------

        virtual WebDevToolsAgent* devToolsAgent() = 0;

        // Inspect a particular point in the WebView.  (x = -1 || y = -1) is a
        // special case, meaning inspect the current page and not a specific
        // point.
        virtual void inspectElementAt(const WebPoint&) = 0;


        // FIXME what about:
        // StoreFocusForFrame
        // DownloadImage
        // Get/SetDelegate
        // InsertText -> should move to WebTextInput
        // AutofillSuggestionsForNode
        // HideAutofillPopup
    };

} // namespace WebKit
