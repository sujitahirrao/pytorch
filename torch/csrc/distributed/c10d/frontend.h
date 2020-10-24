#pragma once

#include <ATen/ATen.h>
#include <c10/util/Optional.h>
#include <torch/lib/c10d/ProcessGroup.hpp>
#include <torch/lib/c10d/Store.hpp>
#include <torch/lib/c10d/Types.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace c10d {

class Backend {
 public:
  // Maps to Backend.__new__ in Python.
  static std::string get(std::string);

  // TODO: How to support registering third_party backend?
  static void registerBackend();

 private:
  // TODO: Should this be an enum list instead since this set doesn't
  // change at all.
  std::unordered_set<std::string> registered_backends_;
};

class DistributedC10d {
 public:
  void initProcessGroup(
      const std::string& backend,
      const std::string& init_method,
      const std::chrono::milliseconds& timeout,
      int64_t world_size,
      int64_t rank,
      std::shared_ptr<Store> store,
      const std::string& group_name);

  void destroyProcessGroup(std::shared_ptr<ProcessGroup> group);
  int64_t getRank(const std::shared_ptr<ProcessGroup>& group) const;
  int64_t getWorldSize(const std::shared_ptr<ProcessGroup>& group) const;

  std::shared_ptr<ProcessGroup::Work> isend(
      at::Tensor tensor,
      int64_t dst,
      const std::shared_ptr<ProcessGroup>& group,
      c10::optional<int64_t>& tag);

  std::shared_ptr<ProcessGroup::Work> irecv(
      at::Tensor tensor,
      int64_t src,
      const std::shared_ptr<ProcessGroup>& group,
      c10::optional<int64_t>& tag);

  void send(
      at::Tensor tensor,
      int64_t dst,
      const std::shared_ptr<ProcessGroup>& group,
      c10::optional<int64_t>& tag);

  int64_t recv(
      at::Tensor tensor,
      const c10::optional<int64_t>& src,
      const std::shared_ptr<ProcessGroup>& group,
      c10::optional<int64_t>& tag);

  std::shared_ptr<ProcessGroup::Work> broadcastMultiGPU(
      std::vector<at::Tensor>& tensor_list,
      int64_t src,
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false,
      int64_t src_tensor = 0);

  std::shared_ptr<ProcessGroup::Work> broadcast(
      at::Tensor tensor,
      int64_t src,
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allReduceMultiGPU(
      std::vector<at::Tensor>& tensor_list,
      const std::shared_ptr<ProcessGroup>& group,
      ReduceOp op = ReduceOp::SUM,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allReduce(
      at::Tensor tensor,
      const std::shared_ptr<ProcessGroup>& group,
      ReduceOp op = ReduceOp::SUM,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allReduceCoalesced(
      std::vector<at::Tensor>& tensors,
      const std::shared_ptr<ProcessGroup>& group,
      ReduceOp op = ReduceOp::SUM,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> reduceMultiGPU(
      std::vector<at::Tensor>& tensor_list,
      int64_t dst,
      const std::shared_ptr<ProcessGroup>& group,
      ReduceOp op = ReduceOp::SUM,
      bool async_op = false,
      int64_t dst_tensor = 0);

  std::shared_ptr<ProcessGroup::Work> reduce(
      at::Tensor tensor,
      int64_t dst,
      const std::shared_ptr<ProcessGroup>& group,
      ReduceOp op = ReduceOp::SUM,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allGatherMultiGPU(
      std::vector<std::vector<at::Tensor>>& output_tensor_lists,
      std::vector<at::Tensor>& input_tensor_list,
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allGather(
      std::vector<at::Tensor>& tensor_list,
      at::Tensor tensor,
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allGatherCoalesced(
      std::vector<std::vector<at::Tensor>>& output_tensor_lists,
      std::vector<at::Tensor>& input_tensor_list,
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> gather(
      at::Tensor tensor,
      const c10::optional<std::vector<at::Tensor>>& gather_list,
      const std::shared_ptr<ProcessGroup>& group,
      int64_t dst = 0,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> scatter(
      at::Tensor tensor,
      std::vector<at::Tensor>& scatter_list,
      const std::shared_ptr<ProcessGroup>& group,
      int64_t src = 0,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> reduceScatterMultiGPU(
      std::vector<at::Tensor>& output_tensor_list,
      std::vector<std::vector<at::Tensor>>& input_tensor_lists,
      const std::shared_ptr<ProcessGroup>& group,
      ReduceOp op = ReduceOp::SUM,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> reduceScatter(
      at::Tensor output,
      std::vector<at::Tensor>& input_tensor_list,
      const std::shared_ptr<ProcessGroup>& group,
      ReduceOp op = ReduceOp::SUM,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allToAllSingle(
      at::Tensor output,
      at::Tensor input,
      std::vector<int64_t>& output_split_sizes,
      std::vector<int64_t>& input_split_sizes,
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> allToAll(
      std::vector<at::Tensor>& output_tensor_list,
      std::vector<at::Tensor>& input_tensor_list,
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false);

  std::shared_ptr<ProcessGroup::Work> barrier(
      const std::shared_ptr<ProcessGroup>& group,
      bool async_op = false);

  std::shared_ptr<ProcessGroup> newGroup(
      std::vector<int64_t> ranks,
      std::chrono::milliseconds timeout,
      Backend backend);

  std::shared_ptr<ProcessGroup> worldProcessGroup();

 private:
  DistributedC10d(){};

  bool rankNotInGroup(const std::shared_ptr<ProcessGroup>& group) const;
  int64_t getGroupRank(
      const std::shared_ptr<ProcessGroup>& group,
      const int64_t rank) const;
  int64_t getGlobalRank(
      const std::shared_ptr<ProcessGroup>& group,
      const int64_t group_rank) const;
  void checkDefaultPg() const;
  int64_t getGroupSize(const std::shared_ptr<ProcessGroup>& group) const;
  std::string getBackend(const std::shared_ptr<ProcessGroup>& group);

  std::string backend_;
  // TODO: Ask Alex what kind of equality we need. It determine whether we
  // need to use ProcessGroup or ProcesGroup* as key.
  std::unordered_map<
      std::shared_ptr<ProcessGroup>,
      std::pair<std::string, std::shared_ptr<Store>>>
      pg_map_;

  // Note, this is different mapping relationship than original Python
  // implementation.
  std::unordered_map<std::shared_ptr<ProcessGroup>, std::string> pg_names_;

  // Process group's global rank to local rank mapping
  std::unordered_map<
      std::shared_ptr<ProcessGroup>,
      std::unordered_map<int64_t, int64_t>>
      pg_group_ranks_;

  std::shared_ptr<ProcessGroup> default_pg_;

  // Default value should be "env://"
  std::string default_pg_init_method_;

  int64_t group_count_;
};

} // namespace c10d
