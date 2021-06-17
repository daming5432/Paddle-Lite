// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lite/kernels/rknpu/bridges/graph.h"
#include <rknpu/graph.h>
#include "lite/kernels/rknpu/bridges/utility.h"

namespace paddle {
namespace lite {
namespace subgraph {
namespace rknpu {

int Graph::Add(const std::string& name, std::shared_ptr<Node> node) {
  auto it = nodes_.find(name);
  if (it != nodes_.end()) {
    // Only variable node can be shared with the same name
    if (!node->is_var() || !it->second.back()->is_var()) {
      LOG(FATAL) << "[Rockchip NPU] Const or data node " << name
                 << " is redefined.";
      return -1;
    }
  } else {
    auto ret = nodes_.insert(
        std::make_pair(name, std::vector<std::shared_ptr<Node>>()));
    CHECK(ret.second);
    it = ret.first;
  }
  it->second.push_back(node);
  return it->second.size();
}

// Const or data node
std::shared_ptr<Node> Graph::Add(const std::string& name,
                                 const Tensor& tensor,
                                 std::vector<int64_t> shape,
                                 PrecisionType precision,
                                 DataLayoutType layout,
                                 const QuantizationInfo& qnt) {
  std::shared_ptr<Node> node = nullptr;
  if (precision == PrecisionType::kUnk) {
    precision = tensor.precision();  // todo
  }
  if (precision == PrecisionType::kUnk) {
    if (qnt.enable_int8 && qnt.quant_bits == 8) {
      precision = PrecisionType::kInt8;
    } else if (!qnt.enable_int8) {
      precision = PrecisionType::kFloat;
    } else {
      LOG(ERROR) << "[Rockchip NPU] Unsupported precision type("
                 << PrecisionToStr(precision) << ")!";
    }
  }
  if (precision != tensor.precision()) {
    LOG(WARNING) << "[Rockchip NPU] The precision type("
                 << PrecisionToStr(tensor.precision()) << ") of tensor " << name
                 << " is not matched with the requested one("
                 << PrecisionToStr(precision) << ")!";
  }
  if (tensor.persistable()) {
    // Const node
    VLOG(5) << "[Rockchip NPU] Add Const node " << name;
    node = std::make_shared<Node>(precision, layout, Node::Role::kConst);
    auto idx = Add(name, node);
    CHECK_EQ(idx, 1);
    node->set_data(CvtTensor(graph_,
                             name,
                             shape,
                             qnt.scale,
                             const_cast<void*>(tensor.raw_data()),
                             precision,
                             layout));
  } else {
    // Data node
    node = Add(name, shape, precision, layout, qnt);
  }
  return node;
}

// Data node
std::shared_ptr<Node> Graph::Add(const std::string& name,
                                 std::vector<int64_t> shape,
                                 PrecisionType precision,
                                 DataLayoutType layout,
                                 const QuantizationInfo& qnt) {
  VLOG(5) << "[Rockchip NPU] Add Data/Var node " << name;
  auto node = std::make_shared<Node>(precision, layout, Node::Role::kData);
  auto idx = Add(name, node);
  CHECK_EQ(idx, 1);
  node->set_data(
      CvtTensor(graph_, name, shape, qnt.scale, nullptr, precision, layout));
  return node;
}

}  // namespace rknpu
}  // namespace subgraph
}  // namespace lite
}  // namespace paddle
