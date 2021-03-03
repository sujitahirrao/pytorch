#include <torch/csrc/jit/runtime/static/passes.h>

#include <torch/csrc/jit/ir/alias_analysis.h>
#include <torch/csrc/jit/passes/subgraph_rewrite.h>

namespace torch {
namespace jit {

void ConcatAddMulReplaceNaNClip(std::shared_ptr<torch::jit::Graph>& graph) {
  // TODO:: check restrictions for inputs; outputs not used elsewhere
  std::string pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g, %h, %i, %j):
        %y0 = aten::cat(%a, %b)
        %y1 = aten::add(%y0, %c, %d)
        %y2 = aten::mul(%y1, %e)
        %y3 = aten::nan_to_num(%y2, %f, %g, %h)
        %res = aten::clamp(%y3, %i, %j)
        return (%res))IR";
  std::string pattern2 = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g, %h, %i, %j):
        %y0 = aten::cat(%a, %b)
        %y1 = aten::add(%y0, %c, %d)
        %y2 = aten::mul(%y1, %e)
        %y3 = aten::nan_to_num_(%y2, %f, %g, %h)
        %res = aten::clamp(%y3, %i, %j)
        return (%res))IR";
  std::string pattern3 = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g, %h, %i, %j):
        %y0 = aten::cat(%a, %b)
        %y1 = aten::add(%y0, %c, %d)
        %y2 = aten::mul(%y1, %e)
        %y3 = aten::nan_to_num_(%y2, %f, %g, %h)
        %res = aten::clamp_(%y3, %i, %j)
        return (%res))IR";
  std::string pattern4 = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g, %h, %i, %j):
        %y0 = aten::cat(%a, %b)
        %y1 = aten::add(%y0, %c, %d)
        %y2 = aten::mul(%y1, %e)
        %y3 = aten::nan_to_num(%y2, %f, %g, %h)
        %res = aten::clamp_(%y3, %i, %j)
        return (%res))IR";
  std::string fused_pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g, %h, %i, %j):
        %res = fb::concat_add_mul_replacenan_clip(%c, %e, %a, %i, %j)
        return (%res))IR";

  SubgraphRewriter fuse;
  fuse.RegisterRewritePattern(pattern, fused_pattern);
  fuse.runOnGraph(graph);

  fuse.RegisterRewritePattern(pattern2, fused_pattern);
  fuse.runOnGraph(graph);

  fuse.RegisterRewritePattern(pattern3, fused_pattern);
  fuse.runOnGraph(graph);

  fuse.RegisterRewritePattern(pattern4, fused_pattern);
  fuse.runOnGraph(graph);
}

void CastedBatchOneHotLengths(std::shared_ptr<torch::jit::Graph>& graph) {
  // TODO:: check restrictions for inputs; outputs not used elsewhere
  std::string pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g):
        %y0 : Tensor = aten::to(%a, %b, %c, %c, %d)
        %y1 : Tensor = fb::batch_one_hot_lengths(%y0, %e, %f)
        %res : Tensor = aten::to(%y1, %g, %c, %c, %d)
        return (%res))IR";
  std::string fused_pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g):
        %res : Tensor = fb::casted_batch_one_hot_lengths(%a, %e, %f)
        return (%res))IR";
  SubgraphRewriter fuse;
  fuse.RegisterRewritePattern(pattern, fused_pattern);
  fuse.runOnGraph(graph);
}

void ConcatBatchMatMulBatchGather(std::shared_ptr<torch::jit::Graph>& graph) {
  // TODO:: check restrictions for inputs; outputs not used elsewhere
  std::string pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f):
        %y0 : Tensor = aten::stack(%a, %b)
        %y1 : Tensor = aten::transpose(%y0, %b, %c)
        %y2 : Tensor = aten::bmm(%y0, %y1)
        %y3 : Tensor = aten::flatten(%y2, %d, %e)
        %res : Tensor = aten::index_select(%y3, %b, %f)
        return (%res))IR";
  std::string fused_pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f):
        %res : Tensor = fb::concat_batch_matmul_batch_gather(%f, %a)
        return (%res))IR";
  SubgraphRewriter fuse;
  fuse.RegisterRewritePattern(pattern, fused_pattern);
  fuse.runOnGraph(graph);
}

void ClipRangesGatherRangesLengthsToOffsets(
    std::shared_ptr<torch::jit::Graph>& graph) {
  // TODO:: check restrictions for inputs; outputs not used elsewhere
  std::string pattern = R"IR(
    graph(%a, %b, %c, %d):
        %y0 : Tensor = fb::clip_ranges(%b, %c)
        %y1 : Tensor, %y2 : Tensor = fb::gather_ranges(%a, %y0)
        %y3 : Tensor = fb::lengths_to_offsets(%y2, %d)
        return (%y3, %y1))IR";
  std::string fused_pattern = R"IR(
    graph(%a, %b, %c, %d):
        %y0 : Tensor, %y1 : Tensor = fb::clip_ranges_gather_lengths_to_offsets(%a, %b, %c, %d)
        return (%y1, %y0))IR";
  SubgraphRewriter fuse;
  fuse.RegisterRewritePattern(pattern, fused_pattern);
  fuse.runOnGraph(graph);
}

