/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_SCHEDULER_H_
#define MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_SCHEDULER_H_

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include "runtime/framework/graph_compiler.h"
#include "runtime/framework/actor/data_prepare_actor.h"
#include "runtime/framework/actor/data_source_actor.h"
#include "runtime/framework/actor/loop_count_actor.h"
#include "runtime/framework/actor/kernel_actor.h"
#include "runtime/framework/actor/output_actor.h"
#include "runtime/framework/actor/switch_actor.h"
#include "runtime/framework/actor/gather_actor.h"
#include "runtime/framework/actor/copy_actor.h"
#include "thread/actor_threadpool.h"

namespace mindspore {
namespace runtime {
using mindspore::device::DeviceContext;
using mindspore::session::KernelGraph;
using mindspore::session::KernelWithIndex;
using ActorInfo = std::string;

// The second element of pair represents the output index of abstract actor corresponding to the graph output node.
using GraphOutputPair = std::pair<AbstractActor *, size_t>;

// The actor set generated by graph transformer is the execution unit of actor runtime.
// It includes data source actor, kernel actor, switch actor, copy actor, loop count actor and output actor.
// The data prepare actor is used to prepare data for device tensor store and host tensor queue to represent the begin
// of one step.
// The data source actor is used to obtain data and process them into device tensors, and send them to kernel actor.
// The kernel actor is used to receive the device tensors to luanch kernel. Specifically notice the no input
// kernel actor, it means that this actor has no input device tensor, need be triggered externally.
// The switch actor is used to run different branches in the control flow scenario.
// The gather actor is used to collect the inputs of graph and send branch id to loop count actor in multi-branch
// output scenario.
// The copy actor is used to convert the device tensor between the different device kernel.
// The loop count actor is used to receive the control of tail kernel actor to represent the end of one step
// and decide whether to loop execution by loop count.
// The output actor is used to receive the output result of actor which represents the graph output.
struct ActorSet {
  explicit ActorSet(const ActorInfo &name) : name_(name) {}
  DataPrepareActorPtr data_prepare_actor_{nullptr};
  std::vector<DataSourceActorPtr> data_source_actors_;
  std::vector<KernelActorPtr> kernel_actors_;
  // No input kernel actors need be triggered specifically.
  std::vector<KernelActorPtr> no_input_kernel_actors_;
  std::vector<SwitchActorPtr> switch_actors_;
  std::vector<GatherActorPtr> gather_actors_;
  std::vector<CopyActorPtr> copy_actors_;
  LoopCountActorPtr loop_count_actor_{nullptr};
  OutputActorPtr output_actor_{nullptr};
  ActorInfo name_;
};
using ActorSetPtr = std::shared_ptr<ActorSet>;

class GraphScheduler {
 public:
  static GraphScheduler &GetInstance() {
    static GraphScheduler instance;
    return instance;
  }

  // 1. Thread pool creating.
  // 2. The global actors creating and scheduling.
  void Initialize();

  // Clear the members.
  void Clear();
  void Clear(const ActorInfo &actor_info, const std::vector<KernelGraphPtr> &graphs);

  // Transform graph to actor DAG, contains build and link.
  ActorSet *Transform(const GraphCompilerInfo &graph_compiler_info);

  // Schedule actors in the actor runtime. Single machine scheduling is supported currently, and distributed scheduling
  // will be supported in the future.
  void Schedule(const ActorSet *actor_set);

  // The processing entry of actors running. The third parameter is used only in the step execution strategy.
  bool Run(const ActorSet *actor_set, const std::vector<std::vector<TensorPtr>> &input_tensors,
           const std::vector<TensorPtr> &input_tensors_with_value_node = {},
           GraphExecutionStrategy strategy = GraphExecutionStrategy::kPipeline);

  // Fetch the actor set by actor info.
  ActorSet *Fetch(const ActorInfo &actor_info) const;

