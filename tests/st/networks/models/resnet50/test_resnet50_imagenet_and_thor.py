# Copyright 2020-2021 Huawei Technologies Co., Ltd
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

"""train and evaluate resnet50 network on imagenet dataset"""

import os
import time
from multiprocessing import Process, Queue
import pytest
import numpy as np

from mindspore import context
from mindspore.common.tensor import Tensor
from mindspore.communication.management import init
from mindspore.context import ParallelMode
from mindspore.train.callback import Callback
from mindspore.train.model import Model
from mindspore.train.train_thor import ConvertModelUtils
from mindspore.train.loss_scale_manager import FixedLossScaleManager
from mindspore.nn.optim import thor
import mindspore.dataset as ds
import mindspore.nn as nn

from tests.st.networks.models.resnet50.src.metric import DistAccuracy, ClassifyCorrectCell
from tests.st.networks.models.resnet50.src.dataset import create_dataset
from tests.st.networks.models.resnet50.src.lr_generator import get_learning_rate
from tests.st.networks.models.resnet50.src.config import config
from tests.st.networks.models.resnet50.src.CrossEntropySmooth import CrossEntropySmooth
from tests.st.networks.models.resnet50.src_thor.config import config as thor_config
from tests.st.networks.models.resnet50.src_thor.dataset import create_dataset2 as create_dataset_thor
from tests.st.networks.models.resnet50.src.resnet import resnet50

MINDSPORE_HCCL_CONFIG_PATH = "/home/workspace/mindspore_config/hccl/rank_tabel_4p/rank_table_4p_1.json"
MINDSPORE_HCCL_CONFIG_PATH_2 = "/home/workspace/mindspore_config/hccl/rank_tabel_4p/rank_table_4p_2.json"
dataset_path = "/home/workspace/mindspore_dataset/imagenet/imagenet_original/train"
eval_path = "/home/workspace/mindspore_dataset/imagenet/imagenet_original/val"

np.random.seed(1)
ds.config.set_seed(1)
os.environ['GLOG_v'] = str(2)


def get_thor_lr(global_step, lr_init, decay, total_epochs, steps_per_epoch, decay_epochs=100):
    """get_model_lr"""
    lr_each_step = []
    total_steps = steps_per_epoch * total_epochs
    for i in range(total_steps):
        epoch = (i + 1) / steps_per_epoch
        base = (1.0 - float(epoch) / total_epochs) ** decay
        lr_local = lr_init * base
        if epoch >= decay_epochs:
            lr_local = lr_local * 0.5
        if epoch >= decay_epochs + 1:
            lr_local = lr_local * 0.5
        lr_each_step.append(lr_local)
    current_step = global_step
    lr_each_step = np.array(lr_each_step).astype(np.float32)
    learning_rate = lr_each_step[current_step:]
    return learning_rate


def get_thor_damping(global_step, damping_init, decay_rate, total_epochs, steps_per_epoch):
    """get_model_damping"""
    damping_each_step = []
    total_steps = steps_per_epoch * total_epochs
    for step in range(total_steps):
        epoch = (step + 1) / steps_per_epoch
        damping_here = damping_init * (decay_rate ** (epoch / 10))
        damping_each_step.append(damping_here)
    current_step = global_step
    damping_each_step = np.array(damping_each_step).astype(np.float32)
    damping_now = damping_each_step[current_step:]
    return damping_now


