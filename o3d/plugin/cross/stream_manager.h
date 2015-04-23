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


#ifndef EXPERIMENTAL_O3D_O3DPLUGIN_AUTOGEN_O3D_GLUE_STREAM_MANAGER_H_
#define EXPERIMENTAL_O3D_O3DPLUGIN_AUTOGEN_O3D_GLUE_STREAM_MANAGER_H_

#include <npapi.h>
#include <string>
#include <vector>
#include "base/scoped_ptr.h"
#include "core/cross/callback.h"
#include "plugin/cross/download_stream.h"

namespace glue {

// Stream manager class, to help managing asynchronous loading of URLs into
// files.
class StreamManager {
 public:
  // TODO : get rid of these horrible callback objects
  // and use an interface instead
  typedef o3d::Callback1<DownloadStream*> NewStreamCallback;

  typedef o3d::Callback4<DownloadStream*,
                         bool,
                         const std::string &,
                         const std::string &> FinishedCallback;

  // The signature for these callbacks corresponds to the NPP_WriteReady()
  // and NPP_Write() calls
  typedef o3d::ResultCallback1<int32, DownloadStream*> WriteReadyCallback;

  typedef o3d::ResultCallback4<int32,
                               DownloadStream*,
                               int32,
                               int32,
                               void*> WriteCallback;

  explicit StreamManager(NPP plugin_instance);
  ~StreamManager();

  // Loads URL into a file, calls finished_callback->Run(success, filename)
  // when done.
  //  returns a DownloadStream object (or NULL) if error
  //   filename is the name of the file where the contents of the URL are
  //     stored.
  // LoadURL() takes ownership of new_stream_callback, write_ready_callback,
  // write_callback, and callback:  They will be deleted once the stream has
  // completed.
  DownloadStream *LoadURL(const std::string &url,
                          NewStreamCallback *new_stream_callback,
                          WriteReadyCallback *write_ready_callback,
                          WriteCallback *write_callback,
                          FinishedCallback *callback,
                          uint16 stream_type);

  // Manages the NPP_NewStream callback.
  bool NewStream(NPStream *stream, uint16 *stype);
  // Manages the NPP_DestroyStream callback.
  bool DestroyStream(NPStream *stream, NPReason reason);
  // Manages the NPP_StreamAsFile callback.
  bool SetStreamFile(NPStream *stream, const char *filename);
  // Manages the NPP_URLNotify callback.
  bool URLNotify(const char *url, NPReason reason, void *notifyData);

  // Continuous streaming support
  int32 WriteReady(NPStream *stream);
  int32 Write(NPStream *stream, int32 offset, int32 len, void *buffer);

 private:
  class NPDownloadStream : public DownloadStream {
   public:
    NPDownloadStream(const std::string &url,
                     const std::string &file,
                     uint16 stream_type,
                     NPP plugin_instance,
                     NewStreamCallback *new_stream_callback,
                     WriteReadyCallback *write_ready_callback,
                     WriteCallback *write_callback,
                     FinishedCallback *callback)
        : url_(url),
          file_(file),
          stream_type_(stream_type),
          plugin_instance_(plugin_instance),
          stream_(NULL),
          new_stream_callback_(new_stream_callback),
          write_ready_callback_(write_ready_callback),
          write_callback_(write_callback),
          finished_callback_(callback),
          bytes_received_(0),
          state_(STREAM_REQUESTED) {}

    virtual ~NPDownloadStream();

    // DownloadStream interface
    virtual std::string   GetURL();
    virtual std::string   GetCachedFile();
    virtual State         GetState();
    virtual int           GetReceivedByteCount();
    virtual size_t        GetStreamLength();
    virtual void          Cancel();

    // NPAPI stream stuff
    NPStream              *GetStream() const { return stream_; }
    bool                  NewStream(NPStream *new_stream, uint16 *stype);
    bool                  DestroyStream(NPReason reason);
    bool                  SetStreamFile(const char *filename);
    bool                  URLNotify(NPReason reason);
    int32                 WriteReady();
    int32                 Write(int32 offset, int32 len, void *buffer);


   private:
    std::string url_;
    std::string file_;

    // stream type (as file or continuous stream)
    uint16 stream_type_;

    NPP plugin_instance_;
    NPStream *stream_;

    // callbacks
    scoped_ptr<NewStreamCallback> new_stream_callback_;
    scoped_ptr<WriteReadyCallback> write_ready_callback_;
    scoped_ptr<WriteCallback> write_callback_;
    scoped_ptr<FinishedCallback> finished_callback_;

    int bytes_received_;
    State state_;
  };
  bool CheckEntry(NPDownloadStream *entry);

  NPP plugin_instance_;
  std::vector<NPDownloadStream *> entries_;
};

}  // namespace glue

#endif  // EXPERIMENTAL_O3D_O3DPLUGIN_AUTOGEN_O3D_GLUE_STREAM_MANAGER_H_