 private:
  GraphScheduler() = default;
  ~GraphScheduler() = default;
  DISABLE_COPY_AND_ASSIGN(GraphScheduler);

  // The Global actors contain memory manager actor, recorder actor and debug actor.
  void BuildAndScheduleGlobalActor();

  // Transform the nodes of graph to actors.
  ActorSetPtr Build(const GraphCompilerInfo &graph_compiler_info);
  // Link actors to DAG through the edge connection of graph and graph execution strategy.
  void Link(ActorSet *actor_set, const GraphCompilerInfo &graph_compiler_info);

  // The processing of actors build.
  std::vector<DataSourceActorPtr> BuildDataSourceActor(const GraphCompilerInfo &graph_compiler_info,
                                                       const HostTensorQueuePtr &host_queue);
  std::vector<KernelActorPtr> BuildKernelActor(const GraphCompilerInfo &graph_compiler_info);
  LoopCountActorPtr BuildLoopCountActor(const GraphCompilerInfo &graph_compiler_info);
  OutputActorPtr BuildOutputActor(const GraphCompilerInfo &graph_compiler_info);
  DataPrepareActorPtr BuildDataPrepareActor(const GraphCompilerInfo &graph_compiler_info,
                                            const std::vector<DataSourceActorPtr> &data_source_actors,
                                            const HostTensorQueuePtr &host_queue);
  std::vector<KernelActorPtr> BuildNoInputKernelActor(const ActorSet *actor_set, GraphExecutionStrategy strategy);
  std::vector<SwitchActorPtr> BuildSwitchActor(const GraphCompilerInfo &graph_compiler_info);
  std::vector<GatherActorPtr> BuildGatherActor(const GraphCompilerInfo &graph_compiler_info);

  // Cache the information of graph output node to actor between “build” and “link”, for linking between the tail of
  // previous graph and the head of next graph.
  void CacheGraphOutputToActor(const GraphCompilerInfo &graph_compiler_info);

  // The processing of actors linking.
  // 1. The processing of linking data arrows.
  // The gather of linking data arrows of kernel, it will call following functions by the different from actor type.
  void LinkDataArrow(KernelActor *const to_actor, const GraphCompilerInfo &graph_compiler_info,
                     const KernelGraphPtr &graph, const KernelWithIndex &from_kernel_with_output_idx,
                     const KernelWithIndex &to_kernel_with_input_idx);
  void LinkDataArrowForBaseActor(AbstractActor *const from_actor, KernelActor *const to_actor,
                                 const KernelWithIndex &from_kernel_with_output_idx,
                                 const KernelWithIndex &to_kernel_with_input_idx);
  // Link data arrows for internal parameter, convert internal parameter to actor by internal parameter cache to link.
  void LinkDataArrowForInternalParameter(AbstractActor *const from_actor, KernelActor *const to_actor,
                                         const KernelWithIndex &from_kernel_with_output_idx,
                                         const KernelWithIndex &to_kernel_with_input_idx, const KernelGraphPtr &graph);
  void LinkDataArrowForDeviceTensorStore(AbstractActor *const from_actor, KernelActor *const to_actor,
                                         const KernelWithIndex &from_kernel_with_output_idx,
                                         const KernelWithIndex &to_kernel_with_input_idx, const KernelGraphPtr &graph);
  void LinkDataArrowForDeviceDSActor(AbstractActor *const from_actor, KernelActor *const to_actor,
                                     const KernelWithIndex &from_kernel_with_output_idx,
                                     const KernelWithIndex &to_kernel_with_input_idx, const KernelGraphPtr &graph);
  void LinkDataArrowForHostDSActor(AbstractActor *const from_actor, KernelActor *const to_actor,
                                   const KernelWithIndex &from_kernel_with_output_idx,
                                   const KernelWithIndex &to_kernel_with_input_idx, const KernelGraphPtr &graph);
  void LinkDataArrowForKernelActor(AbstractActor *const from_actor, KernelActor *const to_actor,
                                   const KernelWithIndex &from_kernel_with_output_idx,
                                   const KernelWithIndex &to_kernel_with_input_idx, const KernelGraphPtr &graph);
  // Link data arrows in the copy actor scene, insert the copy actor between from_actor and to_actor.
  void LinkDataArrowForCopyActor(AbstractActor *const from_actor, KernelActor *const to_actor,
                                 const KernelWithIndex &from_kernel_with_output_idx,
                                 const KernelWithIndex &to_kernel_with_input_idx);

