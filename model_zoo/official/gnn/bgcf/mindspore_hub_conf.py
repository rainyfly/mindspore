# Copyright 2020 Huawei Technologies Co., Ltd
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
"""hub config."""
from src.bgcf import BGCF
from src.config import parser_args

def bgcf_net(*args, **kwargs):
    return BGCF(*args, **kwargs)


def create_network(name, *args, **kwargs):
    """
    create bgcf network
    """
    if name == "bgcf":
        config = parser_args()
        config.num_user = 7068
        config.num_item = 3570
        return bgcf_net([config.input_dim, config.num_user, config.num_item],
                        config.embedded_dimension,
                        config.activation,
                        [0.0, 0.0, 0.0],
                        config.num_user,
                        config.num_item,
                        config.input_dim,
                        *args, **kwargs)
    raise NotImplementedError(f"{name} is not implemented in the repo")
