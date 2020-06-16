# Copyright 2019 Huawei Technologies Co., Ltd
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
# ==============================================================================
"""
Introduction to mindrecord:

Mindrecord is a module to implement reading, writing, search and
converting for MindSpore format dataset. Users could load(modify)
mindrecord data through FileReader(FileWriter). Users could also
convert other format dataset to mindrecord data through
corresponding sub-module.
"""

from .filewriter import FileWriter
from .filereader import FileReader
from .mindpage import MindPage
from .common.exceptions import *
from .shardutils import SUCCESS, FAILED
from .tools.cifar10_to_mr import Cifar10ToMR
from .tools.cifar100_to_mr import Cifar100ToMR
from .tools.imagenet_to_mr import ImageNetToMR
from .tools.mnist_to_mr import MnistToMR
from .tools.tfrecord_to_mr import TFRecordToMR

__all__ = ['FileWriter', 'FileReader', 'MindPage',
           'Cifar10ToMR', 'Cifar100ToMR', 'ImageNetToMR', 'MnistToMR', 'TFRecordToMR',
           'SUCCESS', 'FAILED']
