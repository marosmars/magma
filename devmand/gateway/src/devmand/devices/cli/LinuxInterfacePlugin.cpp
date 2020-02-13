// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#include <devmand/channels/cli/Cli.h>
#include <devmand/devices/cli/LinuxInterfacePlugin.h>
#include <devmand/devices/cli/StructuredLinuxDevice.h>
#include <devmand/devices/cli/schema/ModelRegistry.h>
#include <devmand/devices/cli/translation/GrpcReader.h>
#include <devmand/devices/cli/translation/GrpcListReader.h>
#include <devmand/devices/cli/translation/PluginRegistry.h>
#include <devmand/devices/cli/translation/ReaderRegistry.h>
#include <folly/executors/GlobalExecutor.h>
#include <folly/futures/Future.h>
#include <folly/json.h>
#include <unordered_map>

namespace devmand {
namespace devices {
namespace cli {

using namespace devmand::channels::cli;
using namespace std;
using namespace folly;
using namespace ydk;
using namespace httplib;

DeviceType LinuxInterfacePlugin::getDeviceType() const {
  return {"Linux", "*"};
}

void cli::LinuxInterfacePlugin::provideReaders(
    ReaderRegistryBuilder& reg) const {
  const string remotePluginId = "localhost:50051"; // TODO
  auto remotePluginChannel = grpc::CreateChannel(
      "localhost:50051", grpc::InsecureChannelCredentials());
  auto remoteReaderPlugin = make_shared<GrpcReader>(
      remotePluginChannel, remotePluginId, getCPUExecutor());
  auto remoteListReaderPlugin = make_shared<GrpcListReader>(
      remotePluginChannel, remotePluginId, getCPUExecutor());

  reg.addList(
      "/openconfig-interfaces:interfaces/interface", remoteListReaderPlugin);
  reg.add(
      "/openconfig-interfaces:interfaces/interface/config", remoteReaderPlugin);
  reg.add(
      "/openconfig-interfaces:interfaces/interface/state", remoteReaderPlugin);
}

void cli::LinuxInterfacePlugin::provideWriters(
    WriterRegistryBuilder& reg) const {
  (void)reg;
  // no writers yet
}

LinuxInterfacePlugin::LinuxInterfacePlugin(BindingContext& _openconfigContext)
    : openconfigContext(_openconfigContext) {}

} // namespace cli
} // namespace devices
} // namespace devmand
