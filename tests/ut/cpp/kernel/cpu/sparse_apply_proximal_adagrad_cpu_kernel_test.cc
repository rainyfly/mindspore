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

#include <vector>
#include "common/common_test.h"
#define private public
#define protected public
#include "backend/kernel_compiler/cpu/sparse_apply_proximal_adagrad_cpu_kernel.h"
#undef private
#undef protected

namespace mindspore {
namespace kernel {
class SparseApplyProximalAdagradCpuKernelTest : public UT::Common {
 public:
  SparseApplyProximalAdagradCpuKernelTest()
      : sparse_proximal_adagrad_(std::make_shared<SparseApplyProximalAdagradCpuKernelMod>()) {}

  void SetUp() override {
    var_.clear();
    accum_.clear();
    grad_.clear();
    inputs_.clear();
    workspace_.clear();
    outputs_.clear();
  }

  AddressPtr CreateKernelAddress(void *addr) {
    auto kernel_addr = std::make_shared<Address>();
    kernel_addr->addr = addr;
    return kernel_addr;
  }

  void CreateInputAddress(std::vector<int64_t> &indices) {
    inputs_.push_back(CreateKernelAddress(var_.data()));
    inputs_.push_back(CreateKernelAddress(accum_.data()));
    inputs_.push_back(CreateKernelAddress(&lr_));
    inputs_.push_back(CreateKernelAddress(&l1_));
    inputs_.push_back(CreateKernelAddress(&l2_));
    inputs_.push_back(CreateKernelAddress(grad_.data()));
    inputs_.push_back(CreateKernelAddress(indices.data()));
  }

  void CreateWorkspaceAddress(std::vector<float> &new_grad, std::vector<int64_t> &new_indices,
                              std::vector<float> &tmp_grad, std::vector<int64_t> &tmp_indices) {
    workspace_.push_back(CreateKernelAddress(new_grad.data()));
    workspace_.push_back(CreateKernelAddress(new_indices.data()));
    workspace_.push_back(CreateKernelAddress(tmp_grad.data()));
    workspace_.push_back(CreateKernelAddress(tmp_indices.data()));
  }

  std::vector<float> var_;
  std::vector<float> accum_;
  std::vector<float> grad_;
  std::vector<AddressPtr> inputs_;
  std::vector<AddressPtr> workspace_;
  std::vector<AddressPtr> outputs_;
  std::shared_ptr<SparseApplyProximalAdagradCpuKernelMod> sparse_proximal_adagrad_;
  float lr_ = 0.01;
  float l1_ = 0.0;
  float l2_ = 0.0;
};

TEST_F(SparseApplyProximalAdagradCpuKernelTest, dense_test) {
  for (size_t i = 0; i < 3 * 3 * 3; ++i) {
    var_.push_back(1.0);
    accum_.push_back(1.0);
    grad_.push_back(1.0);
  }
  sparse_proximal_adagrad_->indices_size_ = 3;
  sparse_proximal_adagrad_->var_first_dim_size_ = 3;
  sparse_proximal_adagrad_->var_outer_dim_size_ = 9;
  sparse_proximal_adagrad_->indices_data_type_ = kNumberTypeInt64;

  std::vector<int64_t> indices{0, 1, 2};
  CreateInputAddress(indices);
  std::vector<float> new_grad(3 * 3 * 3);
  std::vector<int64_t> new_indices(3);
  std::vector<float> tmp_grad(3 * 3 * 3);
  std::vector<int64_t> tmp_indices(3);
  CreateWorkspaceAddress(new_grad, new_indices, tmp_grad, tmp_indices);
  sparse_proximal_adagrad_->Launch(inputs_, workspace_, outputs_);
  for (size_t i = 0; i < 3 * 3 * 3; ++i) {
    EXPECT_TRUE(std::fabs(var_[i] - 0.9929289) < 1e-6);
  }
}

TEST_F(SparseApplyProximalAdagradCpuKernelTest, sparse_test1) {
  for (size_t i = 0; i < 3 * 3 * 3; ++i) {
    var_.push_back(1.0);
    accum_.push_back(1.0);
  }
  for (size_t i = 0; i < 2 * 3 * 3; ++i) {
    grad_.push_back(1.0);
  }
  sparse_proximal_adagrad_->indices_size_ = 2;
  sparse_proximal_adagrad_->var_first_dim_size_ = 3;
  sparse_proximal_adagrad_->var_outer_dim_size_ = 9;
  sparse_proximal_adagrad_->indices_data_type_ = kNumberTypeInt64;

  std::vector<int64_t> indices{0, 2};
  CreateInputAddress(indices);
  std::vector<float> new_grad(3 * 3 * 3);
  std::vector<int64_t> new_indices(3);
  std::vector<float> tmp_grad(3 * 3 * 3);
  std::vector<int64_t> tmp_indices(3);
  CreateWorkspaceAddress(new_grad, new_indices, tmp_grad, tmp_indices);
  sparse_proximal_adagrad_->Launch(inputs_, workspace_, outputs_);
  for (size_t i = 0; i < 3 * 3; ++i) {
    EXPECT_TRUE(std::fabs(var_[i] - 0.9929289) < 1e-6);
  }
  for (size_t i = 3 * 3; i < 2 * 3 * 3; ++i) {
    EXPECT_EQ(var_[i], 1.0);
  }
  for (size_t i = 2 * 3 * 3; i < 3 * 3 * 3; ++i) {
    EXPECT_TRUE(std::fabs(var_[i] - 0.9929289) < 1e-6);
  }
}

TEST_F(SparseApplyProximalAdagradCpuKernelTest, sparse_test2) {
  for (size_t i = 0; i < 3 * 3 * 3; ++i) {
    var_.push_back(1.0);
    accum_.push_back(1.0);
    grad_.push_back(1.0);
  }
  sparse_proximal_adagrad_->indices_size_ = 3;
  sparse_proximal_adagrad_->var_first_dim_size_ = 3;
  sparse_proximal_adagrad_->var_outer_dim_size_ = 9;
  sparse_proximal_adagrad_->indices_data_type_ = kNumberTypeInt64;

  std::vector<int64_t> indices{2, 2, 1};
  CreateInputAddress(indices);
  std::vector<float> new_grad(3 * 3 * 3);
  std::vector<int64_t> new_indices(3);
  std::vector<float> tmp_grad(3 * 3 * 3);
  std::vector<int64_t> tmp_indices(3);
  CreateWorkspaceAddress(new_grad, new_indices, tmp_grad, tmp_indices);
  sparse_proximal_adagrad_->Launch(inputs_, workspace_, outputs_);
  for (size_t i = 0; i < 3 * 3; ++i) {
    EXPECT_EQ(var_[i], 1.0);
  }
  for (size_t i = 3 * 3; i < 2 * 3 * 3; ++i) {
    EXPECT_TRUE(std::fabs(var_[i] - 0.9929289) < 1e-6);
  }
  for (size_t i = 2 * 3 * 3; i < 3 * 3 * 3; ++i) {
    EXPECT_TRUE(std::fabs(var_[i] - 0.9910557) < 1e-6);
  }
}
}  // namespace kernel
}  // namespace mindspore
