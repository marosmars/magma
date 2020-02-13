#!/bin/bash

# This script regenerates cpp files from gRPC proto file,
# assuming docker/scripts/start_devmand_image.sh was executed and container is running.
# Passing argument 'clean' will clean the destination directory.

dirname="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
protoDir="/cache/devmand/repo/src/devmand/channels/cli/plugin/proto"
protoFile="${protoDir}/ReaderPlugin.proto"
protocppRelativeDir="/../protocpp"
protocppDir="${protoDir}/${protocppRelativeDir}"
hostProtocppDir="${dirname}/${protocppRelativeDir}"

if [ "$1" == "clean" ]; then
    # rm -rf
    docker exec --user $(id -u):$(id -g) 85 rm -rf "${protocppDir}"
    shift
fi
docker exec --user $(id -u):$(id -g) 85 mkdir -p "${protocppDir}"
docker exec --user $(id -u):$(id -g) 85 protoc -I "${protoDir}" --cpp_out="${protocppDir}" "${protoFile}"
docker exec --user $(id -u):$(id -g) 85 bash -c "protoc -I \"${protoDir}\" --grpc_out=\"${protocppDir}\" --plugin=protoc-gen-grpc=\`which grpc_cpp_plugin\` \"${protoFile}\""

# fix warnings:
# unused parameter options
find ${hostProtocppDir} -name '*.cc' | xargs sed -i "s/\(StubOptions& options.*\)/\1(void)options;/g"
# unused parameter deterministic
find ${hostProtocppDir} -name '*.cc' | xargs sed -i "s/\(bool deterministic.*\)/\1(void)deterministic;/g"
# casting to int
find ${hostProtocppDir} -name '*.cc' | xargs sed -i "s/[ ]\([^ ]\+\+length()\)/(int)\1/g"
# unused definition
find ${hostProtocppDir} -name '*.cc' | xargs sed -i "s/\(INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION.*\)/\1\n#ifdef INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION\n#endif/g"

rename 's/\.cc$/\.cpp/' ${hostProtocppDir}/*.cc

find ${hostProtocppDir} \( -name "*.[hc]pp" -or -name "*.h" \) -exec clang-format -i --style=file {} \;
