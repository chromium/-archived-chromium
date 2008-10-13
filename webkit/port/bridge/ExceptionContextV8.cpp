// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "ExceptionContext.h"

#include "Node.h"

namespace WebCore {

// Unlike JSC, which stores exceptions in ExecState that is accessible from
// ScriptController that is retrievable from Node*, V8 uses static chain of
// handlers (encapsulated as v8::TryCatch and here as ExceptionCatcher)
// to track exceptions, so it has no need for Node*.
ExceptionContext::ExceptionContext(Node* node)
{
}

ExceptionContext::ExceptionContext()
    : m_exception()
    , m_exceptionCatcher(0)
{
}

void ExceptionContext::setExceptionCatcher(ExceptionCatcher* exceptionCatcher)
{
    if (m_exceptionCatcher && exceptionCatcher)
        m_exceptionCatcher->detachContext();

    m_exceptionCatcher = exceptionCatcher;
}

bool ExceptionContext::hadException()
{
    if (m_exceptionCatcher)
        m_exceptionCatcher->updateContext();

    return !m_exception.IsEmpty();
}

JSException ExceptionContext::exception() const
{
    return m_exception;
}

JSException ExceptionContext::noException()
{
    return v8::Local<v8::Value>();
}

ExceptionCatcher::ExceptionCatcher(ExceptionContext* exceptionContext)
    : m_context(exceptionContext)
    , m_catcher()
{
    exceptionContext->setExceptionCatcher(this);
}

void ExceptionCatcher::detachContext()
{
    m_context = 0;
}

void ExceptionCatcher::updateContext()
{
    ASSERT(m_context);

    if (m_catcher.HasCaught())
        m_context->setException(m_catcher.Exception());
    else
        m_context->setException(ExceptionContext::noException());
}

ExceptionCatcher::~ExceptionCatcher()
{
    if (!m_context)
        return;

    updateContext();
    m_context->setExceptionCatcher(0);
}

}  // namespace WebCore
