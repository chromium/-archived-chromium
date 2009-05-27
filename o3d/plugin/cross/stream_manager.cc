/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "plugin/cross/stream_manager.h"

#include <algorithm>
#include <map>

#include "base/logging.h"
#include "plugin/cross/o3d_glue.h"
#include "third_party/nixysa/files/static_glue/npapi/common.h"

namespace glue {

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StreamManager::StreamManager(NPP plugin_instance)
    : plugin_instance_(plugin_instance) {
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StreamManager::~StreamManager() {
  // If the destructor gets called while streams are in mid-flight then delete
  // them here to make sure we can garbage collect cleanly.
  std::vector<NPDownloadStream *>::iterator it;
  for (it = entries_.begin(); it < entries_.end(); ++it) {
    delete (*it);
  }
  entries_.clear();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
DownloadStream *StreamManager::LoadURL(const std::string &url,
                                       NewStreamCallback *new_stream_callback,
                                       WriteReadyCallback *write_ready_callback,
                                       WriteCallback *write_callback,
                                       FinishedCallback *finished_callback,
                                       uint16 stream_type) {
  DCHECK(finished_callback != NULL);

  NPDownloadStream *entry = new NPDownloadStream(url,
                                                 "",
                                                 stream_type,
                                                 plugin_instance_,
                                                 new_stream_callback,
                                                 write_ready_callback,
                                                 write_callback,
                                                 finished_callback);

  GLUE_PROFILE_START(plugin_instance_, "geturlnotify");
  // NPN_GetURLNotify may call-back into the plug-in before returning, so
  // add the download stream entry before making the call.
  entries_.push_back(entry);
  NPError ret = NPN_GetURLNotify(plugin_instance_, url.c_str(), NULL, entry);
  GLUE_PROFILE_STOP(plugin_instance_, "geturlnotify");
  if (ret != NPERR_NO_ERROR) {
    // If the operation failed, it's possible that the browser hosting
    // environment did not call the appropriate notify routines which
    // clean up the entries_ stack.  We check if the entry is still
    // at the top, and delete it here.
    if (!entries_.empty() && entries_.back() == entry) {
      entries_.pop_back();
      // NOTE: Should we run the finished_callback here ?
      delete entry;
      entry = NULL;
    }
  }
  return entry;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::CheckEntry(NPDownloadStream *entry) {
  std::vector<NPDownloadStream *>::iterator it =
      std::find(entries_.begin(), entries_.end(), entry);
  return it != entries_.end();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::NewStream(NPStream *stream, uint16 *stype) {
  NPDownloadStream *entry = static_cast<NPDownloadStream*>(stream->notifyData);
  if (!CheckEntry(entry)) {
    // We got a new stream, but we don't know about it.
    return false;
  }
  return entry->NewStream(stream, stype);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::DestroyStream(NPStream *stream, NPReason reason) {
  NPDownloadStream *entry = static_cast<NPDownloadStream*>(stream->notifyData);
  if (!CheckEntry(entry)) {
    // We don't know about this stream.
    return false;
  }
  DCHECK_EQ(stream, entry->GetStream());

  return entry->DestroyStream(reason);
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::SetStreamFile(NPStream *stream, const char *filename) {
  NPDownloadStream *entry = static_cast<NPDownloadStream*>(stream->notifyData);
  if (!CheckEntry(entry)) {
    // We don't know about this stream.
    return false;
  }
  DCHECK_EQ(stream, entry->GetStream());

  return entry->SetStreamFile(filename);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::URLNotify(const char *url,
                              NPReason reason,
                              void *notifyData) {
  NPDownloadStream *entry = static_cast<NPDownloadStream *>(notifyData);
  if (!CheckEntry(entry)) {
    // We don't know about this stream.
    return false;
  }

  std::vector<NPDownloadStream *>::iterator it =
      std::find(entries_.begin(), entries_.end(), entry);
  DCHECK(it != entries_.end());
  entries_.erase(it);

  bool result = entry->URLNotify(reason);
  delete entry;
  return result;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 StreamManager::WriteReady(NPStream *stream) {
  NPDownloadStream *entry = static_cast<NPDownloadStream*>(stream->notifyData);
  if (!CheckEntry(entry)) {
    // We don't know about this stream.
    return 0;
  }

  return entry->WriteReady();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 StreamManager::Write(NPStream *stream,
                           int32 offset,
                           int32 len,
                           void *buffer) {
  NPDownloadStream *entry = static_cast<NPDownloadStream*>(stream->notifyData);
  if (!CheckEntry(entry)) {
    // We don't know about this stream.
    return 0;
  }

  return entry->Write(offset, len, buffer);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DownloadStream implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StreamManager::NPDownloadStream::~NPDownloadStream() {
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::string StreamManager::NPDownloadStream::GetURL() {
  return url_;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::string StreamManager::NPDownloadStream::GetCachedFile() {
  return file_;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
DownloadStream::State StreamManager::NPDownloadStream::GetState() {
  return state_;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int StreamManager::NPDownloadStream::GetReceivedByteCount() {
  return bytes_received_;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
size_t StreamManager::NPDownloadStream::GetStreamLength() {
  return stream_ ? stream_->end : 0;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void StreamManager::NPDownloadStream::Cancel() {
  NPN_DestroyStream(plugin_instance_, stream_, NPRES_USER_BREAK);
  state_ = STREAM_FINISHED;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// NPDownloadStream implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

typedef std::map<std::string, std::string> StringMap;

// Extracts headers from the browser-returned header string, as a name->value
// map.
static StringMap ExtractHeaders(const char *header_string) {
  using std::string;
  StringMap map;
  if (!header_string) return map;
  string headers = header_string;
  // Doc says headers as returned by the browser are LF-terminated, including
  // the last one.
  // It's unclear if they are rewritten by the browser to be in a "canonical"
  // form (i.e. single-line, no extra space etc.). We currently assume that
  // they are.
  // TODO: verify this, and/or implement correct parsing to handle RFC
  // 1945/2616 parsing.
  string::size_type index = 0;
  while (index < headers.size()) {
    string::size_type eol = std::min(headers.size(), headers.find("\n", index));
    string line = headers.substr(index, eol-index);
    string::size_type separator = line.find(":");
    if (separator != string::npos) {
      string key = line.substr(0, separator);
      string value;
      if (separator + 1 < line.size()) {
        string::size_type value_index =
            line.find_first_not_of(" \t", separator + 1);
        if (value_index != string::npos)
          value = line.substr(value_index);
      }
      map[key] = value;
    }
    index = eol + 1;
  }

  return map;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::NPDownloadStream::NewStream(NPStream *new_stream,
                                                uint16 *stype) {
  stream_ = new_stream;
  state_ = DownloadStream::STREAM_STARTED;
  *stype = stream_type_;

  // callback if provided
  if (new_stream_callback_.get()) {
    new_stream_callback_->Run(this);
  }

  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::NPDownloadStream::DestroyStream(NPReason reason) {
  stream_ = NULL;
  state_ = DownloadStream::STREAM_FINISHED;
  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::NPDownloadStream::SetStreamFile(const char *filename) {
  if (finished_callback_.get()) {
    StringMap header_map = ExtractHeaders(stream_->headers);
    std::string mime_type = header_map["Content-Type"];
    file_ = filename;
    // On success, run the finished_callback.
    if (file_.size() != 0) {
      // finished_callback should only be called once.
      finished_callback_->Run(this, true, file_, mime_type);
      finished_callback_.reset(NULL);
    }
  }

  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StreamManager::NPDownloadStream::URLNotify(NPReason reason) {
  if (finished_callback_.get()) {
    // On failure, run the finished_callback.
    // Note that the streaming case (NP_NORMAL) does not get a file
    // so we can't check its size
    if ((reason != NPRES_DONE) ||
        (stream_type_ != NP_NORMAL && file_.size() == 0)) {
      // finished_callback should only be called once.
      finished_callback_->Run(this, false, "", "");
      finished_callback_.reset(NULL);
    }

    // Finished callback for streaming case
    // where SetStreamFile() is not called
    if (reason == NPRES_DONE && stream_type_ == NP_NORMAL) {
      finished_callback_->Run(this, true, "", "");
      finished_callback_.reset(NULL);
    }
  }

  new_stream_callback_.reset(NULL);
  write_ready_callback_.reset(NULL);
  write_callback_.reset(NULL);

  return true;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 StreamManager::NPDownloadStream::WriteReady() {
  if (write_ready_callback_.get()) {
    return write_ready_callback_->Run(this);
  }

  return 4096;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int32 StreamManager::NPDownloadStream::Write(int32 offset,
                                             int32 len,
                                             void *buffer) {
  if (write_callback_.get()) {
    int32 n = write_callback_->Run(this, offset, len, buffer);
    bytes_received_ += n;
    return n;
  }

  return len;
}

}  // namespace glue
