// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#include <devmand/channels/cli/IoConfigurationBuilder.h>
#include <devmand/devices/Datastore.h>
#include <devmand/devices/cli/StructuredUbntDevice2.h>
#include <devmand/devices/cli/UbntInterfacePlugin.h>
#include <devmand/devices/cli/schema/ModelRegistry.h>
#include <devmand/devices/cli/translation/PluginRegistry.h>
#include <devmand/devices/cli/translation/ReaderRegistry.h>
#include <folly/json.h>
#include <memory>

namespace devmand {
namespace devices {
namespace cli {

using namespace devmand::channels::cli;
using namespace std;
using namespace folly;

std::unique_ptr<devices::Device> StructuredUbntDevice2::createDevice(
    Application& app,
    const cartography::DeviceConfig& deviceConfig) {
  return createDeviceWithEngine(app, deviceConfig, app.getCliEngine());
}

unique_ptr<devices::Device> StructuredUbntDevice2::createDeviceWithEngine(
    Application& app,
    const cartography::DeviceConfig& deviceConfig,
    Engine& engine) {
  IoConfigurationBuilder ioConfigurationBuilder(deviceConfig, engine);
  auto cmdCache = ReadCachingCli::createCache();
  const std::shared_ptr<Channel>& channel = std::make_shared<Channel>(
      deviceConfig.id, ioConfigurationBuilder.createAll(cmdCache));

  PluginRegistry pReg;
  pReg.registerPlugin(make_shared<UbntInterfacePlugin>(
      engine.getModelRegistry()->getBindingContext(Model::OPENCONFIG_0_1_6)));
  shared_ptr<DeviceContext> deviceCtx = pReg.getDeviceContext({"ubnt", "*"});

  ReaderRegistryBuilder rRegBuilder{
      engine.getModelRegistry()->getSchemaContext(Model::OPENCONFIG_0_1_6)};
  deviceCtx->provideReaders(rRegBuilder);

  return std::make_unique<StructuredUbntDevice2>(
      app,
      deviceConfig.id,
      deviceConfig.readonly,
      channel,
      engine.getModelRegistry(),
      rRegBuilder.build(),
      cmdCache);
}

StructuredUbntDevice2::StructuredUbntDevice2(
    Application& application,
    const Id id_,
    bool readonly_,
    const shared_ptr<Channel> _channel,
    const std::shared_ptr<ModelRegistry> _mreg,
    std::unique_ptr<ReaderRegistry>&& _rReg,
    const shared_ptr<CliCache> _cmdCache)
    : Device(application, id_, readonly_),
      channel(_channel),
      cmdCache(_cmdCache),
      mreg(_mreg),
      rReg(forward<unique_ptr<ReaderRegistry>>(_rReg)) {}

void StructuredUbntDevice2::setIntendedDatastore(const dynamic& config) {
  (void)config;
}

shared_ptr<Datastore> StructuredUbntDevice2::getOperationalDatastore() {
  MLOG(MINFO) << "[" << id << "] "
              << "Retrieving state";

  // Reset cache
  cmdCache->wlock()->clear();

  auto state = Datastore::make(*reinterpret_cast<MetricSink*>(&app), getId());
  state->setStatus(true);

  DeviceAccess access = DeviceAccess(channel, id);

  state->addRequest(
      rReg->readState(Path::ROOT, access).thenValue([state](auto v) {
        state->update([&v](auto& lockedState) { lockedState.merge_patch(v); });
      }));

  return state;
}

} // namespace cli
} // namespace devices
} // namespace devmand