class LossGet(Callback):
    def __init__(self, per_print_times, data_size):
        super(LossGet, self).__init__()
        if not isinstance(per_print_times, int) or per_print_times < 0:
            raise ValueError("print_step must be int and >= 0.")
        self._per_print_times = per_print_times
        self._loss = 0.0
        self.data_size = data_size
        self._epoch = 0

    def step_end(self, run_context):
        cb_params = run_context.original_args()
        loss = cb_params.net_outputs
        self._epoch = cb_params.cur_epoch_num
        if isinstance(loss, (tuple, list)):
            if isinstance(loss[0], Tensor) and isinstance(loss[0].asnumpy(), np.ndarray):
                loss = loss[0]

        if isinstance(loss, Tensor) and isinstance(loss.asnumpy(), np.ndarray):
            loss = np.mean(loss.asnumpy())

        cur_step_in_epoch = (cb_params.cur_step_num - 1) % cb_params.batch_num + 1

        if isinstance(loss, float) and (np.isnan(loss) or np.isinf(loss)):
            raise ValueError("epoch: {} step: {}. Invalid loss, terminating training."
                             .format(cb_params.cur_epoch_num, cur_step_in_epoch))
        cur_step_in_epoch = (cb_params.cur_step_num - 1) % cb_params.batch_num + 1
        if self._per_print_times != 0 and cb_params.cur_step_num % self._per_print_times == 0:
            self._loss = loss
            print("epoch: %s step: %s, loss is %s" % (cb_params.cur_epoch_num,
                                                      cur_step_in_epoch, loss), flush=True)

    def epoch_begin(self, run_context):
        self.epoch_time = time.time()

    def epoch_end(self, run_context):
        epoch_mseconds = (time.time() - self.epoch_time) * 1000
        self._per_step_mseconds = epoch_mseconds / self.data_size

    def get_loss(self):
        return self._loss

    def get_per_step_time(self):
        return self._per_step_mseconds

    def get_epoch(self):
        return self._epoch


def train_and_eval(device_id, epoch_size, model, dataset, loss_cb, eval_dataset, q):
    print("run_start", device_id)
    eval_interval = config.eval_interval
    step_size = dataset.get_dataset_size()
    acc = 0.0
    time_cost = 0.0
    for epoch_idx in range(0, int(epoch_size / eval_interval)):
        model.train(1, dataset, callbacks=loss_cb)
        eval_start = time.time()
        output = model.eval(eval_dataset)
        eval_cost = (time.time() - eval_start) * 1000
        acc = float(output["acc"])
        time_cost = loss_cb.get_per_step_time()
        loss = loss_cb.get_loss()
        print("the {} epoch's resnet result:\n "
              "device{}, training loss {}, acc {}, "
              "training per step cost {:.2f} ms, eval cost {:.2f} ms, "
              "total_cost {:.2f} ms".format(epoch_idx, device_id,
                                            loss, acc, time_cost,
                                            eval_cost,
                                            time_cost * step_size + eval_cost))
    q.put({'acc': acc, 'cost': time_cost})


