// Copyright (c) 2016-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

//#include <devmand/channels/cli/CliHttpServer.h>
#include <devmand/channels/cli/IoConfigurationBuilder.h>
#include <devmand/channels/cli/datastore/Datastore.h>
#include <devmand/devices/Datastore.h>
#include <devmand/devices/cli/LinuxInterfacePlugin.h>
#include <devmand/devices/cli/StructuredLinuxDevice.h>
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

std::unique_ptr<devices::Device> StructuredLinuxDevice::createDevice(
    Application& app,
    const cartography::DeviceConfig& deviceConfig) {
  return createDeviceWithEngine(app, deviceConfig, app.getCliEngine());
}

unique_ptr<devices::Device> StructuredLinuxDevice::createDeviceWithEngine(
    Application& app,
    const cartography::DeviceConfig& deviceConfig,
    Engine& engine) {
  IoConfigurationBuilder ioConfigurationBuilder(deviceConfig, engine);
  auto cmdCache = ReadCachingCli::createCache();
  const std::shared_ptr<Channel>& channel = std::make_shared<Channel>(
      deviceConfig.id,
      ioConfigurationBuilder.createAll(
          cmdCache,
          make_shared<TreeCache>(
              ioConfigurationBuilder.getConnectionParameters()->flavour)));

  // TODO make configurable singleton
//  auto dummyTxResolver = [](const string token,
//                            bool configDS,
//                            bool readCurrentTx,
//                            Path path) -> dynamic {
//    (void)token;
//    (void)configDS;
//    (void)readCurrentTx;
//    (void)path;
//    throw runtime_error("Not implemented");
//  };
//  auto cliResolver = [channel](const string token) -> shared_ptr<Channel> {
//    if (token == "secret") {
//      return channel;
//    }
//    throw runtime_error("Wrong token");
//  };
//  shared_ptr<CliHttpServer> httpServer =
//      make_shared<CliHttpServer>("0.0.0.0", 4000, cliResolver, dummyTxResolver);
//  unique_ptr<std::thread> httpThread =
//      make_unique<std::thread>([httpServer]() { httpServer->listen(); });
//  while (not httpServer->is_running()) {
//    this_thread::sleep_for(chrono::milliseconds(10));
//  }

  PluginRegistry pReg;
  pReg.registerPlugin(make_shared<LinuxInterfacePlugin>(
      engine.getModelRegistry()->getBindingContext(Model::OPENCONFIG_2_4_3)));
  shared_ptr<DeviceContext> deviceCtx = pReg.getDeviceContext({"Linux", "*"});

  ReaderRegistryBuilder rRegBuilder{
      engine.getModelRegistry()->getSchemaContext(Model::OPENCONFIG_2_4_3)};
  deviceCtx->provideReaders(rRegBuilder);

  return std::make_unique<StructuredLinuxDevice>(
      app,
      deviceConfig.id,
      deviceConfig.readonly,
      channel,
      engine.getModelRegistry(),
      rRegBuilder.build(),
      cmdCache
//      ,
//      httpServer,
//      move(httpThread)
      );
}

StructuredLinuxDevice::StructuredLinuxDevice(
    Application& application,
    const Id id_,
    bool readonly_,
    const shared_ptr<Channel> _channel,
    const std::shared_ptr<ModelRegistry> _mreg,
    std::unique_ptr<ReaderRegistry>&& _rReg,
    const shared_ptr<CliCache> _cmdCache
//    ,
//    const std::shared_ptr<CliHttpServer> _httpServer,
//    std::unique_ptr<std::thread>&& _httpThread
    )
    : Device(application, id_, readonly_),
      channel(_channel),
      cmdCache(_cmdCache),
      mreg(_mreg),
      rReg(forward<unique_ptr<ReaderRegistry>>(_rReg)),
//      httpServer(_httpServer),
//      httpThread(forward<unique_ptr<std::thread>>(_httpThread)),
      operCache(make_unique<devmand::channels::cli::datastore::Datastore>(
          DatastoreType::operational,
          mreg->getSchemaContext(Model::OPENCONFIG_2_4_3))) {}

void StructuredLinuxDevice::setIntendedDatastore(const dynamic& config) {
  (void)config;
}

shared_ptr<Datastore> StructuredLinuxDevice::getOperationalDatastore() {
  MLOG(MINFO) << "[" << id << "] "
              << "Retrieving state";

  // Reset cache
  cmdCache->wlock()->clear();

  auto state = Datastore::make(*reinterpret_cast<MetricSink*>(&app), getId());
  state->setStatus(true);

  DeviceAccess access = DeviceAccess(channel, id, getCPUExecutor());

  state->addRequest(
      rReg->readState(Path::ROOT, access).thenValue([state, this](auto v) {
        auto tx = operCache->newTx();
        bool valid = true;
        string invalidMsg = "";
        try {
          if (!v.empty()) {
            tx->merge("/", v);
            valid = tx->isValid();
          }
        } catch (runtime_error& e) {
          MLOG(MWARNING) << "[" << id << "]" << " Invalid state returned: " << e.what();
          valid = false;
          invalidMsg = e.what();
        }
        tx->abort();

        if (valid) {
          state->update(
              [&v](auto& lockedState) { lockedState.merge_patch(v); });
        } else {
          state->addError("Invalid data according to model: " + invalidMsg);
        }
      }));

  return state;
}

} // namespace cli
} // namespace devices
} // namespace devmand
