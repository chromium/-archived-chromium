/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerScriptController_h
#define WorkerScriptController_h

#if ENABLE(WORKERS)

#include <wtf/OwnPtr.h>
#include <wtf/Threading.h>
#include "v8.h"

namespace WebCore {

    class ScriptSourceCode;
    class ScriptValue;
    class WorkerContext;
    class WorkerContextExecutionProxy;

    class WorkerScriptController {
    public:
        WorkerScriptController(WorkerContext*);
        ~WorkerScriptController();

        WorkerContextExecutionProxy* proxy() { return m_proxy.get(); }

        ScriptValue evaluate(const ScriptSourceCode&);

        void forbidExecution();

    private:
        WorkerContext* m_workerContext;
        OwnPtr<WorkerContextExecutionProxy> m_proxy;

        Mutex m_sharedDataMutex;
        bool m_executionForbidden;
    };

} // namespace WebCore

#endif // ENABLE(WORKERS)

#endif // WorkerScriptController_h
