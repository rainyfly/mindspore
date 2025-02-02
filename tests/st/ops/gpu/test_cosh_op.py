# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

import numpy as np
import pytest

import mindspore.nn as nn
from mindspore import Tensor
from mindspore import context
from mindspore.ops import operations as P


class NetCosh(nn.Cell):
    def __init__(self):
        super(NetCosh, self).__init__()
        self.cosh = P.Cosh()

    def construct(self, x):
        return self.cosh(x)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.float32, np.float64])
def test_cosh_graph(dtype):
    """
    Feature: ALL To ALL
    Description: test cases for Cosh
    Expectation: the result match to numpy
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    np_array = np.array([-1, -0.5, 0, 0.5, 1]).astype(dtype)
    input_x = Tensor(np_array)
    net = NetCosh()
    output = net(input_x)
    expect = np.cosh(np_array)
    assert np.allclose(output.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.float32, np.float64])
def test_cosh_py(dtype):
    """
    Feature: ALL To ALL
    Description: test cases for Cosh
    Expectation: the result match to numpy
    """
    context.set_context(mode=context.PYNATIVE_MODE, device_target="GPU")
    np_array = np.array([-1, -0.5, 0, 0.5, 1]).astype(dtype)
    input_x = Tensor(np_array)
    net = NetCosh()
    output = net(input_x)
    expect = np.cosh(np_array)
    assert np.allclose(output.asnumpy(), expect)
