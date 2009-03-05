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
#include "webkit/glue/webhistoryitem_impl.h"

#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "HistoryItem.h"
MSVC_POP_WARNING();


WebHistoryItem* WebHistoryItem::Create(const GURL& url,
                                       const std::wstring& title,
                                       const std::string& history_state,
                                       WebRequest::ExtraData* extra_data) {
  return new WebHistoryItemImpl(url, title, history_state, extra_data);
}

WebHistoryItemImpl::WebHistoryItemImpl(const GURL& url,
                                       const std::wstring& title,
                                       const std::string& history_state,
                                       WebRequest::ExtraData* extra_data) :
    url_(url),
    title_(title),
    history_state_(history_state),
    history_item_(NULL),
    extra_data_(extra_data) {
}

WebHistoryItemImpl::~WebHistoryItemImpl() {
}

const std::wstring& WebHistoryItemImpl::GetTitle() const {
  return title_;
}

const std::string& WebHistoryItemImpl::GetHistoryState() const {
  return history_state_;
}

WebRequest::ExtraData* WebHistoryItemImpl::GetExtraData() const {
  return extra_data_.get();
}

WebCore::HistoryItem* WebHistoryItemImpl::GetHistoryItem() const {
  if (history_item_)
    return history_item_.get();

  if (history_state_.size() > 0) {
    history_item_ = webkit_glue::HistoryItemFromString(history_state_);
  } else {
    history_item_ = WebCore::HistoryItem::create(
      webkit_glue::StdStringToString(url_.spec()),
      webkit_glue::StdWStringToString(title_),
      0.0);
  }

  return history_item_.get();
}

const GURL& WebHistoryItemImpl::GetURL() const {
  return url_;
}
