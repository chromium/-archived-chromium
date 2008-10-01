// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

// Javascript exception abstraction

#ifndef ExceptionContext_h
#define ExceptionContext_h

#include <wtf/Noncopyable.h>
#include "ScriptController.h"

namespace WebCore {

class Node;

// Provides context of an exception. This class is an abstraction of JSC's
// ExecState. In V8, its purpose is to carry along the exceptions captured
// by the ExceptionCatcher.
class ExceptionContext : Noncopyable {
public:
    ExceptionContext();
    ~ExceptionContext();

    bool hadException();
    JSException exception() const { return m_exception; }

    static ExceptionContext* createFromNode(Node*);

    // Returns a non-exception code object.
    static JSException NoException();

private:
    void setException(JSException exception) { m_exception = exception; }

    JSException m_exception;

#if USE(V8)
    friend class ExceptionCatcher;
    void setExceptionCatcher(ExceptionCatcher*);
    ExceptionCatcher* m_exceptionCatcher;
#endif
};

#if USE(V8)
// A wrapper around v8::TryCatch helper in order to facilitate updating
// ExceptionContext with the latest exceptions that may have occurred.
class ExceptionCatcher {
public:
    ExceptionCatcher(ExceptionContext*);
    ~ExceptionCatcher();
    void updateContext();
    void detachContext();

private:
    ExceptionContext* m_context;
    v8::TryCatch m_catcher;
};
#endif

}  // namespace WebCore

#endif  // !defined(ExceptionContext_h)
