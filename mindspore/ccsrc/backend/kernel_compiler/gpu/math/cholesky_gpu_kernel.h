/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MATH_CHOLESKY_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MATH_CHOLESKY_GPU_KERNEL_H_
#include <cublas_v2.h>
#include <cuda_runtime_api.h>
#include <vector>
#include <string>
#include <algorithm>
#include "backend/kernel_compiler/gpu/cuda_impl/eye_impl.cuh"
#include "backend/kernel_compiler/gpu/cuda_impl/matrix_split_impl.cuh"
#include "backend/kernel_compiler/gpu/cuda_impl/triangle_matrix_copy_impl.cuh"
#include "backend/kernel_compiler/gpu/gpu_kernel.h"
#include "backend/kernel_compiler/gpu/gpu_kernel_factory.h"
#include "backend/kernel_compiler/gpu/kernel_constants.h"
#include "utils/convert_utils.h"

namespace mindspore {
namespace kernel {
constexpr size_t kCholeskyInputsNum = 1;
constexpr size_t kInputIndex = 0;
constexpr size_t kCholeskyOutputsNum = 1;
constexpr size_t kOutputIndex = 0;
constexpr size_t kCholeskyDefaultShape = 1;
constexpr size_t kCholeskyNormalShape = 2;
constexpr size_t kCholeskyBatchedShape = 3;

template <typename T>
class CholeskyGpuKernelMod : public NativeGpuKernelMod {
 public:
  using pointer = T *;

  CholeskyGpuKernelMod() = default;
  ~CholeskyGpuKernelMod() = default;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs, void *stream_ptr) override {
    CHECK_CUSOLVER_RET_WITH_ERROR(cusolverDnSetStream(handle_, reinterpret_cast<cudaStream_t>(stream_ptr)),
                                  "cholesky bind cusolverDnSetStream failed");
    if (!use_split_matrix_) {
      return NoSplitLaunch(inputs, workspace, outputs, stream_ptr);
    }
    return SplitLaunch(inputs, workspace, outputs, stream_ptr);
  }

  bool Init(const CNodePtr &kernel_node) override {
    kernel_name_ = AnfAlgo::GetCNodeName(kernel_node);
    kernel_node_ = kernel_node;
    lower_ = static_cast<bool>(GetAttr<bool>(kernel_node, kLower));
    clean_ = static_cast<bool>(GetAttr<bool>(kernel_node, kClean));
    split_dim_ = static_cast<int>(GetAttr<int64_t>(kernel_node, kSplitDim));
    // cholesky input is sys_positive_matrix and saved by col_major in gpu backend.
    // so we reverse lower to upper, to fake transpose col_major input to row_major.
    if (lower_) {
      uplo_ = CUBLAS_FILL_MODE_UPPER;
    } else {
      uplo_ = CUBLAS_FILL_MODE_LOWER;
    }
    // get CuSolver Dense matrix handler
    handle_ = device::gpu::GPUDeviceManager::GetInstance().GetCusolverDnHandle();

    auto in_shape = AnfAlgo::GetPrevNodeOutputInferShape(kernel_node, kInputIndex);

    is_null_input_ = CHECK_SHAPE_NULL(in_shape, kernel_name_, "input");
    if (is_null_input_) {
      InitSizeLists();
      return true;
    }
    if (split_dim_ == 0) {
      return InitNoSplitDim(in_shape);
    }
    return InitSplitDim(in_shape);
  }

 protected:
  bool InitNoSplitDim(const std::vector<size_t> &in_shape) {
    if (in_shape.size() == kCholeskyDefaultShape) {
      batch_ = 1;
      cho_row_ = in_shape.at(kDim0);
      cho_col_ = cho_row_;
    } else if (in_shape.size() == kCholeskyNormalShape) {
      batch_ = 1;
      cho_row_ = in_shape.at(kDim0);
      cho_col_ = in_shape.at(kDim1);
    } else if (in_shape.size() == kCholeskyBatchedShape) {
      batch_ = SizeToInt(in_shape.at(kDim0));
      cho_row_ = in_shape.at(kDim1);
      cho_col_ = in_shape.at(kDim2);
    } else {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of input only should be 2 or 3";
      return false;
    }
    if (cho_row_ != cho_col_) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the shape of input should be square matrix";
      return false;
    }
    // set matrix row or col to be lead dimension
    m_ = SizeToInt(in_shape.at(kDim1));
    lda_ = m_;
    ldb_ = m_;
    h_array_.resize(batch_);
    InitSizeLists();
    return true;
  }

