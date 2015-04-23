// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/stats_counters.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/winsock_init.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_request_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"
#include "net/tools/fetch/http_server.h"

void usage(const char* program_name) {
  printf("usage: %s\n", program_name);
  exit(-1);
}

int main(int argc, char**argv) {
  base::AtExitManager exit;
  StatsTable table("fetchserver", 50, 1000);
  table.set_current(&table);

#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif  // defined(OS_WIN)

  CommandLine::Init(0, NULL);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  // Do work here.
  MessageLoop loop;
  HttpServer server("", 80);   // TODO(mbelshe): make port configurable
  MessageLoop::current()->Run();

  if (parsed_command_line.HasSwitch(L"stats")) {
    // Dump the stats table.
    printf("<stats>\n");
    int counter_max = table.GetMaxCounters();
    for (int index=0; index < counter_max; index++) {
      std::string name(table.GetRowName(index));
      if (name.length() > 0) {
        int value = table.GetRowValue(index);
        printf("%s:\t%d\n", name.c_str(), value);
      }
    }
    printf("</stats>\n");
  }

}
