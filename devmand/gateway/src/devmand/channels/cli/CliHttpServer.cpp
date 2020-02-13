// Copyright (c) 2019-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#include <devmand/channels/cli/CliHttpServer.h>
#include <folly/json.h>
#include <devmand/error/ErrorHandler.h>

namespace devmand {
namespace channels {
namespace cli {

// TODO check Content-Type: application/json in request

struct CliParams {
  bool write;
  string cmd;

  CliParams(const Request& req) {
    string token = req.matches[1];
    dynamic params = parseJson(req.body);
    if (not params["cmd"].isString()) {
      MLOG(MDEBUG) << "[" << token << "] params.cmd not found";
      throw runtime_error("params.cmd not found");
    }
    cmd = params["cmd"].asString();
    write = params["write"].isBool() && params["write"].asBool();
  }
};

static void executeCliCommand(CliResolver cliResolver,
                              const Request& req, Response& res) {
  string token = req.matches[1];
  try {
    res.status = 400;
    CliParams params(req);
    res.status = 500;
    shared_ptr<Cli> cli = cliResolver(token);
    MLOG(MDEBUG) << "[" << token << "] Obtaining cli succeeded, request: " << req.body;
    string content;
    if (params.write) {
      WriteCommand writeCmd = WriteCommand::create(params.cmd, false);
      content = cli->executeWrite(writeCmd).get();
    } else {
      ReadCommand readCmd = ReadCommand::create(params.cmd, false);
      content = cli->executeRead(readCmd).get();
    }
    MLOG(MDEBUG) << "[" << token << "] CLI Response: " << content;
    res.set_content(content, "application/plain");
    res.status = 200;
  } catch (const std::exception& e) {
    MLOG(MWARNING) << "Error " << e.what();
    dynamic response = dynamic::object;
    dynamic error = dynamic::object;
    error["code"] = 1;
    error["message"] = e.what();
    response["error"] = error;
    string content = toJson(response);
    MLOG(MDEBUG) << "[" << token << "] CLI Response: " << content;
    res.set_content(content, "application/json");
  }
}

struct TxParams {
//  DatastoreType dsType;
  bool configDS;
  bool readCurrentTx;
  Path path = Path::ROOT;

  TxParams(const Request& req) {
    string token = req.matches[1];
    dynamic params = parseJson(req.body);
    if (not params["path"].isString()) {
      MLOG(MDEBUG) << "[" << token << "] params.path not found";
      throw runtime_error("params.path not found");
    }
    configDS = params["readFromConfigDS"].isBool() && params["readFromConfigDS"].asBool();
//    dsType = configDS? DatastoreType::config: DatastoreType::operational;
    readCurrentTx = params["readCurrentTx"].isBool() && params["readCurrentTx"].asBool();
    path = Path(params["path"].asString());
  }
};

static void executeDSRead(TxResolver txResolver, const Request& req, Response& res) {
  string token = req.matches[1];
  dynamic response = dynamic::object;
  try {
    res.status = 400;
    TxParams params(req);
    res.status = 500;
    response = txResolver(token, params.configDS/*dsType*/, params.readCurrentTx, params.path);
    res.status = 200;
  } catch (const std::exception& e) {
    MLOG(MWARNING) << "Error " << e.what();
    dynamic error = dynamic::object;
    error["code"] = 1;
    error["message"] = e.what();
    response["error"] = error;
  }
  string content = toJson(response);
  MLOG(MDEBUG) << "[" << token << "] Response: " << content;
  res.set_content(content, "application/json");
}

CliHttpServer::CliHttpServer(
    string _host,
    int _port,
    CliResolver cliResolver,
    TxResolver txResolver
    ) : host(_host), port(_port) {

  svr.Post(R"(^/executeCliCommand/([^/]+)$)", [cliResolver](const Request& req, Response& res) {
    executeCliCommand(cliResolver, req, res);
  });
  svr.Post(R"(^/executeDSRead/([^/]+)$)", [txResolver](const Request& req, Response& res) {
    executeDSRead(txResolver, req, res);
  });
}

bool CliHttpServer::listen() {
  return svr.listen(host.c_str(), port);
}

void CliHttpServer::stop() {
  svr.stop();
}

bool CliHttpServer::is_running() {
  return svr.is_running();
}

}
}
}
