#include <ATen/ATen.h>
#include <ATen/native/ForeachUtils.h>

namespace at { namespace native {

#define FOREACH_BINARY_OP_SCALAR(OP)                                                                      \
void foreach_tensor_##OP##_scalar_kernel_slow_(TensorList tensors, Scalar scalar) {                       \
  check_foreach_api_restrictions(tensors);                                                                \
                                                                                                          \
  for (auto& t: tensors) {                                                                                \
    t.OP##_(scalar);                                                                                      \
  }                                                                                                       \
}                                                                                                         \
                                                                                                          \
std::vector<Tensor> foreach_tensor_##OP##_scalar_kernel_slow(TensorList tensors, Scalar scalar) {         \
  check_foreach_api_restrictions(tensors);                                                                \
                                                                                                          \
  std::vector<Tensor> result;                                                                             \
  result.reserve(tensors.size());                                                                         \
  for (const auto& t: tensors) {                                                                          \
    result.emplace_back(t.OP(scalar));                                                                    \
  }                                                                                                       \
                                                                                                          \
  return result;                                                                                          \
}

#define FOREACH_BINARY_OP_SCALARLIST(OP)                                                                                \
void foreach_tensor_##OP##_scalarlist_kernel_slow_(TensorList tensors, at::ArrayRef<double> scalars) {                  \
  check_foreach_api_restrictions(tensors, scalars);                                                                     \
                                                                                                                        \
  for (int i = 0; i < tensors.size(); i++) {                                                                            \
      tensors[i].OP##_(scalars[i]);                                                                                     \
    }                                                                                                                   \
}                                                                                                                       \
                                                                                                                        \
std::vector<Tensor> foreach_tensor_##OP##_scalarlist_kernel_slow(TensorList tensors, at::ArrayRef<double> scalars) {    \
  check_foreach_api_restrictions(tensors, scalars);                                                                     \
  std::vector<Tensor> result;                                                                                           \
  result.reserve(tensors.size());                                                                                       \
  for (int i = 0; i < tensors.size(); i++) {                                                                            \
    result.emplace_back(tensors[i].OP(scalars[i]));                                                                     \
  }                                                                                                                     \
                                                                                                                        \
  return result;                                                                                                        \
}

#define FOREACH_BINARY_OP_LIST(OP)                                                                        \
std::vector<Tensor> foreach_tensor_##OP##_list_kernel_slow(TensorList tensors1, TensorList tensors2) {    \
  check_foreach_api_restrictions(tensors1, tensors2);                                                     \
                                                                                                          \
  std::vector<Tensor> result;                                                                             \
  result.reserve(tensors1.size());                                                                        \
  for (int i = 0; i < tensors1.size(); i++) {                                                             \
    result.emplace_back(tensors1[i].OP(tensors2[i]));                                                     \
  }                                                                                                       \
                                                                                                          \
  return result;                                                                                          \
}                                                                                                         \
                                                                                                          \
void foreach_tensor_##OP##_list_kernel_slow_(TensorList tensors1, TensorList tensors2) {                  \
  check_foreach_api_restrictions(tensors1, tensors2);                                                     \
                                                                                                          \
  for (int i = 0; i < tensors1.size(); i++) {                                                             \
    tensors1[i].OP##_(tensors2[i]);                                                                       \
  }                                                                                                       \
}

#define FOREACH_BINARY_OP_LIST_ALPHA(OP)                                                                                \
std::vector<Tensor> foreach_tensor_##OP##_list_kernel_slow(TensorList tensors1, TensorList tensors2, Scalar alpha) {    \
  check_foreach_api_restrictions(tensors1, tensors2);                                                                   \
                                                                                                                        \
  std::vector<Tensor> result;                                                                                           \
  result.reserve(tensors1.size());                                                                                      \
  for (int i = 0; i < tensors1.size(); i++) {                                                                           \
    result.emplace_back(tensors1[i].OP(tensors2[i], alpha));                                                            \
  }                                                                                                                     \
                                                                                                                        \
  return result;                                                                                                        \
}                                                                                                                       \
                                                                                                                        \
void foreach_tensor_##OP##_list_kernel_slow_(TensorList tensors1, TensorList tensors2, Scalar alpha) {                  \
  check_foreach_api_restrictions(tensors1, tensors2);                                                                   \
                                                                                                                        \
  for (int i = 0; i < tensors1.size(); i++) {                                                                           \
    tensors1[i].OP##_(tensors2[i], alpha);                                                                              \
  }                                                                                                                     \
}

#define FOREACH_UNARY_OP(OP)                                               \
std::vector<Tensor> foreach_tensor_##OP##_slow(TensorList tensors) {       \
  check_foreach_api_restrictions(tensors);                                 \
                                                                           \
  std::vector<Tensor> result;                                              \
  result.reserve(tensors.size());                                          \
  for (const auto& t : tensors) {                                          \
    result.emplace_back(t.OP());                                           \
  }                                                                        \
                                                                           \
  return result;                                                           \
}                                                                          \
                                                                           \
