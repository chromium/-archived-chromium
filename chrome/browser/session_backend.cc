// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/session_backend.h"

#include <limits>

#include "base/file_util.h"
#include "base/histogram.h"
#include "base/pickle.h"
#include "chrome/common/scoped_vector.h"

// File version number.
static const int32 kFileCurrentVersion = 1;

// The signature at the beginning of the file = SSNS (Sessions).
static const int32 kFileSignature = 0x53534E53;

namespace {

// SessionFileReader ----------------------------------------------------------

// SessionFileReader is responsible for reading the set of SessionCommands that
// describe a Session back from a file. SessionFileRead does minimal error
// checking on the file (pretty much only that the header is valid).

class SessionFileReader {
 public:
  typedef SessionCommand::id_type id_type;
  typedef SessionCommand::size_type size_type;

  SessionFileReader(const std::wstring& path)
      : handle_(CreateFile(path.c_str(), GENERIC_READ, 0, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)),
                           buffer_(SessionBackend::kFileReadBufferSize, 0),
        buffer_position_(0),
        available_count_(0),
        errored_(false) {
  }
  // Reads the contents of the file specified in the constructor, returning
  // true on success. It is up to the caller to free all SessionCommands
  // added to commands.
  bool Read(std::vector<SessionCommand*>* commands);

 private:
  // Reads a single command, returning it. A return value of NULL indicates
  // either there are no commands, or there was an error. Use errored_ to
  // distinguish the two. If NULL is returned, and there is no error, it means
  // the end of file was sucessfully reached.
  SessionCommand* ReadCommand();

  // Shifts the unused portion of buffer_ to the beginning and fills the
  // remaining portion with data from the file. Returns false if the buffer
  // couldn't be filled. A return value of false only signals an error if
  // errored_ is set to true.
  bool FillBuffer();

  // Whether an error condition has been detected (
  bool errored_;

  // As we read from the file, data goes here.
  std::string buffer_;

  // The file.
  ScopedHandle handle_;

  // Position in buffer_ of the data.
  size_t buffer_position_;

  // Number of available bytes; relative to buffer_position_.
  size_t available_count_;

  DISALLOW_EVIL_CONSTRUCTORS(SessionFileReader);
};

bool SessionFileReader::Read(std::vector<SessionCommand*>* commands) {
  if (!handle_.IsValid())
    return false;
  int32 header[2];
  DWORD read_count;
  TimeTicks start_time = TimeTicks::Now();
  if (!ReadFile(handle_, &header, sizeof(header), &read_count, NULL) ||
      read_count != sizeof(header) || header[0] != kFileSignature ||
      header[1] != kFileCurrentVersion)
    return false;

  ScopedVector<SessionCommand> read_commands;
  SessionCommand* command;
  while ((command = ReadCommand()) && !errored_)
    read_commands->push_back(command);
  if (!errored_)
    read_commands->swap(*commands);
  UMA_HISTOGRAM_TIMES(L"SessionRestore.read_session_file_time",
                      TimeTicks::Now() - start_time);
  return !errored_;
}

SessionCommand* SessionFileReader::ReadCommand() {
  // Make sure there is enough in the buffer for the size of the next command.
  if (available_count_ < sizeof(size_type)) {
    if (!FillBuffer())
      return NULL;
    if (available_count_ < sizeof(size_type)) {
      // Still couldn't read a valid size for the command, assume write was
      // incomplete and return NULL.
      return NULL;
    }
  }
  // Get the size of the command.
  size_type command_size;
  memcpy(&command_size, &(buffer_[buffer_position_]), sizeof(command_size));
  buffer_position_ += sizeof(command_size);
  available_count_ -= sizeof(command_size);

  if (command_size == 0) {
    // Empty command. Shouldn't happen if write was successful, fail.
    return NULL;
  }

  // Make sure buffer has the complete contents of the command.
  if (command_size > available_count_) {
    if (command_size > buffer_.size())
      buffer_.resize((command_size / 1024 + 1) * 1024, 0);
    if (!FillBuffer() || command_size > available_count_) {
      // Again, assume the file was ok, and just the last chunk was lost.
      return NULL;
    }
  }
  const id_type command_id = buffer_[buffer_position_];
  // NOTE: command_size includes the size of the id, which is not part of
  // the contents of the SessionCommand.
  SessionCommand* command =
      new SessionCommand(command_id, command_size - sizeof(id_type));
  if (command_size > sizeof(id_type)) {
    memcpy(command->contents(),
           &(buffer_[buffer_position_ + sizeof(id_type)]),
           command_size - sizeof(id_type));
  }
  buffer_position_ += command_size;
  available_count_ -= command_size;
  return command;
}

bool SessionFileReader::FillBuffer() {
  if (available_count_ > 0 && buffer_position_ > 0) {
    // Shift buffer to beginning.
    memmove(&(buffer_[0]), &(buffer_[buffer_position_]), available_count_);
  }
  buffer_position_ = 0;
  DCHECK(buffer_position_ + available_count_ < buffer_.size());
  DWORD read_count;
  if (!ReadFile(handle_, &(buffer_[available_count_]),
                static_cast<DWORD>(buffer_.size() - available_count_),
                &read_count, NULL)) {
    errored_ = true;
    return false;
  }
  if (read_count == 0)
    return false;
  available_count_ += static_cast<int>(read_count);
  return true;
}

}  // namespace

