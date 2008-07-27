/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 */

#include "config.h"
#include "SQLDatabase.h"

#include "Logging.h"

namespace WebCore {

// This is a stub to keep WebCore compiling. We do not use WebKit's
// favicon database.

SQLDatabase::SQLDatabase()
    : m_db(0)
{

}

bool SQLDatabase::open(const String& filename)
{
    return false;
}

void SQLDatabase::close()
{
}

void SQLDatabase::setFullsync(bool fsync) 
{
}

void SQLDatabase::setSynchronous(SynchronousPragma sync)
{
}

void SQLDatabase::setBusyTimeout(int ms)
{
}

void SQLDatabase::setBusyHandler(int(*handler)(void*, int))
{
}

bool SQLDatabase::executeCommand(const String& sql)
{
    return false;
}

bool SQLDatabase::returnsAtLeastOneResult(const String& sql)
{
    return false;
}

bool SQLDatabase::tableExists(const String& tablename)
{
    return false;
}

void SQLDatabase::clearAllTables()
{
}

void SQLDatabase::runVacuumCommand()
{
}

int64_t SQLDatabase::lastInsertRowID()
{
    return 0;
}

int SQLDatabase::lastChanges()
{
    return 0;
}

int SQLDatabase::lastError()
{
    return 0;
}

const char* SQLDatabase::lastErrorMsg()
{
    return "";
}

} // namespace WebCore


