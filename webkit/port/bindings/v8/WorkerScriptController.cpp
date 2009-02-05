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

#if ENABLE(WORKERS)

#include "WorkerScriptController.h"

#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "v8_proxy.h"
#include "DOMTimer.h"
#include "WorkerContext.h"
#include "WorkerMessagingProxy.h"
#include "WorkerThread.h"

namespace WebCore {

// TODO(dimich): Move these stubs once they're implemented.
int DOMTimer::install(ScriptExecutionContext*, ScheduledAction*, int timeout,
                      bool singleShot) {
  return 0;
}
void DOMTimer::removeById(ScriptExecutionContext*, int timeoutId) {
}

WorkerScriptController::WorkerScriptController(WorkerContext* workerContext)
    : m_workerContext(workerContext)
    , m_executionForbidden(false)
{
}

WorkerScriptController::~WorkerScriptController()
{
    Dispose();
}

void WorkerScriptController::Dispose()
{
    if (!m_context.IsEmpty()) {
        m_context.Dispose();
        m_context.Clear();
    }
}

void WorkerScriptController::InitContextIfNeeded()
{
    // Bail out if the context has already been initialized.
    if (!m_context.IsEmpty())
        return;

    // Create a new environment
    v8::Persistent<v8::ObjectTemplate> global_template;
    m_context = v8::Context::New(NULL, global_template);
        
    // TODO (jianli): to initialize the context.
}

ScriptValue WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode)
{
    {
        MutexLocker lock(m_sharedDataMutex);
        if (m_executionForbidden)
            return ScriptValue();
    }

    v8::Local<v8::Value> result;
    {
        v8::Locker locker;
        v8::HandleScope hs;

        InitContextIfNeeded();
        v8::Context::Scope scope(m_context);

        v8::Local<v8::String> code = v8ExternalString(sourceCode.source());
        v8::Handle<v8::Script> script =
            V8Proxy::CompileScript(code,
                                   sourceCode.url().string(),
                                   sourceCode.startLine() - 1);

        // TODO (jianli): handle infinite recursion.

        result = script->Run();

        if (V8Proxy::HandleOutOfMemory())
            return ScriptValue();
    }

    m_workerContext->thread()->messagingProxy()->
        reportWorkerThreadActivity(m_workerContext->hasPendingActivity());

    return ScriptValue(result);
}

void WorkerScriptController::forbidExecution()
{
    // This function is called from another thread.
    MutexLocker lock(m_sharedDataMutex);
    m_executionForbidden = true;

    Dispose();
}

} // namespace WebCore

#endif // ENABLE(WORKERS)
