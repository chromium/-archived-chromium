// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_DOM_SERIALIZER_DELEGATE_H__
#define WEBKIT_GLUE_DOM_SERIALIZER_DELEGATE_H__

#include <string>

class GURL;

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
