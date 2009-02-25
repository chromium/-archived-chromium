// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "base/logging.h"
#include "base/string_util.h"

ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir) {
  socket_path_ = user_data_dir.Append("Singleton Socket");
}

ProcessSingleton::~ProcessSingleton() {
}

bool ProcessSingleton::NotifyOtherProcess() {
  int sock;
  sockaddr_un addr;
  SetupSocket(&sock, &addr);

  if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 &&
      (errno == ENOENT || errno == ECONNREFUSED)) {
    return false;  // Tell the caller there's nobody to notify.
  }

  // TODO(port): pass in info to the other process.
  // http://code.google.com/p/chromium/issues/detail?id=8073
  NOTIMPLEMENTED() << " don't know how to notify other process about us.";

  return true;  // We did our best, so we die here.
}

void ProcessSingleton::Create() {
  int sock;
  sockaddr_un addr;
  SetupSocket(&sock, &addr);

  if (unlink(socket_path_.value().c_str()) < 0)
    DCHECK_EQ(errno, ENOENT);

  if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    LOG(ERROR) << "bind() failed: " << strerror(errno);

  if (listen(sock, 5) < 0)
    NOTREACHED() << "listen failed: " << strerror(errno);

  // TODO(port): register this socket as something we care about getting
  // input on, process messages, etc.
  // http://code.google.com/p/chromium/issues/detail?id=8073
}

void ProcessSingleton::HuntForZombieChromeProcesses() {
  // On Windows, this examines all the chrome.exe processes to see if one
  // is hung.  TODO(port): should we do anything here?
  // http://code.google.com/p/chromium/issues/detail?id=8073
}

void ProcessSingleton::SetupSocket(int* sock, struct sockaddr_un* addr) {
  *sock = socket(PF_UNIX, SOCK_STREAM, 0);
  if (*sock < 0)
    LOG(FATAL) << "socket() failed: " << strerror(errno);

  addr->sun_family = AF_UNIX;
  base::strlcpy(addr->sun_path, socket_path_.value().c_str(),
                sizeof(addr->sun_path));
}
