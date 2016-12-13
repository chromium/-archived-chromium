// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/utility_thread.h"

#include "base/file_util.h"
#include "base/values.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "chrome/common/child_process.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "chrome/common/render_messages.h"

UtilityThread::UtilityThread() : ChildThread(base::Thread::Options()) {
}

UtilityThread::~UtilityThread() {
}

void UtilityThread::Init() {
  ChildThread::Init();
  ChildProcess::current()->AddRefProcess();
}

void UtilityThread::CleanUp() {
  // Shutdown in reverse of the initialization order.
  ChildThread::CleanUp();
}

void UtilityThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(UtilityThread, msg)
    IPC_MESSAGE_HANDLER(UtilityMsg_UnpackExtension, OnUnpackExtension)
    IPC_MESSAGE_HANDLER(UtilityMsg_UnpackWebResource, OnUnpackWebResource)
  IPC_END_MESSAGE_MAP()
}

void UtilityThread::OnUnpackExtension(const FilePath& extension_path) {
  ExtensionUnpacker unpacker(extension_path);
  if (unpacker.Run() && unpacker.DumpImagesToFile()) {
    Send(new UtilityHostMsg_UnpackExtension_Succeeded(
         *unpacker.parsed_manifest()));
  } else {
    Send(new UtilityHostMsg_UnpackExtension_Failed(unpacker.error_message()));
  }

  ChildProcess::current()->ReleaseProcess();
}

void UtilityThread::OnUnpackWebResource(const std::string& resource_data) {
  // Parse json data.
  // TODO(mrc): Add the possibility of a template that controls parsing, and
  // the ability to download and verify images.
  WebResourceUnpacker unpacker(resource_data);
  if (unpacker.Run()) {
    Send(new UtilityHostMsg_UnpackWebResource_Succeeded(
        *unpacker.parsed_json()));
  } else {
    Send(new UtilityHostMsg_UnpackWebResource_Failed(
        unpacker.error_message()));
  }

  ChildProcess::current()->ReleaseProcess();
}

