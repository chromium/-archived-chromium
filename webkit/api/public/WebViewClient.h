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

#ifndef WebViewClient_h
#define WebViewClient_h

#error "This header file is still a work in progress; do not include!"

#include "WebWidgetClient.h"

namespace WebKit {
    class WebFileChooserCompletion;
    class WebFrame;
    class WebPopupMenu;
    class WebString;
    class WebView;
    struct WebConsoleMessage;
    struct WebContextMenuInfo;
    struct WebPopupMenuInfo;

    class WebViewClient : public WebWidgetClient {
    public:
        // Factory methods -----------------------------------------------------

        // Create a new related WebView.
        virtual WebView* createView(bool hasUserGesture) = 0;

        // Create a new WebPopupMenu.  In the second form, the client is
        // responsible for rendering the contents of the popup menu.
        virtual WebPopupMenu* createPopupMenu(bool activatable) = 0;
        virtual WebPopupMenu* createPopupMenu(bool activatable, const WebPopupMenuInfo&) = 0;


        // Misc ----------------------------------------------------------------

        // A new message was added to the console.
        virtual void didAddMessageToConsole(
            const WebConsoleMessage&, const WebString& sourceName, unsigned sourceLine) = 0;

        // If enabled, sudden termination implies that there are no registered
        // unload event handlers that would need to run in order to close the
        // WebView.  This information allows the embedder to determine if the
        // process can be closed without closing the respective WebViews.
        virtual void enableSuddenTermination() = 0;
        virtual void disableSuddenTermination() = 0;

        // Called when script in the page calls window.print().
        virtual void printPage() = 0;


        // Navigational --------------------------------------------------------

        // These notifications bracket any loading that occurs in the WebView.
        virtual void didStartLoading() = 0;
        virtual void didStopLoading() = 0;

        // A frame (or subframe) was created.  The client may return a
        // WebFrameClient to be associated with the newly created frame.
        virtual WebFrameClient* didCreateFrame(WebFrame* frame) = 0;


        // Editing -------------------------------------------------------------

        // May return null.  The WebEditingClient is passed additional events
        // related to text editing in the page.
        virtual WebEditingClient* editingClient() = 0;

        // The client should perform spell-checking on the given word
        // synchronously.  Return a length of 0 if the word is not misspelled.
        virtual void spellCheck(
            const WebString& word, int& misspelledOffset, int& misspelledLength) = 0;

        // Request the text on the selection clipboard be sent back to the
        // WebView so it can be inserted into the current focus area.  This is
        // only meaningful on platforms that have a selection clipboard (e.g.,
        // X-Windows).
        virtual void pasteFromSelectionClipboard() = 0;


        // Dialogs -------------------------------------------------------------

        // These methods should not return until the dialog has been closed.
        virtual void runModalAlertDialog(const WebString& message) = 0;
        virtual bool runModalConfirmDialog(const WebString& message) = 0;
        virtual bool runModalPromptDialog(
            const WebString& message, const WebString& defaultValue,
            WebString* actualValue) = 0;
        virtual bool runModalBeforeUnloadDialog(const WebString& message) = 0;

        // This method returns immediately after showing the dialog.  When the
        // dialog is closed, it should call the WebFileChooserCompletion to
        // pass the results of the dialog.
        virtual void runFileChooser(
            bool multiSelect, const WebString& title,
            const WebString& initialValue, WebFileChooserCompletion*) = 0;


        // UI ------------------------------------------------------------------

        // Called when script modifies window.status
        virtual void setStatusText(const WebString&) = 0;

        // Called when hovering over an anchor with the given URL.
        virtual void setMouseOverURL(const WebURL&) = 0;

        // Called when a tooltip should be shown at the current cursor position.
        virtual void setToolTipText(const WebString&) = 0;

        // Called when a context menu should be shown at the current cursor position.
        virtual void showContextMenu(const WebContextMenuInfo&) = 0;

        // Called when a drag-n-drop operation should begin.
        virtual void startDragging(WebFrame*, const WebDragData&) = 0;

        // Take focus away from the WebView by focusing an adjacent UI element
        // in the containing window.
        virtual void focusNext() = 0;
        virtual void focusPrevious() = 0;


        // Session History -----------------------------------------------------

        // Returns the history item at the given index.
        virtual WebHistoryItem historyItemAtIndex(int index) = 0;

        // Returns the number of history items before/after the current
        // history item.
        virtual int historyBackListCount() = 0;
        virtual int historyForwardListCount() = 0;

        // Called to notify the embedder when a new history item is added.
        virtual void didAddHistoryItem() = 0;


        // Developer Tools -----------------------------------------------------

        virtual void didOpenInspector(int numResources) = 0;


        // FIXME need to something for:
        // OnPasswordFormsSeen
        // OnAutofillFormSubmitted
        // QueryFormFieldAutofill
        // RemoveStoredAutofillEntry
        // ShowModalHTMLDialog <-- we should be able to kill this
        // GetWebDevToolsAgentDelegate
        // WasOpenedByUserGesture
    };

} // namespace WebKit

#endif
