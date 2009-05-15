// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_function.h"

#include "base/json_reader.h"
#include "base/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"

void AsyncExtensionFunction::SetArgs(const std::string& args) {
  DCHECK(!args_);  // should only be called once
  if (!args.empty()) {
    JSONReader reader;
    args_ = reader.JsonToValue(args, false, false);

    // Since we do the serialization in the v8 extension, we should always get
    // valid JSON.
    if (!args_) {
      DCHECK(false);
      return;
    }
  }
}

const std::string AsyncExtensionFunction::GetResult() {
  std::string json;
  JSONWriter::Write(result_.get(), false, &json);
  return json;
}

void AsyncExtensionFunction::SendResponse(bool success) {
  if (bad_message_) {
    dispatcher_->HandleBadMessage(this);
  } else {
    dispatcher_->SendResponse(this, success);
  }
}

std::string AsyncExtensionFunction::extension_id() {
  return dispatcher_->extension_id();
}

Profile* AsyncExtensionFunction::profile() {
  return dispatcher_->profile();
}
