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

#if COMPILER(MSVC)
__pragma(warning(push, 0))
#endif
#include "IconLoader.h"

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "IconDatabase.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include "SubresourceLoader.h"
#if COMPILER(MSVC)
__pragma(warning(pop))
#endif

using namespace std;

namespace WebCore {

// This is a stub implementation of WebKit's IconLoader class that does
// nothing.

IconLoader::IconLoader(Frame* frame)
    : m_frame(frame)
    , m_loadIsInProgress(false)
{
}

// TODO(brettw) it may be we want to hook our icon loading framework up to here
// instead of being in the RenderView. This may make for easier integration in
// the future.
auto_ptr<IconLoader> IconLoader::create(Frame* frame)
{
    return auto_ptr<IconLoader>(new IconLoader(frame));
}

IconLoader::~IconLoader()
{
}

void IconLoader::startLoading()
{
}

void IconLoader::stopLoading()
{
}

void IconLoader::didReceiveResponse(SubresourceLoader* resourceLoader, const ResourceResponse& response)
{
}

void IconLoader::didReceiveData(SubresourceLoader* loader, const char*, int size)
{
}

void IconLoader::didFail(SubresourceLoader* resourceLoader, const ResourceError&)
{
}

void IconLoader::didFinishLoading(SubresourceLoader* resourceLoader)
{
}

void IconLoader::didReceiveAuthenticationChallenge(SubresourceLoader* resourceLoader, const AuthenticationChallenge& challenge)
{
}

void IconLoader::finishLoading(const KURL& iconURL, PassRefPtr<SharedBuffer> data)
{
}

void IconLoader::clearLoadingState()
{
}

}  // namespace WebCore
