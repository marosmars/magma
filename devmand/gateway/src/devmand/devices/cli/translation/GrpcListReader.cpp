// Copyright (c) 2020-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#include <devmand/devices/cli/translation/GrpcListReader.h>
#include <folly/executors/GlobalExecutor.h> // TODO: remove, make async

namespace devmand {
namespace devices {
namespace cli {

using namespace folly;
using namespace std;
using namespace grpc;
using namespace devmand::channels::cli::plugin;
using namespace devmand::channels::cli;
using namespace std::chrono;

typedef high_resolution_clock clock;

// TODO rename to Grpc Reader Proxy
GrpcListReader::GrpcListReader(
    shared_ptr<grpc::Channel> channel,
    const string _id,
    shared_ptr<Executor> _executor)
    : stub_(devmand::channels::cli::plugin::ReaderPlugin::NewStub(channel)),
      id(_id),
      executor(_executor) {}

static ReadRequest handleCliRequest(
    const string& id,
    const DeviceAccess& device,
    const CliRequest& cliRequest,
    Executor* executor) {
  const ReadCommand& command = ReadCommand::create(cliRequest.cmd());
  MLOG(MDEBUG) << "[" << id << "] "
               << "Got cli request: " << command;
  string cliOutput = device.cli()->executeRead(command).via(executor).get();
  CliResponse* cliResponse = new CliResponse();
  cliResponse->set_output(cliOutput);
  ReadRequest readRequest;
  readRequest.set_allocated_cliresponse(cliResponse);
  return readRequest;
}

Future<vector<dynamic>> GrpcListReader::readKeys(
    const Path& path,
    const DeviceAccess& device) const {
  // TODO: reconnect, error handling - connection issues, wrong services
  // provided etc
  auto startTime = clock::now();
  long int spentInCliMillis = 0;

  ClientContext context;
  unique_ptr<ClientReaderWriter<ReadRequest, ReadResponse>> stream(
      stub_->Read(&context)); // TODO async
  if (not stream) {
    MLOG(MWARNING) << "[" << id << "] Cannot connect";
    return makeFuture<vector<dynamic>>(runtime_error("Cannot connect"));
  }
  {
    // send the request
    ActualReadRequest* arr = new ActualReadRequest();
    arr->set_path(path.str());
    ReadRequest readRequest;
    readRequest.set_allocated_actualreadrequest(arr);
    stream->Write(readRequest);
  }
  // start reading responses
  ReadResponse readResponse;

  while (stream->Read(&readResponse) && readResponse.has_clirequest()) {
    auto cliStartTime = clock::now();
    ReadRequest readRequest =
        handleCliRequest(id, device, readResponse.clirequest(), executor.get());
    spentInCliMillis +=
        (duration_cast<milliseconds>(clock::now() - cliStartTime)).count();
    stream->Write(readRequest);
  }
  // response received
  MLOG(MWARNING) << "[" << id << "] Got actual response: "
               << readResponse.actualreadresponse().json();
  auto totalMillis =
      (duration_cast<milliseconds>(clock::now() - startTime)).count();
  MLOG(MDEBUG) << "[" << id << "] Total duration: " << totalMillis << " ms, "
               << "in grpc: " << (totalMillis - spentInCliMillis) << " ms, "
               << "in cli " << spentInCliMillis << " ms";
  Status status = stream->Finish();
  if (status.ok()) {
    vector<dynamic> values;
    if (readResponse.actualreadresponse().json() == "") {
      return values;
    }

    auto responseDyn = parseJson(readResponse.actualreadresponse().json());
    if (responseDyn.find("keys") == responseDyn.items().end()) {
      return values;
    }

    if (!responseDyn["keys"].isArray()) {
      return values;
    }

    for (auto& k : responseDyn["keys"]) {
      values.push_back(k);
    }
    return makeFuture(values);
  } else {
    MLOG(MWARNING) << "[" << id << "] Error " << status.error_code() << ": "
                   << status.error_message();
    return makeFuture<vector<dynamic>>(runtime_error("Plugin restarting"));
  }
}

} // namespace cli
} // namespace devices
} // namespace devmand