void ClipRangesGather(std::shared_ptr<torch::jit::Graph>& graph) {
  // TODO:: check restrictions for inputs; outputs not used elsewhere
  // fuse without lengths-to-offsets
  std::string pattern = R"IR(
    graph(%a, %b, %c):
        %y0 : Tensor = fb::clip_ranges(%b, %c)
        %y1 : Tensor, %y2 : Tensor = fb::gather_ranges(%a, %y0)
        return (%y2, %y1))IR";
  std::string fused_pattern = R"IR(
    graph(%a, %b, %c):
        %y0 : Tensor, %y1 : Tensor = fb::clip_ranges_gather(%a, %b, %c)
        return (%y1, %y0))IR";
  SubgraphRewriter fuse;
  fuse.RegisterRewritePattern(pattern, fused_pattern);
  fuse.runOnGraph(graph);
}

void ClipRangesGatherSigridHash(std::shared_ptr<torch::jit::Graph>& graph) {
  // TODO:: check restrictions for inputs; outputs not used elsewhere
  std::string pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g):
        %y0 : Tensor, %y1 : Tensor = fb::clip_ranges_gather_lengths_to_offsets(%a, %b, %c, %d)
        %y2 : Tensor = fb::sigrid_hash(%y0, %e, %f, %g)
        return (%y2, %y1))IR";
  std::string fused_pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f, %g):
        %off : Tensor, %out : Tensor = fb::clip_ranges_gather_sigrid_hash_offsets(%b, %a, %c, %e, %f, %g, %d)
        return (%out, %off))IR";
  SubgraphRewriter fuse;
  fuse.RegisterRewritePattern(pattern, fused_pattern);
  fuse.runOnGraph(graph);
}

void ClipRangesGatherRangesSigridHash(
    std::shared_ptr<torch::jit::Graph>& graph) {
  std::string pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f):
        %y0 : Tensor = fb::clip_ranges(%b, %c)
        %y1 : Tensor, %y2 : Tensor = fb::gather_ranges(%a, %y0)
        %y3 : Tensor = fb::sigrid_hash(%y1, %d, %e, %f)
        return (%y3, %y2))IR";
  std::string fused_pattern = R"IR(
    graph(%a, %b, %c, %d, %e, %f):
        %off : Tensor, %out : Tensor = fb::clip_ranges_gather_sigrid_hash_v3(%b, %a, %c, %d, %e, %f)
        return (%out, %off))IR";
  SubgraphRewriter fuse;
  fuse.RegisterRewritePattern(pattern, fused_pattern);
  fuse.runOnGraph(graph);
}

void FuseInferenceOpsForSparseNN(std::shared_ptr<torch::jit::Graph>& graph) {
#ifdef FBCODE_CAFFE2
  ConcatAddMulReplaceNaNClip(graph);
  CastedBatchOneHotLengths(graph);
  ConcatBatchMatMulBatchGather(graph);

  ClipRangesGatherRangesLengthsToOffsets(graph);
  ClipRangesGatherSigridHash(graph);
  ClipRangesGatherRangesSigridHash(graph);

  // prioritize clip_ranges+gather_ranges+sigrid_hash fusion over
  // clip_ranges+gather_ranges
  ClipRangesGather(graph);
#endif
}

TORCH_LIBRARY_FRAGMENT(static_runtime, m) {
  m.def("static_runtime::pure_inputs() -> Tensor", []() -> at::Tensor {
    return at::randn({1});
  });
  m.def(
      "static_runtime::permute_copy(Tensor self, int[] dims) -> Tensor",
      [](at::Tensor self, ArrayRef<int64_t> dims) -> at::Tensor {
        at::Tensor out = at::empty_like(self);
        at::native::copy_(out, self);
        return out.permute(dims);
      });
}

void ReplaceWithCopy(std::shared_ptr<torch::jit::Graph>& graph) {
  auto* fake_input =
      graph->insert(Symbol::fromQualString("static_runtime::pure_inputs"), {});
  fake_input->node()->moveBefore(*graph->nodes().begin());

  std::vector<std::pair<Value*, Use>> old_inputs;
  for (auto* input : graph->inputs()) {
    for (const auto& use : input->uses()) {
      old_inputs.emplace_back(std::make_pair(input, use));
    }
    input->replaceAllUsesWith(fake_input);
  }

  AliasDb db(graph);
  for (const auto& p : old_inputs) {
    p.second.user->replaceInput(p.second.offset, p.first);
  }
  fake_input->node()->destroy();

  const std::map<c10::Symbol, c10::Symbol> supported = {
      {c10::Symbol::fromQualString("aten::permute"),
       c10::Symbol::fromQualString("static_runtime::permute_copy")},
      {c10::Symbol::fromQualString("aten::narrow"),
       c10::Symbol::fromQualString("aten::narrow_copy")}};
  std::vector<std::pair<Node*, Node*>> replacement;
  for (auto* n : graph->nodes()) {
    if (!supported.count(n->kind())) {
      continue;
    }
    DCHECK(n->outputs().size() == 1);
    auto* out = n->output();
    if (out->uses().size() > 1) {
      continue;
    }
    if (db.mayContainAlias({out}, graph->outputs())) {
      continue;
    }
    auto new_symbol = supported.at(n->kind());
    auto* new_node = graph->create(new_symbol, n->outputs().size());
    new_node->insertBefore(n);
    for (auto* input : n->inputs()) {
      new_node->addInput(input);
    }
    replacement.emplace_back(std::make_pair(n, new_node));
  }

  for (const auto& p : replacement) {
    auto* old_node = p.first;
    auto* new_node = p.second;
    old_node->replaceAllUsesWith(new_node);
    old_node->destroy();
  }
}

} // namespace jit
} // namespace torch
