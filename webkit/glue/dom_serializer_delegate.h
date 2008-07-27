// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WEBKIT_GLUE_DOM_SERIALIZER_DELEGATE_H__
#define WEBKIT_GLUE_DOM_SERIALIZER_DELEGATE_H__

#include <string>

#include "googleurl/src/gurl.h"

namespace webkit_glue {

// This class is used for providing sink interface that can be used to receive
// the individual chunks of data to be saved.
class DomSerializerDelegate {
 public:
  // This enum indicates  This sink interface can receive the individual chunks
  // of serialized data to be saved, so we use values of following enum
  // definition to indicate the serialization status of serializing all html
  // content. If current frame is not complete serialized, call
  // DidSerializeDataForFrame with URL of current frame, data, data length and
  // flag CURRENT_FRAME_IS_NOT_FINISHED.
  // If current frame is complete serialized, call DidSerializeDataForFrame
  // with URL of current frame, data, data length and flag
  // CURRENT_FRAME_IS_FINISHED.
  // If all frames of page are complete serialized, call
  // DidSerializeDataForFrame with empty URL, empty data, 0 and flag
  // ALL_FRAMES_ARE_FINISHED.
  enum PageSavingSerializationStatus {
    // Current frame is not finished saving.
    CURRENT_FRAME_IS_NOT_FINISHED = 0,
    // Current frame is finished saving.
    CURRENT_FRAME_IS_FINISHED,
    // All frame are finished saving.
    ALL_FRAMES_ARE_FINISHED,
  };

  // Receive the individual chunks of serialized data to be saved.
  // The parameter frame_url specifies what frame the data belongs. The
  // parameter data contains the available data for saving. The parameter
  // status indicates the status of data serialization.
  virtual void DidSerializeDataForFrame(const GURL& frame_url,
      const std::string& data, PageSavingSerializationStatus status) = 0;

  DomSerializerDelegate() { }
  virtual ~DomSerializerDelegate() { }
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_DOM_SERIALIZER_DELEGATE_H__
