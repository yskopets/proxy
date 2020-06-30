/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "benchmark/benchmark.h"
#include "common/stream_info/filter_state_impl.h"
#include "extensions/common/node_info_generated.h"
#include "extensions/common/proto_util.h"
#include "extensions/common/wasm/wasm_state.h"
#include "google/protobuf/util/json_util.h"

// WASM_PROLOG
#ifdef NULL_PLUGIN
namespace Wasm {
#endif  // NULL_PLUGIN

// END WASM_PROLOG

namespace Common {

using namespace google::protobuf::util;

using ::Envoy::Extensions::Common::Wasm::WasmStatePrototype;
using ::Envoy::Extensions::Common::Wasm::WasmStatePrototypeConstSharedPtr;

constexpr absl::string_view node_metadata_json = R"###(
{
   "NAME":"test_pod",
   "NAMESPACE":"test_namespace",
   "LABELS": {
      "app": "productpage",
      "version": "v1",
      "pod-template-hash": "84975bc778"
   },
   "OWNER":"test_owner",
   "WORKLOAD_NAME":"test_workload",
   "PLATFORM_METADATA":{
      "gcp_project":"test_project",
      "gcp_cluster_location":"test_location",
      "gcp_cluster_name":"test_cluster"
   },
   "ISTIO_VERSION":"istio-1.4",
   "MESH_ID":"test-mesh"
}
)###";

constexpr absl::string_view metadata_id_key =
    "envoy.wasm.metadata_exchange.downstream_id";
constexpr absl::string_view metadata_key =
    "envoy.wasm.metadata_exchange.downstream";
constexpr absl::string_view node_id = "test_pod.test_namespace";

static void setData(Envoy::StreamInfo::FilterStateImpl& filter_state,
                    absl::string_view key, absl::string_view value) {
  WasmStatePrototypeConstSharedPtr prototype = std::make_shared<const WasmStatePrototype>();
  auto state_ptr =
      std::make_unique<Envoy::Extensions::Common::Wasm::WasmState>(prototype);
  state_ptr->setValue(value);
  filter_state.setData(key, std::move(state_ptr),
                       Envoy::StreamInfo::FilterState::StateType::Mutable);
}

static const std::string& getData(
    Envoy::StreamInfo::FilterStateImpl& filter_state, absl::string_view key) {
  return filter_state
      .getDataReadOnly<Envoy::Extensions::Common::Wasm::WasmState>(key)
      .value();
}

static void BM_ReadFlatBuffer(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  flatbuffers::FlatBufferBuilder fbb(1024);
  extractNodeFlatBuffer(metadata_struct, fbb);

  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};
  setData(
      filter_state, metadata_key,
      absl::string_view(reinterpret_cast<const char*>(fbb.GetBufferPointer()),
                        fbb.GetSize()));

  size_t size = 0;
  for (auto _ : state) {
    auto buf = getData(filter_state, metadata_key);
    auto peer = flatbuffers::GetRoot<FlatNode>(buf.data());
    size += peer->workload_name()->size() + peer->namespace_()->size() +
            peer->labels()->LookupByKey("app")->value()->size() +
            peer->labels()->LookupByKey("version")->value()->size();
    benchmark::DoNotOptimize(size);
  }
}
BENCHMARK(BM_ReadFlatBuffer);

static void BM_WriteRawBytes(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  auto bytes = metadata_struct.SerializeAsString();
  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};

  for (auto _ : state) {
    setData(filter_state, metadata_id_key, node_id);
    setData(filter_state, metadata_key, bytes);
  }
}
BENCHMARK(BM_WriteRawBytes);

static void BM_WriteFlatBufferWithCache(benchmark::State& state) {
  google::protobuf::Struct metadata_struct;
  JsonParseOptions json_parse_options;
  JsonStringToMessage(std::string(node_metadata_json), &metadata_struct,
                      json_parse_options);
  auto bytes = metadata_struct.SerializeAsString();
  Envoy::StreamInfo::FilterStateImpl filter_state{
      Envoy::StreamInfo::FilterState::LifeSpan::TopSpan};

  std::unordered_map<std::string, std::string> cache;

  for (auto _ : state) {
    // lookup cache by key
    auto nodeinfo_it = cache.find(std::string(node_id));
    std::string node_info;
    if (nodeinfo_it == cache.end()) {
      google::protobuf::Struct test_struct;
      test_struct.ParseFromArray(bytes.data(), bytes.size());
      benchmark::DoNotOptimize(test_struct);

      flatbuffers::FlatBufferBuilder fbb;
      extractNodeFlatBuffer(test_struct, fbb);

      node_info =
          cache
              .emplace(node_id, std::string(reinterpret_cast<const char*>(
                                                fbb.GetBufferPointer()),
                                            fbb.GetSize()))
              .first->second;
    } else {
      node_info = nodeinfo_it->second;
    }

    setData(filter_state, metadata_id_key, node_id);
    setData(filter_state, metadata_key, node_info);
  }
}
BENCHMARK(BM_WriteFlatBufferWithCache);

}  // namespace Common

// WASM_EPILOG
#ifdef NULL_PLUGIN
}  // namespace Wasm
#endif

// Boilerplate main(), which discovers benchmarks in the same file and runs
// them.
int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  benchmark::RunSpecifiedBenchmarks();
}
