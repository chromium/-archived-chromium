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

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "KURL.h"
#include "FrameLoadRequest.h"
#include "ResourceRequest.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/string_util.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webdatasource_impl.h"
#include "webkit/glue/webdocumentloader_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/weburlrequest_impl.h"

// WebDataSource ----------------------------------------------------------------

WebDataSourceImpl::WebDataSourceImpl(WebFrameImpl* frame,
                                     WebDocumentLoaderImpl* loader) :
  frame_(frame),
  loader_(loader),
  initial_request_(loader->originalRequest()),
  request_(loader->request()) {
}

WebDataSourceImpl::~WebDataSourceImpl() {
}

// static
WebDataSourceImpl* WebDataSourceImpl::CreateInstance(
    WebFrameImpl* frame, WebDocumentLoaderImpl* loader) {
  return new WebDataSourceImpl(frame, loader);
}

// WebDataSource
WebFrame* WebDataSourceImpl::GetWebFrame() {
  return frame_;
}

const WebRequest& WebDataSourceImpl::GetInitialRequest() const {
  // WebKit may change the frame load request as it sees fit, so we must sync
  // our request object.
  initial_request_.set_frame_load_request(
      WebCore::FrameLoadRequest(loader_->originalRequest()));
  return initial_request_;
}

const WebRequest& WebDataSourceImpl::GetRequest() const {
  // WebKit may change the frame load request as it sees fit, so we must sync
  // our request object.
  request_.set_frame_load_request(
      WebCore::FrameLoadRequest(loader_->request()));
  return request_;
}

const WebResponse& WebDataSourceImpl::GetResponse() const {
  response_.set_resource_response(loader_->response());
  return response_;
}

void WebDataSourceImpl::SetExtraData(WebRequest::ExtraData* extra) {
  initial_request_.SetExtraData(extra);
  request_.SetExtraData(extra);
}

std::wstring WebDataSourceImpl::GetResponseMimeType() const {
  return webkit_glue::StringToStdWString(loader_->responseMIMEType());
}

GURL WebDataSourceImpl::GetUnreachableURL() const {
  const WebCore::KURL& url = loader_->unreachableURL();
  return url.isEmpty() ? GURL() : webkit_glue::KURLToGURL(url);
}

bool WebDataSourceImpl::HasUnreachableURL() const {
  const WebCore::KURL& url = loader_->unreachableURL();
  return !url.isEmpty();
}

const std::vector<GURL>& WebDataSourceImpl::GetRedirectChain() const {
  return redirect_chain_;
}

void WebDataSourceImpl::ClearRedirectChain() {
  redirect_chain_.clear();
}

void WebDataSourceImpl::AppendRedirect(const GURL& url) {
  redirect_chain_.push_back(url);
}

const SearchableFormData* WebDataSourceImpl::GetSearchableFormData() const {
  return loader_->searchable_form_data();
}

const PasswordForm* WebDataSourceImpl::GetPasswordFormData() const {
  return loader_->password_form_data();
}

bool WebDataSourceImpl::IsFormSubmit() const {
  return loader_->is_form_submit();
}

std::wstring WebDataSourceImpl::GetPageTitle() const {
  return webkit_glue::StringToStdWString(loader_->title());
}

/*
See comment in webdatasource.h

void WebDataSourceImpl::GetData(IStream** data) {
  DebugBreak();
}

void WebDataSourceImpl::GetRepresentation(IWebDocumentRepresentation** rep) {
  DebugBreak();
}

void WebDataSourceImpl::GetResponse(IWebURLResponse** response) {
  DebugBreak();
}

std::wstring WebDataSourceImpl::GetTextEncodingName() {
  DebugBreak();
  return L"";
}

bool WebDataSourceImpl::IsLoading() {
  DebugBreak();
}

void WebDataSourceImpl::GetWebArchive(IWebArchive** archive) {
  DebugBreak();
}

void WebDataSourceImpl::GetMainResource(IWebResource** resource) {
  DebugBreak();
}

void WebDataSourceImpl::GetSubresources(int* count, IWebResource*** resources) {
  DebugBreak();
}

void WebDataSourceImpl::GetSubresourceForURL(const std::wstring& url,
                                             IWebResource** resource) {
  DebugBreak();
}

void WebDataSourceImpl::AddSubresource(IWebResource* subresource) {
  DebugBreak();
}
*/
