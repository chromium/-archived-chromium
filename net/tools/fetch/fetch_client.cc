// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/singleton.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_request_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_factory.h"

void usage(const char* program_name) {
  printf("usage: %s --url=<url>  [--n=<clients>] [--stats] [--use_cache]\n",
         program_name);
  exit(1);
}

// Test Driver
class Driver {
 public:
  Driver()
      : clients_(0) {}

  void ClientStarted() { clients_++; }
  void ClientStopped() {
    if (!--clients_) {
      MessageLoop::current()->Quit();
    }
  }

 private:
  int clients_;
};

// A network client
class Client {
 public:
  Client(net::HttpTransactionFactory* factory, const std::string& url) :
      url_(url),
      transaction_(factory->CreateTransaction()),
      buffer_(new net::IOBuffer(kBufferSize)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          connect_callback_(this, &Client::OnConnectComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &Client::OnReadComplete)) {
    buffer_->AddRef();
    driver_->ClientStarted();
    request_info_.url = url_;
    request_info_.method = "GET";
    int state = transaction_->Start(&request_info_, &connect_callback_);
    DCHECK(state == net::ERR_IO_PENDING);
  };

 private:
  void OnConnectComplete(int result) {
    // Do work here.
    int state = transaction_->Read(buffer_.get(), kBufferSize, &read_callback_);
    if (state == net::ERR_IO_PENDING)
      return;  // IO has started.
    if (state < 0)
      return;  // ERROR!
    OnReadComplete(state);
  }

  void OnReadComplete(int result) {
    if (result == 0) {
      OnRequestComplete(result);
      return;
    }

    // Deal with received data here.
    static StatsCounter bytes_read("FetchClient.bytes_read");
    bytes_read.Add(result);

    // Issue a read for more data.
    int state = transaction_->Read(buffer_.get(), kBufferSize, &read_callback_);
    if (state == net::ERR_IO_PENDING)
      return;  // IO has started.
    if (state < 0)
      return;  // ERROR!
    OnReadComplete(state);
  }

  void OnRequestComplete(int result) {
    static StatsCounter requests("FetchClient.requests");
    requests.Increment();
    driver_->ClientStopped();
    printf(".");
  }

  static const int kBufferSize = (16 * 1024);
  GURL url_;
  net::HttpRequestInfo request_info_;
  scoped_ptr<net::HttpTransaction> transaction_;
  scoped_ptr<net::IOBuffer> buffer_;
  net::CompletionCallbackImpl<Client> connect_callback_;
  net::CompletionCallbackImpl<Client> read_callback_;
  Singleton<Driver> driver_;
};

int main(int argc, char**argv) {
  base::AtExitManager exit;
  StatsTable table("fetchclient", 50, 1000);
  table.set_current(&table);

  CommandLine::Init(0, NULL);
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  std::string url = WideToASCII(parsed_command_line.GetSwitchValue(L"url"));
  if (!url.length())
    usage(argv[0]);
  int client_limit = 1;
  if (parsed_command_line.HasSwitch(L"n"))
    StringToInt(WideToASCII(parsed_command_line.GetSwitchValue(L"n")),
                &client_limit);
  bool use_cache = parsed_command_line.HasSwitch(L"use-cache");

  // Do work here.
  MessageLoop loop;

  net::HostResolver host_resolver;
  scoped_ptr<net::ProxyService> proxy_service(net::ProxyService::CreateNull());
  net::HttpTransactionFactory* factory = NULL;
  if (use_cache) {
    factory = new net::HttpCache(&host_resolver, proxy_service.get(), 0);
  } else {
    factory = new net::HttpNetworkLayer(
        net::ClientSocketFactory::GetDefaultFactory(), &host_resolver,
        proxy_service.get());
  }

  {
    StatsCounterTimer driver_time("FetchClient.total_time");
    StatsScope<StatsCounterTimer> scope(driver_time);

    Client** clients = new Client*[client_limit];
    for (int i = 0; i < client_limit; i++)
      clients[i] = new Client(factory, url);

    MessageLoop::current()->Run();
  }

  // Print Statistics here.
  int num_clients = table.GetCounterValue("c:FetchClient.requests");
  int test_time = table.GetCounterValue("t:FetchClient.total_time");
  int bytes_read = table.GetCounterValue("c:FetchClient.bytes_read");

  printf("\n");
  printf("Clients     : %d\n", num_clients);
  printf("Time        : %dms\n", test_time);
  printf("Bytes Read  : %d\n", bytes_read);
  if (test_time > 0) {
    const char *units = "bps";
    double bps = static_cast<float>(bytes_read * 8) /
        (static_cast<float>(test_time) / 1000.0);

    if (bps > (1024*1024)) {
      bps /= (1024*1024);
      units = "Mbps";
    } else if (bps > 1024) {
      bps /= 1024;
      units = "Kbps";
    }
    printf("Bandwidth   : %.2f%s\n", bps, units);
  }

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
  return 0;
}
