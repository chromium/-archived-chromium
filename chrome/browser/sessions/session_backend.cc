// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_backend.h"

#include <limits>

#include "base/file_util.h"
#include "base/histogram.h"
#include "base/pickle.h"
#include "chrome/common/scoped_vector.h"

using base::TimeTicks;

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

  explicit SessionFileReader(const FilePath& path)
      : errored_(false),
        buffer_(SessionBackend::kFileReadBufferSize, 0),
        buffer_position_(0),
        available_count_(0) {
    file_.reset(new net::FileStream());
    file_->Open(path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  }
  // Reads the contents of the file specified in the constructor, returning
  // true on success. It is up to the caller to free all SessionCommands
  // added to commands.
  bool Read(BaseSessionService::SessionType type,
            std::vector<SessionCommand*>* commands);

 private:
  // Reads a single command, returning it. A return value of NULL indicates
  // either there are no commands, or there was an error. Use errored_ to
  // distinguish the two. If NULL is returned, and there is no error, it means
  // the end of file was successfully reached.
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
  scoped_ptr<net::FileStream> file_;

  // Position in buffer_ of the data.
  size_t buffer_position_;

  // Number of available bytes; relative to buffer_position_.
  size_t available_count_;

  DISALLOW_EVIL_CONSTRUCTORS(SessionFileReader);
};

bool SessionFileReader::Read(BaseSessionService::SessionType type,
                             std::vector<SessionCommand*>* commands) {
  if (!file_->IsOpen())
    return false;
  int32 header[2];
  int read_count;
  TimeTicks start_time = TimeTicks::Now();
  read_count = file_->ReadUntilComplete(reinterpret_cast<char*>(&header),
                                        sizeof(header));
  if (read_count != sizeof(header) || header[0] != kFileSignature ||
      header[1] != kFileCurrentVersion)
    return false;

  ScopedVector<SessionCommand> read_commands;
  SessionCommand* command;
  while ((command = ReadCommand()) && !errored_)
    read_commands->push_back(command);
  if (!errored_)
    read_commands->swap(*commands);
  if (type == BaseSessionService::TAB_RESTORE) {
    UMA_HISTOGRAM_TIMES("TabRestore.read_session_file_time",
                        TimeTicks::Now() - start_time);
  } else {
    UMA_HISTOGRAM_TIMES("SessionRestore.read_session_file_time",
                        TimeTicks::Now() - start_time);
  }
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
  int to_read = static_cast<int>(buffer_.size() - available_count_);
  int read_count = file_->ReadUntilComplete(&(buffer_[available_count_]),
                                            to_read);
  if (read_count < 0) {
    errored_ = true;
    return false;
  }
  if (read_count == 0)
    return false;
  available_count_ += read_count;
  return true;
}

}  // namespace

// SessionBackend -------------------------------------------------------------

// File names (current and previous) for a type of TAB.
static const char* kCurrentTabSessionFileName = "Current Tabs";
static const char* kLastTabSessionFileName = "Last Tabs";

// File names (current and previous) for a type of SESSION.
static const char* kCurrentSessionFileName = "Current Session";
static const char* kLastSessionFileName = "Last Session";

// static
const int SessionBackend::kFileReadBufferSize = 1024;

SessionBackend::SessionBackend(BaseSessionService::SessionType type,
                               const FilePath& path_to_dir)
    : type_(type),
      path_to_dir_(path_to_dir),
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

void SessionBackend::AppendCommands(
    std::vector<SessionCommand*>* commands,
    bool reset_first) {
  Init();
  // Make sure and check current_session_file_, if opening the file failed
  // current_session_file_ will be NULL.
  if ((reset_first && !empty_file_) || !current_session_file_.get() ||
      !current_session_file_->IsOpen()) {
    ResetFile();
  }
  // Need to check current_session_file_ again, ResetFile may fail.
  if (current_session_file_.get() && current_session_file_->IsOpen() &&
      !AppendCommandsToFile(current_session_file_.get(), *commands)) {
    current_session_file_.reset(NULL);
  }
  empty_file_ = false;
  STLDeleteElements(commands);
  delete commands;
}

void SessionBackend::ReadLastSessionCommands(
    scoped_refptr<BaseSessionService::InternalGetCommandsRequest> request) {
  if (request->canceled())
    return;
  Init();
  ReadLastSessionCommandsImpl(&(request->commands));
  request->ForwardResult(
      BaseSessionService::InternalGetCommandsRequest::TupleType(
          request->handle(), request));
}

bool SessionBackend::ReadLastSessionCommandsImpl(
    std::vector<SessionCommand*>* commands) {
  Init();
  SessionFileReader file_reader(GetLastSessionPath());
  return file_reader.Read(type_, commands);
}

