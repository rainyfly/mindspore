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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_ACTIVATION_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_ACTIVATION_GPU_KERNEL_H_

#include <vector>
#include <map>
#include <string>
#include "backend/kernel_compiler/gpu/gpu_kernel.h"
#include "backend/kernel_compiler/gpu/gpu_kernel_factory.h"
#include "backend/kernel_compiler/gpu/kernel_constants.h"
#include "backend/kernel_compiler/gpu/cuda_impl/relu_impl.cuh"

namespace mindspore {
namespace kernel {
template <typename T>
class ActivationFwdGpuKernelMod : public NativeGpuKernelMod {
 public:
  ActivationFwdGpuKernelMod() { ResetResource(); }
  ~ActivationFwdGpuKernelMod() override { DestroyResource(); }

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &,
              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
    if (is_null_input_) {
      return true;
    }
    T *input = GetDeviceAddress<T>(inputs, 0);
    T *output = GetDeviceAddress<T>(outputs, 0);

    const float alpha = 1;
    const float beta = 0;
    CHECK_CUDNN_RET_WITH_EXCEPT(kernel_node_,
                                cudnnActivationForward(cudnn_handle_, activation_desc_, &alpha, data_descriptor_, input,
                                                       &beta, data_descriptor_, output),
                                "cudnnActivationForward failed");

    return true;
  }
  bool Init(const CNodePtr &kernel_node) override {
    kernel_node_ = kernel_node;
    auto node_name = AnfAlgo::GetCNodeName(kernel_node);
    auto iter = kernel_map.find(node_name);
    if (iter == kernel_map.end()) {
      MS_LOG(EXCEPTION) << "Only support these activations: ReLU6, Tanh, Elu, Sigmoid currently, but got " << node_name;
    }
    mode_ = iter->second;

    InitResource();
    cudnn_data_type_ = GetCudnnDataType(TypeIdLabel(AnfAlgo::GetInputDeviceDataType(kernel_node, 0)));
    size_t input_num = AnfAlgo::GetInputTensorNum(kernel_node);
    if (input_num != 1) {
      MS_LOG(EXCEPTION) << "For '" << node_name << "', the number of inputs should be 1, but got " << input_num;
    }
    auto input_shape = AnfAlgo::GetInputRealDeviceShapeIfExist(kernel_node, 0);
    is_null_input_ = CHECK_SHAPE_NULL(input_shape, node_name, "input");
    if (is_null_input_) {
      InitSizeLists();
      return true;
    }
    CheckTensorSize({input_shape});
    std::vector<size_t> shape;
    double coef = (mode_ == CUDNN_ACTIVATION_CLIPPED_RELU) ? 6.0 : 0.0;
    if (mode_ == CUDNN_ACTIVATION_ELU) {
      float alpha = GetAttr<float>(kernel_node, "alpha");
      coef = static_cast<double>(alpha);
    }
    CHECK_CUDNN_RET_WITH_EXCEPT(kernel_node_,
                                cudnnSetActivationDescriptor(activation_desc_, mode_, CUDNN_NOT_PROPAGATE_NAN, coef),
                                "cudnnSetActivationDescriptor failed");
    const int split_dim = 4;
    if (input_shape.size() <= split_dim) {
      ShapeNdTo4d(input_shape, &shape);
      if (AnfAlgo::GetInputFormat(kernel_node, 0) == kOpFormat_NHWC) {
        CHECK_CUDNN_RET_WITH_EXCEPT(
          kernel_node_,
          cudnnSetTensor4dDescriptor(data_descriptor_, CUDNN_TENSOR_NHWC, cudnn_data_type_, SizeToInt(shape[0]),
                                     SizeToInt(shape[3]), SizeToInt(shape[1]), SizeToInt(shape[2])),
          "cudnnSetTensor4dDescriptor failed");
      } else {
        CHECK_CUDNN_RET_WITH_EXCEPT(
          kernel_node_,
          cudnnSetTensor4dDescriptor(data_descriptor_, CUDNN_TENSOR_NCHW, cudnn_data_type_, SizeToInt(shape[0]),
                                     SizeToInt(shape[1]), SizeToInt(shape[2]), SizeToInt(shape[3])),
          "cudnnSetTensor4dDescriptor failed");
      }
    } else {
      CudnnSetTensorNdDescriptor(input_shape, data_descriptor_, cudnn_data_type_, kernel_node_);
    }

    InitSizeLists();
    return true;
  }

  void DestroyResource() noexcept override {
    CHECK_CUDNN_RET_WITH_ERROR(kernel_node_, cudnnDestroyActivationDescriptor(activation_desc_),
                               "cudnnDestroyActivationDescriptor failed");
    CHECK_CUDNN_RET_WITH_ERROR(kernel_node_, cudnnDestroyTensorDescriptor(data_descriptor_),
                               "cudnnDestroyTensorDescriptor failed");
  }

  void ResetResource() noexcept override {
    cudnn_handle_ = nullptr;
    activation_desc_ = nullptr;
    mode_ = CUDNN_ACTIVATION_SIGMOID;
    data_descriptor_ = nullptr;
    is_null_input_ = false;
    input_size_list_.clear();
    output_size_list_.clear();
    workspace_size_list_.clear();
    cudnn_data_type_ = CUDNN_DATA_FLOAT;
    input_size_ = 0;
    output_size_ = 0;
    workspace_size_ = 0;
  }

 protected:
  void InitResource() override {
    cudnn_handle_ = device::gpu::GPUDeviceManager::GetInstance().GetCudnnHandle();
    CHECK_CUDNN_RET_WITH_EXCEPT(kernel_node_, cudnnCreateTensorDescriptor(&data_descriptor_),
                                "cudnnCreateTensorDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT(kernel_node_, cudnnCreateActivationDescriptor(&activation_desc_),
                                "cudnnCreateActivationDescriptor failed");
  }

  void InitSizeLists() override {
    if (!is_null_input_) {
      CHECK_CUDNN_RET_WITH_EXCEPT(kernel_node_, cudnnGetTensorSizeInBytes(data_descriptor_, &input_size_),
                                  "cudnnGetTensorSizeInBytes failed");
      output_size_ = input_size_;
    }
    input_size_list_.push_back(input_size_);
    output_size_list_.push_back(output_size_);
    workspace_size_list_.push_back(workspace_size_);
  }

 private:
  std::map<std::string, cudnnActivationMode_t> kernel_map = {{"ReLU6", CUDNN_ACTIVATION_CLIPPED_RELU},
                                                             {"Tanh", CUDNN_ACTIVATION_TANH},
                                                             {"Elu", CUDNN_ACTIVATION_ELU},
                                                             {"Sigmoid", CUDNN_ACTIVATION_SIGMOID}};

  cudnnHandle_t cudnn_handle_;
  cudnnActivationDescriptor_t activation_desc_;
  cudnnActivationMode_t mode_;
  cudnnTensorDescriptor_t data_descriptor_;
  bool is_null_input_;

  cudnnDataType_t cudnn_data_type_;
  size_t input_size_;
  size_t output_size_;
  size_t workspace_size_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_ACTIVATION_GPU_KERNEL_H_