def train_process(q, device_id, epoch_size, device_num, enable_hccl):
    os.system("mkdir " + str(device_id))
    os.chdir(str(device_id))
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend", device_id=device_id)
    os.environ['MINDSPORE_HCCL_CONFIG_PATH'] = MINDSPORE_HCCL_CONFIG_PATH
    os.environ['RANK_ID'] = str(device_id)
    os.environ['RANK_SIZE'] = str(device_num)
    if enable_hccl:
        context.set_auto_parallel_context(device_num=device_num, parallel_mode=ParallelMode.DATA_PARALLEL,
                                          gradients_mean=True, all_reduce_fusion_config=[107, 160])
        init()

    # network

    net = resnet50(class_num=config.class_num)

    # evaluation network
    dist_eval_network = ClassifyCorrectCell(net)

    if not config.use_label_smooth:
        config.label_smooth_factor = 0.0

    # loss
    loss = CrossEntropySmooth(sparse=True, reduction="mean",
                              smooth_factor=config.label_smooth_factor, num_classes=config.class_num)

    # train dataset
    dataset = create_dataset(dataset_path=dataset_path, do_train=True, repeat_num=1, batch_size=config.batch_size)

    step_size = dataset.get_dataset_size()
    # evaluation dataset
    eval_dataset = create_dataset(dataset_path=eval_path, do_train=False,
                                  repeat_num=1, batch_size=config.eval_batch_size)

    # loss scale
    loss_scale = FixedLossScaleManager(config.loss_scale, drop_overflow_update=False)

    # learning rate
    lr = Tensor(get_learning_rate(lr_init=config.lr_init, lr_end=0.0, lr_max=config.lr_max,
                                  warmup_epochs=config.warmup_epochs, total_epochs=config.epoch_size,
                                  steps_per_epoch=step_size, lr_decay_mode=config.lr_decay_mode))

    # optimizer
    decayed_params = []
    no_decayed_params = []
    for param in net.trainable_params():
        if 'beta' not in param.name and 'gamma' not in param.name and 'bias' not in param.name:
            decayed_params.append(param)
        else:
            no_decayed_params.append(param)

    group_params = [{'params': decayed_params, 'weight_decay': config.weight_decay},
                    {'params': no_decayed_params, 'weight_decay': 0.0},
                    {'order_params': net.trainable_params()}]

    if config.use_lars:
        momentum = nn.Momentum(group_params, lr, config.momentum,
                               loss_scale=config.loss_scale, use_nesterov=config.use_nesterov)
        opt = nn.LARS(momentum, epsilon=config.lars_epsilon, coefficient=config.lars_coefficient,
                      lars_filter=lambda x: 'beta' not in x.name and 'gamma' not in x.name and 'bias' not in x.name)

    else:
        opt = nn.Momentum(group_params, lr, config.momentum,
                          loss_scale=config.loss_scale, use_nesterov=config.use_nesterov)

    # model
    model = Model(net, loss_fn=loss, optimizer=opt,
                  loss_scale_manager=loss_scale, amp_level="O2", keep_batchnorm_fp32=False,
                  metrics={'acc': DistAccuracy(batch_size=config.eval_batch_size, device_num=device_num)},
                  eval_network=dist_eval_network)

    # callbacks
    loss_cb = LossGet(1, step_size)
    train_and_eval(device_id, epoch_size, model, dataset, loss_cb, eval_dataset, q)


def train_process_thor(q, device_id, epoch_size, device_num, enable_hccl):
    os.system("mkdir " + str(device_id))
    os.chdir(str(device_id))
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    context.set_context(device_id=device_id)
    os.environ['MINDSPORE_HCCL_CONFIG_PATH'] = MINDSPORE_HCCL_CONFIG_PATH_2
    os.environ['RANK_ID'] = str(device_id - 4)
    os.environ['RANK_SIZE'] = str(device_num)
    if enable_hccl:
        context.set_auto_parallel_context(device_num=device_num, parallel_mode=ParallelMode.DATA_PARALLEL,
                                          gradients_mean=True, all_reduce_fusion_config=[85, 160])
        init()

    # network
    net = resnet50(thor_config.class_num)

    if not thor_config.label_smooth:
        thor_config.label_smooth_factor = 0.0

    # loss
    loss = CrossEntropySmooth(sparse=True, reduction="mean", smooth_factor=thor_config.label_smooth_factor,
                              num_classes=thor_config.class_num)

    # train dataset
    dataset = create_dataset_thor(dataset_path=dataset_path, do_train=True,
                                  batch_size=thor_config.batch_size, train_image_size=thor_config.train_image_size,
                                  eval_image_size=thor_config.eval_image_size, target="Ascend",
                                  distribute=True)
    step_size = dataset.get_dataset_size()

    # loss scale
    loss_scale = FixedLossScaleManager(thor_config.loss_scale, drop_overflow_update=False)

    # learning rate
    lr = get_thor_lr(0, 0.05803, 4.04839, 53, 5004, decay_epochs=39)
    damping = get_thor_damping(0, 0.02714, 0.50036, 70, 5004)
    # optimizer
    split_indices = [26, 53]
    opt = thor(net, Tensor(lr), Tensor(damping), thor_config.momentum, thor_config.weight_decay, thor_config.loss_scale,
               thor_config.batch_size, split_indices=split_indices, frequency=thor_config.frequency)

    # evaluation network
    dist_eval_network = ClassifyCorrectCell(net)
    # model
    model = Model(net, loss_fn=loss, optimizer=opt, loss_scale_manager=loss_scale,
                  metrics={'acc': DistAccuracy(batch_size=thor_config.eval_batch_size, device_num=device_num)},
                  amp_level="O2", keep_batchnorm_fp32=False,
                  eval_network=dist_eval_network)

    model = ConvertModelUtils().convert_to_thor_model(model=model, network=net, loss_fn=loss, optimizer=opt,
                                                      loss_scale_manager=loss_scale, metrics={'acc'},
                                                      amp_level="O2", keep_batchnorm_fp32=False)

    # callbacks
    loss_cb = LossGet(1, step_size)

    # train and eval
    print("run_start", device_id)
    model.train(2, dataset, callbacks=loss_cb,
                sink_size=dataset.get_dataset_size(), dataset_sink_mode=True)
    time_cost = loss_cb.get_per_step_time()
    loss = loss_cb.get_loss()
    epoch_idx = loss_cb.get_epoch()
    print("the {} epoch's resnet result:\n "
          "device{}, training loss {}, "
          "training per step cost {:.2f} ms, total_cost {:.2f} ms".format(epoch_idx, device_id,
                                                                          loss, time_cost, time_cost * step_size))
    q.put({'loss': loss, 'cost': time_cost})