  // 2. The processing of linking control arrows.
  void LinkControlArrowByAutoMonad(KernelActor *to_actor, const AnfNodePtr &from_node, const KernelGraphPtr &graph);
  // The skipped node doesn't run, so need link the control arrow between the inputs and user of skipped node.
  void LinkControlArrowBySkippedNode(KernelActor *to_actor, const AnfNodePtr &skipped_node);
  // Link the control arrows for allreduce kernel by the send/recv nodes in the kernel graph.
  void LinkControlArrowBySendRecvNodes(const KernelGraphPtr &graph);

  // The gather of linking the global control arrows, it will call following functions:
  void LinkGlobalControlArrow(ActorSet *const actor_set, const std::vector<CNodePtr> &communication_nodes,
                              const std::vector<KernelActor *> &auto_monad_actors,
                              const GraphCompilerInfo &graph_compiler_info);
  // Link the control arrows by the communication nodes in the kernel graph to ensure communication nodes running order.
  void LinkControlArrowByCommunicationNode(const std::vector<CNodePtr> &communication_nodes,
                                           const GraphCompilerInfo &graph_compiler_info);
  void LinkDeviceTensorStoreForAutoMonadActor(const std::vector<KernelActor *> &auto_monad_actors);
  void LinkControlArrowForDataPrepareActor(DataPrepareActor *data_prepare_actor, const ActorSet *actor_set);
  void LinkControlArrowForLoopCountActor(LoopCountActor *loop_count_actor, const ActorSet *actor_set,
                                         const ControlNodeParserPtr &parser);

  // 3. The processing of linking output result arrows.
  void LinkOutputResultArrowForOutputActor(OutputActor *to_actor, const GraphCompilerInfo &graph_compiler_info);

  // 4. The processing of control flow linking.
  void LinkArrowByControlNode(const GraphCompilerInfo &graph_compiler_info, ActorSet *const actor_set);
  void LinkDataArrowForGatherActor(GatherActor *const from_actor, KernelActor *const to_actor,
                                   const KernelWithIndex &front_node_with_index,
                                   const KernelWithIndex &to_node_with_index);
  void LinkDataArrowForSwitchActor(const GraphCompilerInfo &graph_compiler_info, SwitchActor *const actor);
  // Connect the input of the actor.
  void LinkDataArrowByControlNode(const GraphCompilerInfo &graph_compiler_info, const KernelWithIndex &input_node,
                                  const FuncGraphPtr &from_func_graph, OpActor<DeviceTensor> *const to_actor,
                                  const size_t to_index);
  // When the input of the actor is a call node, the output of the funcgraph called by the call node needs to be
  // connected.
  void LinkDataArrowByCallInput(const KernelWithIndex &call_node_with_index, const ControlNodeParserPtr &parser,
                                const FuncGraphPtr &from_func_graph, OpActor<DeviceTensor> *const to_actor,
                                const size_t to_index);
  void LinkDataArrowForSwitchActor(SwitchActor *const from_actor, const size_t from_index,
                                   OpActor<DeviceTensor> *const to_actor, const size_t to_index,
                                   const size_t branch_index = SIZE_MAX);

  void LinkControlArrowForGatherActor(std::vector<KernelActorPtr> *const kernel_actors,
                                      const std::vector<KernelGraphPtr> &graphs, const ControlNodeParserPtr &parser);