void SessionBackend::DeleteLastSession() {
  Init();
  file_util::Delete(GetLastSessionPath(), false);
}

void SessionBackend::MoveCurrentSessionToLastSession() {
  Init();
  current_session_file_.reset(NULL);

  const FilePath current_session_path = GetCurrentSessionPath();
  const FilePath last_session_path = GetLastSessionPath();
  if (file_util::PathExists(last_session_path))
    file_util::Delete(last_session_path, false);
  if (file_util::PathExists(current_session_path)) {
    int64 file_size;
    if (file_util::GetFileSize(current_session_path, &file_size)) {
      if (type_ == BaseSessionService::TAB_RESTORE) {
        UMA_HISTOGRAM_COUNTS("TabRestore.last_session_file_size",
                             static_cast<int>(file_size / 1024));
      } else {
        UMA_HISTOGRAM_COUNTS("SessionRestore.last_session_file_size",
                             static_cast<int>(file_size / 1024));
      }
    }
    last_session_valid_ = file_util::Move(current_session_path,
                                          last_session_path);
  }

  if (file_util::PathExists(current_session_path))
    file_util::Delete(current_session_path, false);

  // Create and open the file for the current session.
  ResetFile();
}

bool SessionBackend::AppendCommandsToFile(net::FileStream* file,
    const std::vector<SessionCommand*>& commands) {
  for (std::vector<SessionCommand*>::const_iterator i = commands.begin();
       i != commands.end(); ++i) {
    int wrote;
    const size_type content_size = static_cast<size_type>((*i)->size());
    const size_type total_size =  content_size + sizeof(id_type);
    if (type_ == BaseSessionService::TAB_RESTORE)
      UMA_HISTOGRAM_COUNTS("TabRestore.command_size", total_size);
    else
      UMA_HISTOGRAM_COUNTS("SessionRestore.command_size", total_size);
    wrote = file->Write(reinterpret_cast<const char*>(&total_size),
                        sizeof(total_size), NULL);
    if (wrote != sizeof(total_size)) {
      NOTREACHED() << "error writing";
      return false;
    }
    id_type command_id = (*i)->id();
    wrote = file->Write(reinterpret_cast<char*>(&command_id),
                        sizeof(command_id), NULL);
    if (wrote != sizeof(command_id)) {
      NOTREACHED() << "error writing";
      return false;
    }
    wrote = file->Write(reinterpret_cast<char*>((*i)->contents()),
                        content_size, NULL);
    if (wrote != content_size) {
      NOTREACHED() << "error writing";
      return false;
    }
  }
  return true;
}

void SessionBackend::ResetFile() {
  DCHECK(inited_);
  if (current_session_file_.get()) {
    // File is already open, truncate it. We truncate instead of closing and
    // reopening to avoid the possibility of scanners locking the file out
    // from under us once we close it. If truncation fails, we'll try to
    // recreate.
    if (current_session_file_->Truncate(sizeof_header()) != sizeof_header())
      current_session_file_.reset(NULL);
  }
  if (!current_session_file_.get())
    current_session_file_.reset(OpenAndWriteHeader(GetCurrentSessionPath()));
  empty_file_ = true;
}

net::FileStream* SessionBackend::OpenAndWriteHeader(const FilePath& path) {
  DCHECK(!path.empty());
  net::FileStream* file = new net::FileStream();
  file->Open(path, base::PLATFORM_FILE_CREATE_ALWAYS |
             base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_EXCLUSIVE_WRITE |
             base::PLATFORM_FILE_EXCLUSIVE_READ);
  if (!file->IsOpen())
    return NULL;
  int32 header[2];
  header[0] = kFileSignature;
  header[1] = kFileCurrentVersion;
  int wrote = file->Write(reinterpret_cast<char*>(&header),
                          sizeof(header), NULL);
  if (wrote != sizeof_header())
    return NULL;
  return file;
}

FilePath SessionBackend::GetLastSessionPath() {
  FilePath path = path_to_dir_;
  if (type_ == BaseSessionService::TAB_RESTORE)
    path = path.AppendASCII(kLastTabSessionFileName);
  else
    path = path.AppendASCII(kLastSessionFileName);
  return path;
}

FilePath SessionBackend::GetCurrentSessionPath() {
  FilePath path = path_to_dir_;
  if (type_ == BaseSessionService::TAB_RESTORE)
    path = path.AppendASCII(kCurrentTabSessionFileName);
  else
    path = path.AppendASCII(kCurrentSessionFileName);
  return path;
}
