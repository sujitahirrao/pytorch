import os
import sys

import torch
from torch.fx import symbolic_trace, subgraph_rewriter

# Make the helper files in test/ importable
pytorch_test_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
sys.path.append(pytorch_test_dir)
from torch.testing._internal.jit_utils import JitTestCase

if __name__ == '__main__':
    raise RuntimeError("This test file is not meant to be run directly, use:\n\n"
                       "\tpython test/test_fx.py TESTNAME\n\n"
                       "instead.")

class TestSubgraphRewriter(JitTestCase):

    def test_subgraph_rewriter_preserves_logic(self):
        class M(torch.nn.Module):
            def forward(self, x):
                val = torch.neg(x) + torch.relu(x)
                return torch.add(val, val)

        def pattern(x):
            return torch.neg(x) + torch.relu(x)

        def comparison(x):
            val = torch.neg(x) + torch.relu(x)
            return torch.add(val, val)

        traced_module = symbolic_trace(M())
        comparison_fn = symbolic_trace(comparison)

        x = torch.rand(1, 3)

        # Replace `pattern` with the same pattern (shouldn't change
        # the underlying logic)
        subgraph_rewriter.replace_pattern(traced_module, pattern, pattern)

        traced_module.graph.lint(traced_module)

        ref_output = comparison_fn(x)
        test_output = traced_module.forward(x)
        self.assertEqual(ref_output, test_output)

    def test_subgraph_rewriter_with_oneliner_pattern(self):
        class M(torch.nn.Module):
            def forward(self, x):
                val = torch.neg(x)
                return torch.add(val, val)

        def pattern(x):
            return torch.neg(x)

        def replacement(x):
            return torch.relu(x)

        def comparison(x):
            val = torch.relu(x)
            return torch.add(val, val)

        traced_module = symbolic_trace(M())
        comparison_fn = symbolic_trace(comparison)

        x = torch.rand(1, 3)

        subgraph_rewriter.replace_pattern(traced_module, pattern, replacement)

        traced_module.graph.lint(traced_module)

        ref_output = comparison_fn(x)
        test_output = traced_module.forward(x)
        self.assertEqual(ref_output, test_output)

    def test_subgraph_rewriter_single_pattern_match(self):
        class M(torch.nn.Module):
            def forward(self, x):
                val = torch.neg(x) + torch.relu(x)
                return torch.add(val, val)

        def pattern(x):
            return torch.neg(x) + torch.relu(x)

        def replacement(x):
            return torch.relu(x)

        def comparison(x):
            val = torch.relu(x)
            return torch.add(val, val)

        traced_module = symbolic_trace(M())
        comparison_fn = symbolic_trace(comparison)

        x = torch.rand(1, 3)

        subgraph_rewriter.replace_pattern(traced_module, pattern, replacement)

        traced_module.graph.lint(traced_module)

        ref_output = comparison_fn(x)
        test_output = traced_module.forward(x)
        self.assertEqual(ref_output, test_output)

    def test_subgraph_rewriter_multiple_pattern_match(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()

            def forward(self, x, w1, w2):
                m1 = torch.cat([w1, w2]).sum()
                m2 = torch.cat([w1, w2]).sum()
                return x + torch.max(m1) + torch.max(m2)

        def pattern(w1, w2):
            return torch.cat([w1, w2]).sum()

        def replacement(w1, w2):
            return torch.stack([w1, w2])

        def comparison(x, w1, w2):
            m1 = torch.stack([w1, w2])
            m2 = torch.stack([w1, w2])
            return x + torch.max(m1) + torch.max(m2)

        traced_module = symbolic_trace(M())
        comparison_fn = symbolic_trace(comparison)

        x = torch.rand(1, 3)
        w1 = torch.rand(1, 3)
        w2 = torch.rand(1, 3)

        subgraph_rewriter.replace_pattern(traced_module, pattern, replacement)

        traced_module.graph.lint(traced_module)

        ref_outs = comparison_fn(x, w1, w2)
        test_outs = traced_module.forward(x, w1, w2)
        self.assertEqual(ref_outs, test_outs)

    def test_subgraph_rewriter_graph_argument_order(self):
        class M(torch.nn.Module):
            def __init__(self):
                super().__init__()

            def forward(self, x, y):
                return torch.mm(x, y)

        def pattern(x, y):
            return torch.mm(x, y)

        def comparison(x, y):
            return torch.mm(x, y)

        traced_module = symbolic_trace(M())
        comparison_fn = symbolic_trace(comparison)

        x = torch.randn(3, 4)
        y = torch.randn(4, 5)

        subgraph_rewriter.replace_pattern(traced_module, pattern, pattern)

        traced_module.graph.lint(traced_module)

        ref_outs = comparison_fn(x, y)
        test_outs = traced_module.forward(x, y)
        self.assertEqual(ref_outs, test_outs)
