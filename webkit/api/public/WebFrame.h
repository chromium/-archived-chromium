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

#ifndef WebFrame_h
#define WebFrame_h

#error "This header file is still a work in progress; do not include!"

#include "WebCommon.h"

struct NPObject;

namespace WebKit {
    class WebData;
    class WebDataSource;
    class WebHistoryItem;
    class WebString;
    class WebURL;
    class WebURLRequest;
    class WebView;

    class WebFrame {
    public:
        // Returns the frame that is currently executing script or 0 if there
        // is none.
        WEBKIT_API static WebFrame* activeFrame();


        // Basic properties ---------------------------------------------------

        // The name of this frame.
        virtual WebString name() = 0;

        // The url of the document loaded in this frame.  This is equivalent to
        // dataSource()->request().url().
        virtual WebURL url() = 0;

        // The url of the favicon (if any) specified by the document loaded in
        // this frame.
        virtual WebURL favIconURL() = 0;

        // The url of the OpenSearch Desription Document (if any) specified by
        // the document loaded in this frame.
        virtual WebURL openSearchDescriptionURL() = 0;

        // Returns the security origin of the current document.
        virtual WebString securityOrigin() = 0;


        // Geometry -----------------------------------------------------------

        // NOTE: These routines do not force page layout so their results may
        // not be accurate if the page layout is out-of-date.

        // The scroll offset from the top-left corner of the frame in pixels.
        virtual WebSize scrollOffset() = 0;

        // The size of the contents area.
        virtual WebSize contentsSize() = 0;

        // Returns the minimum preferred width of the content contained in the
        // current document.
        virtual int contentsPreferredWidth() = 0;

        // Returns true if the contents (minus scrollbars) has non-zero area.
        virtual bool hasVisibleContent() = 0;


        // Hierarchy ----------------------------------------------------------

        // Returns the containing view.
        virtual WebView* view() = 0;

        // Returns the parent frame.
        virtual WebFrame* parent() = 0;

        // Returns the top-most frame in the hierarchy containing this frame.
        virtual WebFrame* top() = 0;

        // Returns the first/last child frame.
        virtual WebFrame* firstChild() = 0;
        virtual WebFrame* lastChild() = 0;

        // Returns the next/previous sibling frame.
        virtual WebFrame* nextSibling() = 0;
        virtual WebFrame* previousSibling() = 0;

        // Returns the next/previous frame in "frame traversal order"
        // optionally wrapping around.
        virtual WebFrame* traverseNext(bool wrap) = 0;
        virtual WebFrame* traversePrevious(bool wrap) = 0;

        // Returns the child frame identified by the given name.
        virtual WebFrame* findChildByName(const WebString& name) = 0;

        // Returns the child frame identified by the given xpath expression.
        virtual WebFrame* findChildByExpression(const WebString& xpath) = 0;


        // Scripting ----------------------------------------------------------

        // Calls window.gc() if it is defined.
        virtual void collectGarbage() = 0;

        // Returns a NPObject corresponding to this frame's DOMWindow.
        virtual NPObject* windowObject() = 0;

        // Binds a NPObject as a property of this frame's DOMWindow.
        virtual void bindToWindowObject(const WebString& name, NPObject*) = 0;

        // Executes script in the context of the current page.
        virtual void executeScript(const WebScriptSource&) = 0;

        // Executes script in a new context associated with the frame. The
        // script gets its own global scope and its own prototypes for
        // intrinsic JS objects (String, Array, and so-on). It shares the
        // wrappers for all DOM nodes and DOM constructors.
        virtual void executeScriptInNewContext(const WebScriptSource* sources,
                                               unsigned numSources) = 0;

        // Logs to the console associated with this frame.
        virtual void addMessageToConsole(const WebConsoleMessage&) = 0;


        // Styling -------------------------------------------------------------

        // Insert the given text as a STYLE element at the beginning of the
        // document.
        virtual bool insertStyleText(const WebString&) = 0;


        // Navigation ----------------------------------------------------------

        virtual void reload() = 0;

        virtual void loadRequest(const WebURLRequest&) = 0;

        virtual void loadHistoryItem(const WebHistoryItem&) = 0;

        virtual void loadData(const WebData& data,
                              const WebString& mimeType,
                              const WebString& textEncoding,
                              const WebURL& baseURL,
                              const WebURL& unreachableURL = WebURL(),
                              bool replace = false) = 0;

        virtual void loadHTMLString(const WebData& html,
                                    const WebURL& baseURL,
                                    const WebURL& unreachableURL = WebURL(),
                                    bool replace = false) = 0;

        virtual bool isLoading() = 0;

        // Stops any pending loads on the frame and its children.
        virtual void stopLoading() = 0;

        // Returns the data source that is currently loading.  May be null.
        virtual WebDataSource* provisionalDataSource() = 0;

        // Returns the data source that is currently loaded.
        virtual WebDataSource* dataSource() = 0;

        // Returns the previous history item.  Check WebHistoryItem::isNull()
        // before using.
        virtual WebHistoryItem previousHistoryItem() = 0;

        // Returns the current history item.  Check WebHistoryItem::isNull()
        // before using.
        virtual WebHistoryItem currentHistoryItem() = 0;

        // View-source rendering mode.  Set this before loading an URL to cause
        // it to be rendered in view-source mode.
        virtual void enableViewSourceMode(bool) = 0;
        virtual bool isViewSourceModeEnabled() = 0;


        // App-cache -----------------------------------------------------------

        virtual void selectAppCacheWithoutManifest() = 0;
        virtual void selectAppCacheWithManifest(const WebURL& manifest) = 0;

        // Will be null if an app cache has not been selected.
        virtual WebAppCacheContext* appCacheContext() = 0;


        // Editing -------------------------------------------------------------

        // Replaces the selection with the given text.
        virtual void replaceSelection(const WebString& text) = 0;

        // See EditorCommand.cpp for the list of supported commands.
        virtual void executeCommand(const WebString&) = 0;
        virtual void executeCommand(const WebString&, const WebString& value) = 0;
        virtual bool isCommandEnabled(const WebString&);

        // Spell-checking support.
        virtual void enableContinuousSpellChecking(bool) = 0;
        virtual bool isContinuousSpellCheckingEnabled() = 0;


        // Selection -----------------------------------------------------------

        virtual void selectAll() = 0;
        virtual void selectNone() = 0;

        virtual WebString selectionAsText() = 0;
        virtual WebString selectionAsHTML() = 0;


        // Printing ------------------------------------------------------------

        // Reformats the WebFrame for printing.  pageSize is the page size in
        // pixels.  Returns the number of pages that can be printed at the
        // given page size.
        virtual int printBegin(const WebSize& pageSize);

        // Prints one page, and returns the calculated page shrinking factor
        // (usually between 1/1.25 and 1/2).  Returns 0 if the page number is
        // invalid or not in printing mode.
        virtual float printPage(int pageToPrint, const WebCanvas&);

        // Reformats the WebFrame for screen display.
        virtual void printEnd();


        // Find-in-page --------------------------------------------------------

        // FIXME
    };

} // namespace WebKit

#endif