  void LinkControlArrowForSwitchActor(std::vector<SwitchActorPtr> *const switch_actors, LoopCountActor *const to_actor,
                                      const KernelMapPosition &origin_outputs_order);
  // In control flow, there are scenarios where there are multi-branch outputs, and the gather actor needs to
  // send the branch id to the loop count actor.
  void LinkBranchArrowForSwitchActor(const GraphCompilerInfo &graph_compiler_info);
  void LinkBranchArrowForGatherActor(const GraphCompilerInfo &graph_compiler_info);
  void LinkOutputResultArrowForSwitchActor(const GraphCompilerInfo &graph_compiler_info, const ActorSet *actor_set);
  // Add input for switch actor. Since part of the input of funcgraph is on call node, these inputs need to be added
  // to switch actor.
  void PrepareInputNodeForSwitchActor(const std::vector<AnfNodePtr> &control_nodes);

  // Check whether the actor set is valid.
  bool CheckActorValid(const ActorSet *actor_set,
                       GraphExecutionStrategy strategy = GraphExecutionStrategy::kPipeline) const;

  // Persist device tensors of graph's some nodes(such as weights and value nodes).
  void PersistDeviceTensor(const GraphCompilerInfo &graph_compiler_info);

  // The fetch results are kernel_type and kernel_name.
  void FetchKernelTransformTypeAndName(const AnfNodePtr &node, const KernelGraphPtr &graph,
                                       const GraphCompilerInfo &graph_compiler_info,
                                       KernelTransformType *const kernel_type, std::string *const kernel_name);

  // The operation of the map of actor_name_to_actor_.
  void InsertActor(OpActor<DeviceTensor> *actor);
  OpActor<DeviceTensor> *FetchActor(const std::string &actor_name) const;

  // Display the actor information of corresponding kernel graph.
  void DumpActor(const ActorSet *actor_set, const GraphCompilerInfo &graph_compiler_info) const;
  void DumpAbstractActor(const AbstractActor *actor, std::ofstream &ofs) const;
  void DumpDataPrepareActor(const DataPrepareActor *actor, std::ofstream &ofs) const;
  void DumpDSActor(const DataSourceActor *actor, std::ofstream &ofs) const;
  void DumpLoopCountActor(const LoopCountActor *actor, std::ofstream &ofs) const;
  void DumpKernelActor(const KernelActor *actor, std::ofstream &ofs) const;
  void DumpOutputActor(const OutputActor *actor, std::ofstream &ofs) const;
  void DumpCopyActor(const CopyActor *actor, std::ofstream &ofs) const;
  void DumpGatherActor(const GatherActor *actor, std::ofstream &ofs) const;
  void DumpSwitchActor(const SwitchActor *actor, std::ofstream &ofs) const;
  void DumpDeviceTensorStore(const GraphCompilerInfo &graph_compiler_info, std::ofstream &ofs) const;

  // The global maps, only be cleared in the deconstruction.
  std::unordered_map<ActorInfo, ActorSetPtr> actors_;
  std::unordered_map<std::string, OpActor<DeviceTensor> *> actor_name_to_actor_;

  // The local maps and vectors, will be cleared at the end of each graph transform:
  // 1.The second element of pair represents the output index of op actor corresponding to the graph output front node.
  std::map<KernelWithIndex, GraphOutputPair, session::KernelWithIndexCmp> graph_output_to_actor_;
  // 2.Since the control node does not have a backend node, it can only be connected through the relationship between
  // the front node, so the mapping relationship between the front node and the actor needs to be recorded.
  std::unordered_map<AnfNodePtr, KernelActorPtr> front_node_to_actor_;
  // 3.Beaceuse the copy actors are built in the link, so need record the all copy actors in the link process to push
  // into the actor set after link.
  std::vector<CopyActorPtr> copy_actors_;

  // The id of global actor.
  AID memory_manager_aid_;
  const AID *recorder_aid_{nullptr};
  const AID *debug_aid_{nullptr};

  bool init_{false};
};
}  // namespace runtime
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_SCHEDULER_H_
