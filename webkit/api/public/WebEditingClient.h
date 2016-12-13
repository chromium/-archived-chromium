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

#ifndef WebEditingClient_h
#define WebEditingClient_h

#error "This header file is still a work in progress; do not include!"

namespace WebKit {
    class WebNode;
    class WebRange;
    class WebView;

    class WebEditingClient {
    public:
        // The following methods allow the client to intercept and overrule
        // editing operations.
        virtual bool shouldBeginEditing(WebView*, const WebRange&) = 0;
        virtual bool shouldEndEditing(WebView*, const WebRange&) = 0;
        virtual bool shouldInsertNode(
            WebView*, const WebNode&, const WebRange&, const WebEditingAction&) = 0;
        virtual bool shouldInsertText(
            WebView*, const WebString&, const WebRange&, const WebEditingAction&) = 0;
        virtual bool shouldChangeSelectedRange(
            WebView*, const WebRange& from, const WebRange& to, const WebTextAffinity&,
            bool stillSelecting) = 0;
        virtual bool shouldDeleteRange(WebView*, const WebRange&) = 0;
        virtual bool shouldApplyStyle(WebView*, const WebString& style, const WebRange&) = 0;

        virtual bool isSmartInsertDeleteEnabled(WebView*) = 0;
        virtual bool isSelectTrailingWhitespaceEnabled(WebView*) = 0;
        virtual void setInputMethodEnabled(WebView*, bool enabled) = 0;

        virtual void didBeginEditing(WebView*) = 0;
        virtual void didChangeSelection(WebView*, bool isSelectionEmpty) = 0;
        virtual void didChangeContents(WebView*) = 0;
        virtual void didExecuteCommand(WebView*, const WebString& commandName) = 0;
        virtual void didEndEditing(WebView*) = 0;
    };

} // namespace WebKit

#endif
