// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/base_session_service.h"

#include "base/pickle.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sessions/session_backend.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/stl_util-inl.h"

// InternalGetCommandsRequest -------------------------------------------------

BaseSessionService::InternalGetCommandsRequest::~InternalGetCommandsRequest() {
  STLDeleteElements(&commands);
}

// BaseSessionService ---------------------------------------------------------

namespace {

// Helper used by CreateUpdateTabNavigationCommand(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| - |*bytes_written|).
// |bytes_written| is incremented to reflect the data written.
void WriteStringToPickle(Pickle& pickle, int* bytes_written, int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteString(str);
  } else {
    pickle.WriteString(std::string());
  }
}

// Wide version of WriteStringToPickle.
void WriteWStringToPickle(Pickle& pickle, int* bytes_written, int max_bytes,
                          const std::wstring& str) {
  int num_bytes = str.size() * sizeof(wchar_t);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle.WriteWString(str);
  } else {
    pickle.WriteWString(std::wstring());
  }
}

}  // namespace

// Delay between when a command is received, and when we save it to the
// backend.
static const int kSaveDelayMS = 2500;

// static
const int BaseSessionService::max_persist_navigation_count = 6;

BaseSessionService::BaseSessionService(SessionType type,
                                       Profile* profile,
                                       const std::wstring& path)
    : profile_(profile),
      path_(path),
      backend_thread_(NULL),
#pragma warning(suppress: 4355)  // Okay to pass "this" here.
      save_factory_(this),
      pending_reset_(false),
      commands_since_reset_(0) {
  if (profile) {
    // We should never be created when off the record.
    DCHECK(!profile->IsOffTheRecord());
  }
  backend_ = new SessionBackend(type, profile_ ? profile_->GetPath() : path_);
  DCHECK(backend_.get());
  backend_thread_ = g_browser_process->file_thread();
  if (!backend_thread_)
    backend_->Init();
  // If backend_thread is non-null, backend will init itself as appropriate.
}

BaseSessionService::~BaseSessionService() {
}

void BaseSessionService::DeleteLastSession() {
  if (!backend_thread()) {
    backend()->DeleteLastSession();
  } else {
    backend_thread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend(), &SessionBackend::DeleteLastSession));
  }
}

void BaseSessionService::ScheduleCommand(SessionCommand* command) {
  DCHECK(command);
  commands_since_reset_++;
  pending_commands_.push_back(command);
  StartSaveTimer();
}

void BaseSessionService::StartSaveTimer() {
  // Don't start a timer when testing (profile == NULL or
  // MessageLoop::current() is NULL).
  if (MessageLoop::current() && profile() && save_factory_.empty()) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        save_factory_.NewRunnableMethod(&BaseSessionService::Save),
        kSaveDelayMS);
  }
}

void BaseSessionService::Save() {
  DCHECK(backend());

  if (pending_commands_.empty())
    return;

  if (!backend_thread()) {
    backend()->AppendCommands(
        new std::vector<SessionCommand*>(pending_commands_), pending_reset_);
  } else {
    backend_thread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend(), &SessionBackend::AppendCommands,
        new std::vector<SessionCommand*>(pending_commands_),
        pending_reset_));
  }
  // Backend took ownership of commands.
  pending_commands_.clear();

  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }
}

SessionCommand* BaseSessionService::CreateUpdateTabNavigationCommand(
    SessionID::id_type command_id,
    SessionID::id_type tab_id,
    int index,
    const NavigationEntry& entry) {
  // Use pickle to handle marshalling.
  Pickle pickle;
  pickle.WriteInt(tab_id);
  pickle.WriteInt(index);

  // We only allow navigations up to 63k (which should be completely
  // reasonable). On the off chance we get one that is too big, try to
  // keep the url.

  // Bound the string data (which is variable length) to
  // |max_state_size bytes| bytes.
  static const SessionCommand::size_type max_state_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;

  int bytes_written = 0;
  
  WriteStringToPickle(pickle, &bytes_written, max_state_size,
                      entry.display_url().spec());

  WriteWStringToPickle(pickle, &bytes_written, max_state_size,
                       entry.title());

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
                      entry.content_state());

  pickle.WriteInt(entry.transition_type());
  int type_mask = entry.has_post_data() ? TabNavigation::HAS_POST_DATA : 0;
  pickle.WriteInt(type_mask);

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
      entry.referrer().is_valid() ? entry.referrer().spec() : std::string());

  // Adding more data? Be sure and update TabRestoreService too.
  return new SessionCommand(command_id, pickle);
}

bool BaseSessionService::RestoreUpdateTabNavigationCommand(
    const SessionCommand& command,
    TabNavigation* navigation,
    SessionID::id_type* tab_id) {
  scoped_ptr<Pickle> pickle(command.PayloadAsPickle());
  if (!pickle.get())
    return false;
  void* iterator = NULL;
  std::string url_spec;
  if (!pickle->ReadInt(&iterator, tab_id) ||
      !pickle->ReadInt(&iterator, &(navigation->index_)) ||
      !pickle->ReadString(&iterator, &url_spec) ||
      !pickle->ReadWString(&iterator, &(navigation->title_)) ||
      !pickle->ReadString(&iterator, &(navigation->state_)) ||
      !pickle->ReadInt(&iterator,
                       reinterpret_cast<int*>(&(navigation->transition_))))
    return false;
  // type_mask did not always exist in the written stream. As such, we
  // don't fail if it can't be read.
  bool has_type_mask = pickle->ReadInt(&iterator, &(navigation->type_mask_));

  if (has_type_mask) {
    // the "referrer" property was added after type_mask to the written
    // stream. As such, we don't fail if it can't be read.
    std::string referrer_spec;
    pickle->ReadString(&iterator, &referrer_spec);
    if (!referrer_spec.empty())
      navigation->referrer_ = GURL(referrer_spec);
  }

  navigation->url_ = GURL(url_spec);
  return true;
}

bool BaseSessionService::ShouldTrackEntry(const NavigationEntry& entry) {
  // Don't track entries that have post data. Post data may contain passwords
  // and other sensitive data users don't want stored to disk.
  return entry.display_url().is_valid() && !entry.has_post_data();
}

bool BaseSessionService::ShouldTrackEntry(const TabNavigation& navigation) {
  // Don't track entries that have post data. Post data may contain passwords
  // and other sensitive data users don't want stored to disk.
  return navigation.url().is_valid() &&
         (navigation.type_mask() & TabNavigation::HAS_POST_DATA) == 0;
}

BaseSessionService::Handle BaseSessionService::ScheduleGetLastSessionCommands(
    InternalGetCommandsRequest* request,
    CancelableRequestConsumerBase* consumer) {
  scoped_refptr<InternalGetCommandsRequest> request_wrapper(request);
  AddRequest(request, consumer);
  if (backend_thread()) {
    backend_thread()->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        backend(), &SessionBackend::ReadLastSessionCommands, request));
  } else {
    backend()->ReadLastSessionCommands(request);
  }
  return request->handle();
}
