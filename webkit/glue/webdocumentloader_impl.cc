/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "webkit/glue/webdocumentloader_impl.h"
#undef LOG

#include "base/logging.h"

using WebCore::DocumentLoader;
using WebCore::ResourceRequest;
using WebCore::SubstituteData;

PassRefPtr<WebDocumentLoaderImpl> WebDocumentLoaderImpl::create(
    const ResourceRequest& request, const SubstituteData& data) {
  return adoptRef(new WebDocumentLoaderImpl(request, data));
}

WebDocumentLoaderImpl::WebDocumentLoaderImpl(const ResourceRequest& request,
                                             const SubstituteData& data)
    : DocumentLoader(request, data),
      lock_history_(false),
      form_submit_(false) {
}

void WebDocumentLoaderImpl::SetDataSource(WebDataSource* datasource) {
  datasource_.reset(datasource);
}

WebDataSource* WebDocumentLoaderImpl::GetDataSource() const {
  return datasource_.get();
}

// DocumentLoader
void WebDocumentLoaderImpl::attachToFrame() {
  DocumentLoader::attachToFrame();
  if (detached_datasource_.get()) {
    DCHECK(datasource_.get());
    datasource_.reset(detached_datasource_.release());
  }
}

void WebDocumentLoaderImpl::detachFromFrame() {
  DocumentLoader::detachFromFrame();
  detached_datasource_.reset(datasource_.release());
}