// SessionCommand -------------------------------------------------------------

SessionCommand::SessionCommand(id_type id, size_type size)
    : id_(id),
      contents_(size, 0) {
}

SessionCommand::SessionCommand(id_type id, const Pickle& pickle)
    : id_(id),
      contents_(pickle.size(), 0) {
  DCHECK(pickle.size() < std::numeric_limits<size_type>::max());
  memcpy(contents(), pickle.data(), pickle.size());
}

bool SessionCommand::GetPayload(void* dest, size_t count) const {
  if (size() != count)
    return false;
  memcpy(dest, &(contents_[0]), count);
  return true;
}

Pickle* SessionCommand::PayloadAsPickle() const {
  return new Pickle(contents(), static_cast<int>(size()));
}

// SessionBackend -------------------------------------------------------------

// Target file name.
static const wchar_t* const kCurrentSessionFileName = L"Current Session";

// Previous target file.
static const wchar_t* const kLastSessionFileName = L"Last Session";

// Saved session file name.
static const wchar_t* const kSavedSessionFileName = L"Saved Session";

// static
const int SessionBackend::kFileReadBufferSize = 1024;

SessionBackend::SessionBackend(const std::wstring& path_to_dir)
    : path_to_dir_(path_to_dir),
      last_session_valid_(false),
      inited_(false),
      empty_file_(true) {
  // NOTE: this is invoked on the main thread, don't do file access here.
}

void SessionBackend::Init() {
  if (inited_)
    return;

  inited_ = true;

  // Create the directory for session info.
  file_util::CreateDirectory(path_to_dir_);

  MoveCurrentSessionToLastSession();
}

void SessionBackend::SaveSession(
    const std::vector<SessionCommand*>& commands) {
  Init();
  ScopedHandle handle(OpenAndWriteHeader(GetSavedSessionPath()));
  if (handle.IsValid())
    AppendCommandsToFile(handle, commands);
  STLDeleteContainerPointers(commands.begin(), commands.end());
}

void SessionBackend::AppendCommands(
    std::vector<SessionCommand*>* commands,
    bool reset_first) {
  Init();
  if ((reset_first && !empty_file_) || !current_session_handle_.IsValid())
    ResetFile();
  if (current_session_handle_.IsValid() &&
      !AppendCommandsToFile(current_session_handle_, *commands)) {
    current_session_handle_.Set(NULL);
  }
  empty_file_ = false;
  STLDeleteElements(commands);
  delete commands;
}

void SessionBackend::ReadSession(
    scoped_refptr<SessionService::InternalSavedSessionRequest> request) {
  if (request->canceled())
    return;
  Init();
  ReadSessionImpl(request->is_saved_session, &(request->commands));
  request->ForwardResult(
      SessionService::InternalSavedSessionRequest::TupleType(request->handle(),
                                                             request));
}

