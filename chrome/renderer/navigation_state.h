// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NAVIGATION_STATE_H_
#define CHROME_RENDERER_NAVIGATION_STATE_H_

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/searchable_form_data.h"

// The RenderView stores an instance of this class in the "extra data" of each
// WebDataSource (see RenderView::DidCreateDataSource).
class NavigationState : public WebKit::WebDataSource::ExtraData {
 public:
  static NavigationState* CreateBrowserInitiated(
      int32 pending_page_id,
      PageTransition::Type transition_type,
      base::Time request_time) {
    return new NavigationState(transition_type, request_time, false,
                               pending_page_id);
  }

  static NavigationState* CreateContentInitiated() {
    // We assume navigations initiated by content are link clicks.
    return new NavigationState(PageTransition::LINK, base::Time(), true, -1);
  }

  static NavigationState* FromDataSource(WebKit::WebDataSource* ds) {
    return static_cast<NavigationState*>(ds->extraData());
  }

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id() const { return pending_page_id_; }

  // Is this a new navigation?
  bool is_new_navigation() const { return pending_page_id_ == -1; }

  // Contains the transition type that the browser specified when it
  // initiated the load.
  PageTransition::Type transition_type() const { return transition_type_; }
  void set_transition_type(PageTransition::Type type) {
    transition_type_ = type;
  }

  // The time that this navigation was requested.
  const base::Time& request_time() const {
    return request_time_;
  }
  void set_request_time(const base::Time& value) {
    request_time_ = value;
  }

  // The time that the document load started.
  const base::Time& start_load_time() const {
    return start_load_time_;
  }
  void set_start_load_time(const base::Time& value) {
    start_load_time_ = value;
  }

  // The time that the document load was committed.
  const base::Time& commit_load_time() const {
    return commit_load_time_;
  }
  void set_commit_load_time(const base::Time& value) {
    commit_load_time_ = value;
  }

  // The time that the document finished loading.
  const base::Time& finish_document_load_time() const {
    return finish_document_load_time_;
  }
  void set_finish_document_load_time(const base::Time& value) {
    finish_document_load_time_ = value;
  }

  // The time that the document and all subresources finished loading.
  const base::Time& finish_load_time() const {
    return finish_load_time_;
  }
  void set_finish_load_time(const base::Time& value) {
    finish_load_time_ = value;
  }

  // The time that layout first ran after a new navigation.
  const base::Time& first_paint_time() const {
    return first_paint_time_;
  }
  void set_first_paint_time(const base::Time& value) {
    first_paint_time_ = value;
  }

  // True if we have already processed the "DidCommitLoad" event for this
  // request.  Used by session history.
  bool request_committed() const { return request_committed_; }
  void set_request_committed(bool value) { request_committed_ = value; }

  // True if this navigation was not initiated via WebFrame::LoadRequest.
  bool is_content_initiated() const { return is_content_initiated_; }

  webkit_glue::SearchableFormData* searchable_form_data() const {
    return searchable_form_data_.get();
  }
  void set_searchable_form_data(webkit_glue::SearchableFormData* data) {
    searchable_form_data_.reset(data);
  }

  webkit_glue::PasswordForm* password_form_data() const {
    return password_form_data_.get();
  }
  void set_password_form_data(webkit_glue::PasswordForm* data) {
    password_form_data_.reset(data);
  }

 private:
  NavigationState(PageTransition::Type transition_type,
                  const base::Time& request_time,
                  bool is_content_initiated,
                  int32 pending_page_id)
      : transition_type_(transition_type),
        request_time_(request_time),
        request_committed_(false),
        is_content_initiated_(is_content_initiated),
        pending_page_id_(pending_page_id) {
  }

  PageTransition::Type transition_type_;
  base::Time request_time_;
  base::Time start_load_time_;
  base::Time commit_load_time_;
  base::Time finish_document_load_time_;
  base::Time finish_load_time_;
  base::Time first_paint_time_;
  bool request_committed_;
  bool is_content_initiated_;
  int32 pending_page_id_;
  scoped_ptr<webkit_glue::SearchableFormData> searchable_form_data_;
  scoped_ptr<webkit_glue::PasswordForm> password_form_data_;

  DISALLOW_COPY_AND_ASSIGN(NavigationState);
};

#endif  // CHROME_RENDERER_NAVIGATION_STATE_H_
