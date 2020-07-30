/**
 * Copyright 2019 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MINDSPORE_CCSRC_BACKEND_SESSION_ASCEND_INFERENCE_SESSION_H
#define MINDSPORE_CCSRC_BACKEND_SESSION_ASCEND_INFERENCE_SESSION_H
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <utility>
#include <stack>
#include <map>
#include <tuple>
#include <set>
#include "backend/session/ascend_session.h"
#include "backend/session/kernel_graph.h"
#include "backend/kernel_compiler/kernel.h"
#include "backend/session/session_factory.h"
#include "backend/session/ascend_control_parser.h"

namespace mindspore {
namespace session {
class AscendInferenceSession : public AscendSession {
 public:
  AscendInferenceSession() = default;
  ~AscendInferenceSession() = default;
  void LoadInputData(const std::shared_ptr<KernelGraph> &kernel_graph,
                     const std::vector<tensor::TensorPtr> &inputs_const) const;
  GraphId CompileGraph(NotNull<FuncGraphPtr> func_graph) override;
  bool CheckModelInputs(uint32_t graph_id, const std::vector<tensor::TensorPtr> &inputs) const override;
  bool CompareInput(const tensor::TensorPtr &input, const ParameterPtr &parameter) const;
  template <typename T>
  std::string PrintInputShape(std::vector<T> shape) const;
  std::string InputsInfo(const std::vector<ParameterPtr> &paras, const std::vector<tensor::TensorPtr> &inputs) const;
};
MS_REG_SESSION(kDavinciInferenceDevice, AscendInferenceSession);
}  // namespace session
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_SESSION_ASCEND_INFERENCE_SESSION_H