bool SessionBackend::ReadSessionImpl(bool use_save_file,
                                     std::vector<SessionCommand*>* commands) {
  Init();
  const std::wstring path =
      use_save_file ? GetSavedSessionPath() : GetLastSessionPath();
  SessionFileReader file_reader(path);
  return file_reader.Read(commands);
}

void SessionBackend::DeleteSession(bool saved_session) {
  Init();
  const std::wstring path =
      saved_session ? GetSavedSessionPath() : GetLastSessionPath();
  file_util::Delete(path, false);
}

void SessionBackend::CopyLastSessionToSavedSession() {
  Init();
  file_util::CopyFile(GetLastSessionPath(), GetSavedSessionPath());
}

void SessionBackend::MoveCurrentSessionToLastSession() {
  Init();
  current_session_handle_.Set(NULL);

  const std::wstring current_session_path = GetCurrentSessionPath();
  const std::wstring last_session_path = GetLastSessionPath();
  if (file_util::PathExists(last_session_path))
    file_util::Delete(last_session_path, false);
  if (file_util::PathExists(current_session_path)) {
    int64 file_size;
    if (file_util::GetFileSize(current_session_path, &file_size)) {
      UMA_HISTOGRAM_COUNTS(L"SessionRestore.last_session_file_size",
                           static_cast<int>(file_size / 1024));
    }
    last_session_valid_ = file_util::Move(current_session_path,
                                          last_session_path);
  }

  if (file_util::PathExists(current_session_path))
    file_util::Delete(current_session_path, false);

  // Create and open the file for the current session.
  ResetFile();
}

bool SessionBackend::AppendCommandsToFile(
    HANDLE handle,
    const std::vector<SessionCommand*>& commands) {
  for (std::vector<SessionCommand*>::const_iterator i = commands.begin();
       i != commands.end(); ++i) {
    DWORD wrote;
    const size_type content_size = static_cast<size_type>((*i)->size());
    const size_type total_size =  content_size + sizeof(id_type);
    UMA_HISTOGRAM_COUNTS(L"SessionRestore.command_size", total_size);
    if (!WriteFile(handle, &total_size, sizeof(total_size), &wrote, NULL) ||
        wrote != sizeof(total_size)) {
      NOTREACHED() << "error writing";
      return false;
    }
    id_type command_id = (*i)->id();
    if (!WriteFile(handle, &command_id, sizeof(command_id), &wrote, NULL) ||
        wrote != sizeof(command_id)) {
      NOTREACHED() << "error writing";
      return false;
    }
    if (!WriteFile(handle, (*i)->contents(), content_size, &wrote, NULL) ||
        wrote != content_size) {
      NOTREACHED() << "error writing";
      return false;
    }
  }
  return true;
}

void SessionBackend::ResetFile() {
  DCHECK(inited_);
  // Do Set(NULL) first to make sure we close current file (if open).
  current_session_handle_.Set(NULL);
  current_session_handle_.Set(OpenAndWriteHeader(GetCurrentSessionPath()));
  empty_file_ = true;
}

HANDLE SessionBackend::OpenAndWriteHeader(const std::wstring& path) {
  DCHECK(!path.empty());
  ScopedHandle hfile(
      CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL,
                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
  if (!hfile.IsValid())
    return NULL;
  int32 header[2];
  header[0] = kFileSignature;
  header[1] = kFileCurrentVersion;
  DWORD wrote;
  if (!WriteFile(hfile, &header, sizeof(header), &wrote, NULL) ||
      wrote != sizeof(header))
    return NULL;
  return hfile.Take();
}

std::wstring SessionBackend::GetLastSessionPath() {
  std::wstring path = path_to_dir_;
  file_util::AppendToPath(&path, kLastSessionFileName);
  return path;
}

std::wstring SessionBackend::GetSavedSessionPath() {
  std::wstring path = path_to_dir_;
  file_util::AppendToPath(&path, kSavedSessionFileName);
  return path;
}

std::wstring SessionBackend::GetCurrentSessionPath() {
  std::wstring path = path_to_dir_;
  file_util::AppendToPath(&path, kCurrentSessionFileName);
  return path;
}

