/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_FLATTEN_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_FLATTEN_GPU_KERNEL_H_

#include <cuda_runtime_api.h>
#include <vector>
#include <string>
#include "backend/kernel_compiler/gpu/gpu_kernel.h"
#include "backend/kernel_compiler/gpu/gpu_kernel_factory.h"

namespace mindspore {
namespace kernel {
template <typename T>
class FlattenFwdGpuKernelMod : public NativeGpuKernelMod {
 public:
  FlattenFwdGpuKernelMod() : input_size_(0), is_null_input_(false) {}
  ~FlattenFwdGpuKernelMod() override = default;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &,
              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
    if (is_null_input_) {
      return true;
    }
    T *input = GetDeviceAddress<T>(inputs, 0);
    T *output = GetDeviceAddress<T>(outputs, 0);
    cudaError_t ret =
      cudaMemcpyAsync(output, input, input_size_, cudaMemcpyDeviceToDevice, reinterpret_cast<cudaStream_t>(stream_ptr));
    if (ret) {
      MS_LOG(ERROR) << "cudaMemcpyAsync error in FlattenFwdGpuKernelMod::Launch, error code is " << ret;
      return false;
    }
    return true;
  }
  bool Init(const CNodePtr &kernel_node) override {
    kernel_name_ = AnfAlgo::GetCNodeName(kernel_node);
    auto shape = AnfAlgo::GetInputRealDeviceShapeIfExist(kernel_node, 0);
    is_null_input_ = CHECK_SHAPE_NULL(shape, kernel_name_, "input");
    if (is_null_input_) {
      InitSizeLists();
      return true;
    }
    input_size_ = sizeof(T);
    for (size_t i = 0; i < shape.size(); ++i) {
      input_size_ *= shape[i];
    }
    InitSizeLists();
    return true;
  }
  void ResetResource() noexcept override {
    input_size_ = 0;
    is_null_input_ = false;
    kernel_name_ = "Flatten";
    input_size_list_.clear();
    output_size_list_.clear();
    workspace_size_list_.clear();
  }

 protected:
  void InitSizeLists() override {
    input_size_list_.push_back(input_size_);
    output_size_list_.push_back(input_size_);
  }

 private:
  size_t input_size_;
  bool is_null_input_;
  std::string kernel_name_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_FLATTEN_GPU_KERNEL_H_
