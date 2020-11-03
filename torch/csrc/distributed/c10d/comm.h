#pragma once

#include <ATen/ATen.h>
#include <ATen/core/ivalue.h>
#include <c10d/ProcessGroup.hpp>

namespace c10d {

// Broadcast many tensors to all processes in the process group.
void broadcast_coalesced(
    std::shared_ptr<c10d::ProcessGroup> process_group,
    at::TensorList tensors,
    size_t buffer_size,
    int rank = 0);

// This class passes bucket contents tensor (for multiple replicas) to
// DDP communication hook.
// Optionally in the future this can be enhanced with parameter to bucket
// mappings as well.
class GradBucket {
 public:
  explicit GradBucket(const std::vector<at::Tensor>& tensors)
      : tensors_(tensors) {}
  // Each tensor in the list that getTensors returns refers to the replica on
  // each device. There will be multiple replicas only in the case of single
  // process multiple device mode. In the single process single device mode,
  // this list would consist of only a single tensor.
  const std::vector<at::Tensor>& getTensors() const {
    return tensors_;
  }

  std::vector<at::Tensor>& getTensorsRef() {
    return tensors_;
  }

 private:
  std::vector<at::Tensor> tensors_;
};

// Base class of both `PythonCommHook` and `CppCommHook`.
// Requires implementing 1) `runHook` method that communicates gradients
// asynchronously, and 2) `parseHookResult` method that converts the hook result
// into a tensor vector.
class TORCH_PYTHON_API CommHookInterface {
 public:
  virtual ~CommHookInterface() {}

  // Passes the input grad bucket to the registered communication hook.
  // Once the tensors in the bucket are ready, kicks off the hook asynchronously
  // and returns a future that holds the communication results.
  virtual c10::intrusive_ptr<c10::ivalue::Future> runHook(
      GradBucket& bucket) = 0;

  // Returns the resulting tensors once the communication hook result is ready.
  // The resulting tensors will then be copied to the grads of individual
  // parameters.
  virtual std::vector<at::Tensor> parseHookResult(
      const c10::IValue& result) = 0;
};

// This CppCommHook interface only requires implementing runHook method that
// potentially uses a state.
template <typename T>
class TORCH_API CppCommHookInterface : public CommHookInterface {
 public:
  explicit CppCommHookInterface(T& state) : state_(state) {}

  virtual ~CppCommHookInterface() {}

  std::vector<at::Tensor> parseHookResult(const c10::IValue& result) override {
    TORCH_INTERNAL_ASSERT(
        result.isTensor() || result.isTensorList(),
        "expected the hook result is either a Tensor or a TensorList");

    if (result.isTensor()) {
      return {result.toTensor()};
    }

    return result.toTensorVector();
  }

 protected:
  T state_; // Not owned.
};

} // namespace c10d