def resnet_end(device_num, q):
    acc = 0.0
    cost = 0.0
    for i in range(device_num):
        assert not q.empty()
        output = q.get()
        acc += output['acc']
        cost += output['cost']
    acc = acc / device_num
    cost = cost / device_num

    for i in range(device_num):
        os.system("rm -rf " + str(i))
    print("End training...")
    assert acc > 0.1
    assert cost < 26


def thor_end(device_num, q):
    thor_loss = 0.0
    thor_cost = 0.0
    for i in range(device_num):
        output = q.get()
        thor_loss += output['loss']
        thor_cost += output['cost']
    thor_loss = thor_loss / device_num
    thor_cost = thor_cost / device_num

    for i in range(4, device_num + 4):
        os.system("rm -rf " + str(i))
    print("End training...")
    assert thor_loss < 7
    assert thor_cost < 30


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_single
def test_resnet_imagenet_and_thor_4p():
    """
    Feature: Resnet50 network.
    Description: Train and evaluate resnet50 network on imagenet dataset.
    Expectation: accuracy > 0.1, time cost < 26.
    """
    context.set_context(enable_graph_kernel=False, enable_sparse=False)
    context.reset_auto_parallel_context()
    context.reset_ps_context()

    q = Queue()
    q2 = Queue()
    device_num = 4
    epoch_size = 2
    epoch_size_2 = 1
    enable_hccl = True
    process = []
    process2 = []
    for i in range(device_num):
        device_id = i
        process.append(Process(target=train_process,
                               args=(q, device_id, epoch_size, device_num, enable_hccl)))
        process2.append(Process(target=train_process_thor,
                                args=(q2, device_id + 4, epoch_size_2, device_num, enable_hccl)))
    cpu_count = os.cpu_count()
    half_cpu_count = cpu_count // 2
    each_cpu_count = half_cpu_count // device_num
    for i in range(device_num):
        process[i].start()
        process2[i].start()
        if each_cpu_count > 1:
            cpu_start = each_cpu_count * i
            cpu_end = each_cpu_count * (i + 1)
            process_cpu = [x for x in range(cpu_start, cpu_end)]
            process2_cpu = [x for x in range(cpu_start + half_cpu_count, cpu_end + half_cpu_count)]
            pid1 = process[i].pid
            pid2 = process2[i].pid
            os.sched_setaffinity(pid1, set(process_cpu))
            os.sched_setaffinity(pid2, set(process2_cpu))
    print("Waiting for all subprocesses done...")

    for i in range(device_num):
        process[i].join()
        process2[i].join()
    # resnet
    resnet_end(device_num, q)
    # thor
    thor_end(device_num, q2)
    