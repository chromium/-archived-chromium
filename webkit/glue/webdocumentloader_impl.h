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

#ifndef WEBKIT_GLUE_WEBDOCUMENTLOADER_IMPL_H__
#define WEBKIT_GLUE_WEBDOCUMENTLOADER_IMPL_H__

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "DocumentLoader.h"
MSVC_POP_WARNING();

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/searchable_form_data.h"
#include "webkit/glue/webdatasource.h"

class WebDataSource;

class WebDocumentLoaderImpl : public WebCore::DocumentLoader {
 public:
  static PassRefPtr<WebDocumentLoaderImpl> create(
      const WebCore::ResourceRequest&, const WebCore::SubstituteData&);

  void SetDataSource(WebDataSource*);
  WebDataSource* GetDataSource() const;

  void SetLockHistory(bool lock_history) { lock_history_ = lock_history; }
  bool GetLockHistory() const { return lock_history_; }

  // DocumentLoader
  virtual void attachToFrame();
  virtual void detachFromFrame();

  // Sets the SearchableFormData for this DocumentLoader.
  // WebDocumentLoaderImpl will own the SearchableFormData.
  void set_searchable_form_data(SearchableFormData* searchable_form_data) {
    searchable_form_data_.reset(searchable_form_data);
  }
  // Returns the SearchableFormData for this DocumentLoader.
  // WebDocumentLoaderImpl owns the returned SearchableFormData.
  const SearchableFormData* searchable_form_data() const {
    return searchable_form_data_.get();
  }

  // Sets the PasswordFormData for this DocumentLoader.
  // WebDocumentLoaderImpl will own the PasswordFormData.
  void set_password_form_data(PasswordForm* password_form_data) {
    password_form_data_.reset(password_form_data);
  }
  // Returns the PasswordFormData for this DocumentLoader.
  // WebDocumentLoaderImpl owns the returned PasswordFormData.
  const PasswordForm* password_form_data() const {
    return password_form_data_.get();
  }

  void set_form_submit(bool value) {
    form_submit_ = value;
  }
  bool is_form_submit() const {
    return form_submit_;
  }

 private:
  WebDocumentLoaderImpl(const WebCore::ResourceRequest&,
                        const WebCore::SubstituteData&);

  scoped_ptr<WebDataSource> datasource_;
  scoped_ptr<WebDataSource> detached_datasource_;
  scoped_ptr<const SearchableFormData> searchable_form_data_;
  scoped_ptr<const PasswordForm> password_form_data_;

  bool lock_history_;

  bool form_submit_;

  DISALLOW_EVIL_CONSTRUCTORS(WebDocumentLoaderImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBDOCUMENTLOADER_IMPL_H__