  bool InitSplitDim(const std::vector<size_t> &in_shape) {
    if (in_shape.size() != kCholeskyNormalShape) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of input should be " << kCholeskyNormalShape
                    << ", but got " << in_shape.size();
      return false;
    }
    cho_row_ = in_shape.at(kDim0);
    cho_col_ = in_shape.at(kDim1);
    if (cho_row_ != cho_col_) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the shape of input should be square matrix";
      return false;
    }

    if (SizeToInt(cho_row_) <= split_dim_) {
      use_split_matrix_ = false;
      batch_ = 1;
      m_ = SizeToInt(in_shape.at(kDim1));
      lda_ = m_;
      ldb_ = m_;
      h_array_.resize(batch_);
      InitSizeLists();
      return true;
    }

    use_split_matrix_ = true;
    size_t batch = cho_col_ / split_dim_;
    res_dim_ = cho_col_ - batch * split_dim_;
    if (res_dim_ == 0) {
      batch_ = batch;
    } else {
      batch_ = batch + 1;
    }
    m_ = split_dim_;
    lda_ = m_;
    ldb_ = m_;
    h_array_.resize(batch_);
    InitSizeLists();
    return true;
  }

  void InitSizeLists() override {
    size_t workspace_size = batch_ * sizeof(pointer);
    workspace_size_list_.emplace_back(workspace_size);
    workspace_size = batch_ * sizeof(int);
    workspace_size_list_.emplace_back(workspace_size);

    size_t input_size;
    if (!use_split_matrix_) {
      input_size = batch_ * m_ * lda_ * unit_size_;
    } else {
      input_size = cho_row_ * cho_col_ * unit_size_;
      workspace_size = batch_ * m_ * lda_ * unit_size_;
      workspace_size_list_.emplace_back(workspace_size);
    }
    input_size_list_.push_back(input_size);
    size_t output_size = batch_ * m_ * lda_ * unit_size_;
    output_size_list_.push_back(output_size);
  }

  bool NoSplitLaunch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
                     const std::vector<AddressPtr> &outputs, void *stream_ptr) {
    // here all addresses are malloc by cuda, so deal with them as device's address.
    auto input1_addr = GetDeviceAddress<T>(inputs, kDim0);
    auto output_addr = GetDeviceAddress<T>(outputs, kDim0);

    auto d_array_addr = GetDeviceAddress<pointer>(workspace, kDim0);
    auto d_info_array_addr = GetDeviceAddress<int>(workspace, kDim1);

    // copy input data to output, cholesky inplace output in gpu backend.
    CHECK_CUDA_RET_WITH_ERROR(kernel_node_,
                              cudaMemcpyAsync(output_addr, input1_addr, batch_ * m_ * lda_ * unit_size_,
                                              cudaMemcpyDeviceToDevice, reinterpret_cast<cudaStream_t>(stream_ptr)),
                              "cuda memcopy input to output Fail");

    for (size_t i = 0; i < batch_; i++) {
      h_array_[i] = output_addr + i * lda_ * m_;
    }

    // copy output's addr to d_array_addr
    CHECK_CUDA_RET_WITH_ERROR(kernel_node_,
                              cudaMemcpyAsync(d_array_addr, h_array_.data(), sizeof(pointer) * batch_,
                                              cudaMemcpyHostToDevice, reinterpret_cast<cudaStream_t>(stream_ptr)),
                              "cuda memcopy Fail");

    // solve to cholesky factorization according to cuSolver api, outputs have been written to input's matrix.
    if constexpr (std::is_same_v<T, float>) {
      CHECK_CUSOLVER_RET_WITH_EXCEPT(
        kernel_node_, cusolverDnSpotrfBatched(handle_, uplo_, m_, d_array_addr, lda_, d_info_array_addr, batch_),
        "cusolver cholesky batched Fail");
    } else if constexpr (std::is_same_v<T, double>) {
      CHECK_CUSOLVER_RET_WITH_EXCEPT(
        kernel_node_, cusolverDnDpotrfBatched(handle_, uplo_, m_, d_array_addr, lda_, d_info_array_addr, batch_),
        "cusolver cholesky batched Fail");
    } else {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the data type only should be float or double, right now.";
    }
    size_t output_elements = outputs.at(kDim0)->size / unit_size_;
    // copy results from original input's matrix to output's matrix by up or lower flag.
    TriangleMatrixCopy(input1_addr, output_addr, clean_, uplo_, output_elements, ldb_, m_,
                       reinterpret_cast<cudaStream_t>(stream_ptr));
    return true;
  }

  bool SplitLaunch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
                   const std::vector<AddressPtr> &outputs, void *stream_ptr) {
    auto input1_addr = GetDeviceAddress<T>(inputs, kDim0);
    auto output_addr = GetDeviceAddress<T>(outputs, kDim0);

    auto d_array_addr = GetDeviceAddress<pointer>(workspace, kDim0);
    auto d_info_array_addr = GetDeviceAddress<int>(workspace, kDim1);
    auto d_batch_input_addr = GetDeviceAddress<T>(workspace, kDim2);

    for (size_t i = 0; i < batch_; i++) {
      h_array_[i] = d_batch_input_addr + i * lda_ * m_;
    }
    Eye(batch_ * split_dim_ * split_dim_, split_dim_, output_addr, reinterpret_cast<cudaStream_t>(stream_ptr));
    MatrixSplit(batch_ * split_dim_ * split_dim_, split_dim_, cho_col_, input1_addr, d_batch_input_addr,
                reinterpret_cast<cudaStream_t>(stream_ptr));
    CHECK_CUDA_RET_WITH_ERROR(kernel_node_,
                              cudaMemcpyAsync(d_array_addr, h_array_.data(), sizeof(pointer) * batch_,
                                              cudaMemcpyHostToDevice, reinterpret_cast<cudaStream_t>(stream_ptr)),
                              "cuda memcopy Fail");
    if constexpr (std::is_same_v<T, float>) {
      CHECK_CUSOLVER_RET_WITH_EXCEPT(
        kernel_node_, cusolverDnSpotrfBatched(handle_, uplo_, m_, d_array_addr, lda_, d_info_array_addr, batch_),
        "cusolver cholesky batched Fail");
    } else if constexpr (std::is_same_v<T, double>) {
      CHECK_CUSOLVER_RET_WITH_EXCEPT(
        kernel_node_, cusolverDnDpotrfBatched(handle_, uplo_, m_, d_array_addr, lda_, d_info_array_addr, batch_),
        "cusolver cholesky batched Fail");
    } else {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the data type only should be float or double, right now.";
    }
    size_t output_elements = outputs.at(kDim0)->size / unit_size_;
    // copy results from original input's matrix to output's matrix by up or lower flag.
    TriangleMatrixCopy(input1_addr, output_addr, clean_, uplo_, output_elements, ldb_, m_,
                       reinterpret_cast<cudaStream_t>(stream_ptr));
    return true;
  }

 private:
  size_t unit_size_{sizeof(T)};
  size_t cho_row_{0};
  size_t cho_col_{0};
  size_t batch_{0};
  size_t m_{0};
  size_t lda_{0};
  size_t ldb_{0};
  int res_dim_{0};
  int split_dim_{0};
  bool is_null_input_{false};
  bool use_split_matrix_{false};
  cusolverDnHandle_t handle_{nullptr};
  cublasFillMode_t uplo_ = CUBLAS_FILL_MODE_UPPER;
  std::vector<pointer> h_array_;
  bool lower_{false};
  bool clean_{false};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MATH_CHOLESKY_GPU_KERNEL_H_