void foreach_tensor_##OP##_slow_(TensorList tensors) {                     \
  check_foreach_api_restrictions(tensors);                                 \
                                                                           \
  for (auto& t : tensors) {                                                \
    t.OP##_();                                                             \
  }                                                                        \
}

#define FOREACH_POINTWISE_OP_SCALAR(OP)                                                                                              \
std::vector<Tensor> foreach_tensor_##OP##_scalar_slow(TensorList input, TensorList tensors1, TensorList tensors2, Scalar scalar) {   \
  check_nonempty_and_same_length(input, tensors1, tensors2);                                                                         \
                                                                                                                                     \
  std::vector<Tensor> result;                                                                                                        \
  for (int i = 0; i < input.size(); i++) {                                                                                           \
    result.emplace_back(input[i].OP(tensors1[i], tensors2[i], scalar));                                                              \
  }                                                                                                                                  \
                                                                                                                                     \
  return result;                                                                                                                     \
}                                                                                                                                    \
                                                                                                                                     \
void foreach_tensor_##OP##_scalar_slow_(TensorList input, TensorList tensors1, TensorList tensors2, Scalar scalar) {                 \
  check_nonempty_and_same_length(input, tensors1, tensors2);                                                                         \
                                                                                                                                     \
  for (int i = 0; i < input.size(); i++) {                                                                                           \
    input[i].OP##_(tensors1[i], tensors2[i], scalar);                                                                                \
  }                                                                                                                                  \
}                                                                                                                                    \

#define FOREACH_POINTWISE_OP_SCALARLIST(OP)                                                                                                             \
std::vector<Tensor> foreach_tensor_##OP##_scalarlist_slow(TensorList input, TensorList tensors1, TensorList tensors2, at::ArrayRef<double> scalars) {   \
  check_nonempty_and_same_length(input, tensors1, tensors2, scalars);                                                                                   \
                                                                                                                                                        \
  std::vector<Tensor> result;                                                                                                                           \
  for (int i = 0; i < input.size(); i++) {                                                                                                              \
    result.emplace_back(input[i].OP(tensors1[i], tensors2[i], scalars[i]));                                                                             \
  }                                                                                                                                                     \
                                                                                                                                                        \
  return result;                                                                                                                                        \
}                                                                                                                                                       \
                                                                                                                                                        \
void foreach_tensor_##OP##_scalarlist_slow_(TensorList input, TensorList tensors1, TensorList tensors2, at::ArrayRef<double> scalars) {                 \
  check_nonempty_and_same_length(input, tensors1, tensors2, scalars);                                                                                   \
                                                                                                                                                        \
  for (int i = 0; i < input.size(); i++) {                                                                                                              \
    input[i].OP##_(tensors1[i], tensors2[i], scalars[i]);                                                                                               \
  }                                                                                                                                                     \
}                                                                                                                                                       \

FOREACH_BINARY_OP_LIST_ALPHA(add);
FOREACH_BINARY_OP_LIST_ALPHA(sub);
FOREACH_BINARY_OP_SCALAR(add);
FOREACH_BINARY_OP_SCALAR(sub);
FOREACH_BINARY_OP_SCALAR(mul);
FOREACH_BINARY_OP_SCALAR(div);
FOREACH_BINARY_OP_SCALARLIST(add);
FOREACH_BINARY_OP_SCALARLIST(sub);
FOREACH_BINARY_OP_SCALARLIST(mul);
FOREACH_BINARY_OP_SCALARLIST(div);
FOREACH_BINARY_OP_LIST(mul);
FOREACH_BINARY_OP_LIST(div);
FOREACH_UNARY_OP(sqrt);
FOREACH_UNARY_OP(exp);
FOREACH_POINTWISE_OP_SCALAR(addcdiv);
FOREACH_POINTWISE_OP_SCALAR(addcmul);
FOREACH_POINTWISE_OP_SCALARLIST(addcdiv);
FOREACH_POINTWISE_OP_SCALARLIST(addcmul);

#define FOREACH_MAXIMUM_MINIMUM_OP(NAME)                                                     \
std::vector<Tensor> foreach_tensor_##NAME##_slow(TensorList tensors1, TensorList tensors2) { \
  check_foreach_api_restrictions(tensors1, tensors2);                                        \
                                                                                             \
  std::vector<Tensor> result;                                                                \
  result.reserve(tensors1.size());                                                           \
  for (int i = 0; i < tensors1.size(); i++) {                                                \
    result.emplace_back(at::NAME(tensors1[i], tensors2[i]));                                 \
  }                                                                                          \
                                                                                             \
  return result;                                                                             \
}                                                                                            \

FOREACH_MAXIMUM_MINIMUM_OP(maximum)
FOREACH_MAXIMUM_MINIMUM_OP(minimum)

}} // namespace at::native
