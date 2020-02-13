// Copyright (c) 2019-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#pragma once

#include <devmand/channels/cli/Channel.h>
#include <folly/Optional.h>
#include <folly/dynamic.h>
#include <httplib.h>
//#include <devmand/channels/cli/datastore/DatastoreState.h>
#include <devmand/devices/cli/schema/Path.h>

namespace devmand {
namespace channels {
namespace cli {

using namespace std;
using namespace httplib;
using namespace folly;
using namespace devmand::channels::cli;
//using devmand::channels::cli::datastore::DatastoreType;
using devmand::devices::cli::Path;

typedef function<shared_ptr<Channel>(const string token)> CliResolver;
typedef function<dynamic
(const string token, bool configDS/*DatastoreType type*/, bool readCurrentTx, Path path)> TxResolver;

class CliHttpServer {
 private:
  string host;
  int port;

  Server svr;

 public:
  CliHttpServer(
      string host,
      int port,
      CliResolver cliResolver,
      TxResolver txResolver
      );

  bool is_running();

  bool listen();

  void stop();

};
}
}
}
