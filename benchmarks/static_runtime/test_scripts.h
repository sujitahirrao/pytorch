#pragma once

#include <torch/torch.h>

const auto list_construct_script = R"JIT(
  def forward(self, a, b):
    return [a, b]
)JIT";

const auto list_construct_script_2 = R"JIT(
  def forward(self, a, b):
    c = a + a
    return [c, c]
)JIT";

const auto list_construct_script_3 = R"JIT(
  def forward(self, a, b):
    c = a + a
    return [c, c.flatten()]
)JIT";

const auto list_unpack_script = R"JIT(
  def forward(self, a, b):
    c = [a, b]
    x, y = c
    z = x + y
    return z
)JIT";

const auto list_unpack_script_2 = R"JIT(
  def forward(self, a, b):
    c = [a, b]
    x, y = c
    z = (x, y)
    return z
)JIT";

const auto tuple_construct_script = R"JIT(
  def forward(self, a, b):
    return (a, b)
)JIT";

const auto tuple_construct_script_2 = R"JIT(
  def forward(self, a, b):
    return (a.flatten(), b)
)JIT";

const auto add_script = R"JIT(
  def forward(self, a, b):
      return a + b
)JIT";

const auto reshape_script_1 = R"JIT(
  def forward(self, a: Tensor, shape: List[int]):
      b = a.reshape(shape)
      return b + b
)JIT";

const auto reshape_script_2 = R"JIT(
  def forward(self, a: Tensor, shape: List[int]):
      b = a.transpose(0, 1)
      return b.reshape(shape)
)JIT";

const auto reshape_script_3 = R"JIT(
  def forward(self, inp: Tensor, shape: List[int]):
      a = inp + inp
      b = a.reshape(shape)
      c = a.reshape(shape)
      d = c + c
      e = d + d
      f = e * e
      g = f * f
      return b.reshape(shape), g
)JIT";

const auto reshape_script_4 = R"JIT(
  def forward(self, inp: Tensor, shape: List[int]):
      k = inp + inp
      a = k + k
      b = a.reshape(shape)
      c = a.flatten().reshape(shape)
      return b + c
)JIT";

const auto reshape_script_5 = R"JIT(
  def forward(self, inp: Tensor, shape: List[int]):
      a = inp + inp
      b = a.reshape(shape)
      c = a.reshape(shape)
      d = c + c
      e = d + d
      f = e * e
      g = f * f
      return g
)JIT";

const auto flatten_script_1 = R"JIT(
  def forward(self, a: Tensor, start_dim: int, end_dim: int):
      b = torch.flatten(a, start_dim, end_dim)
      return b + b
)JIT";

const auto flatten_script_2 = R"JIT(
  def forward(self, a: Tensor, start_dim: int, end_dim: int):
      b = a.transpose(0, 1)
      return torch.flatten(b, start_dim, end_dim)
)JIT";

const auto aten_sum = R"JIT(
  def forward(self, input):
      return torch.sum(input)
)JIT";

const auto aten_sum_0 = R"JIT(
  def forward(self, input):
      return torch.sum(input, 0)
)JIT";

const auto aten_sum_1 = R"JIT(
  def forward(self, input):
      return torch.sum(input, 1)
)JIT";

const auto aten_sum_0_true = R"JIT(
  def forward(self, input):
      return torch.sum(input, 0, True)
)JIT";

const auto aten_sum_1_true = R"JIT(
  def forward(self, input):
      return torch.sum(input, 1, True)
)JIT";
