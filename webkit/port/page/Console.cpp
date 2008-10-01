/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Console.h"

#include "ChromeClient.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorController.h"
#include "NotImplemented.h"
#include "Page.h"

namespace WebCore {

Console::Console(Frame* frame)
    : m_frame(frame)
{
}

void Console::disconnectFrame()
{
    m_frame = 0;
}

void Console::debug(const String& message)
{
    // In Firebug, console.debug has the same behavior as console.log. So we'll
    // do the same.
    log(message);
}

void Console::error(const String& message)
{
    // TODO(erg): For all of these methods which call addMessageToConsole(), we
    // always assume the line number of the console.error(...) was called on
    // line 0.  To fix this, we need to modify the V8 version of the Console
    // interface to also pass in the current line number, which will be hard
    // since it looks like that information isn't publicly accessible with the
    // current v8 interface. <http://crbug.com/2960>
    //
    // This will fix this currently broken test:
    //    LayoutTests/fast/dom/Window/console-functions.html

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, 0, url.string());
}

void Console::info(const String& message)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, message, 0, url.string());
}

void Console::log(const String& message)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, LogMessageLevel, message, 0, url.string());
}

void Console::warn(const String& message)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    const KURL& url = m_frame->loader()->url();
    String prettyURL = url.prettyURL();

    page->chrome()->client()->addMessageToConsole(message, 0, prettyURL);
    page->inspectorController()->addMessageToConsole(JSMessageSource, WarningMessageLevel, message, 0, url.string());
}

void Console::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceURL)
{
    Page* page = this->page();
    if (!page)
        return;

    if (source == JSMessageSource)
        page->chrome()->client()->addMessageToConsole(message, lineNumber, sourceURL);

    page->inspectorController()->addMessageToConsole(source, level, message, lineNumber, sourceURL);
}

void Console::time(const String& title)
{
    notImplemented();
}

void Console::groupEnd()
{
    notImplemented();
}

Page* Console::page() const
{
    if (!m_frame)
        return 0;

    return m_frame->page();
}

} // namespace WebCore
