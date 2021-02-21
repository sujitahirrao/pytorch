import operator

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.nn.quantized as nnq
import torch.nn.qat as nnqat
import torch.nn.intrinsic.qat as nniqat
toq = torch.ops.quantized

from torch.fx import GraphModule
from torch.fx.graph import Graph, Node

from .utils import getattr_from_fqn

from typing import Dict, Tuple, List, Optional, Set, Callable

def _get_output_nodes(g: Graph) -> List[Node]:
    return [n for n in g.nodes if n.op == 'output']

def get_type_a_related_to_b() -> Set[Tuple[Callable, Callable]]:
    # TODO(future PR): allow customizations
    # TODO(future PR): reuse existing quantization mappings
    # TODO(future PR): add the rest of modules and ops here
    sets_of_related_ops: List[Set[Callable]] = [
        # conv modules
        set([
            nn.Conv2d,
            nnq.Conv2d,
            nnqat.Conv2d,
            # Note: matching weights may not work with nniqat.ConvBn2d directly
            # leaving that as a problem for a future PR to solve.
            nniqat.ConvBn2d,
        ]),
        # linear modules
        set([
            nn.Linear,
            nnq.Linear,
            nnqat.Linear,
        ]),
        # linear functionals
        set([
            F.linear,
            toq.linear,
            toq.linear_relu,
        ]),
        # add
        set([
            torch.add,
            toq.add,
            operator.add,  # x + y
        ]),
        # cat
        set([
            torch.cat,
            toq.cat,
        ]),
    ]

    type_a_related_to_b: Set[Tuple[Callable, Callable]] = set()

    for s in sets_of_related_ops:
        s_list = list(s)
        # add every bidirectional pair
        for idx_0 in range(0, len(s_list) - 1):
            for idx_1 in range(idx_0 + 1, len(s_list)):
                type_a_related_to_b.add((s_list[idx_0], s_list[idx_1]))
                type_a_related_to_b.add((s_list[idx_1], s_list[idx_0]))

    return type_a_related_to_b

def get_non_matchable_functions() -> Set[Callable]:
    """
    `call_function` nodes pointing to these functions are non-matchable.
    """
    # TODO(future PR): allow customizations
    return set([
        torch.quantize_per_tensor,
    ])

def get_non_matchable_modules() -> Set[Callable]:
    """
    `call_module` nodes pointing to instances of these types are non-matchable.
    """
    # TODO(future PR): allow customizations
    return set([
        torch.quantization.ObserverBase,
        torch.quantization.FakeQuantizeBase,
    ])

def get_reversed_fusions() -> Set[Tuple[Callable, Callable]]:
    """
    Set of potential fusions, in reverse order.  The order is reversed
    to match how fusion patterns are defined in quantization code.
    """
    return set([
        (F.relu, F.linear),
    ])

# TODO(future PR): we should see if we can reuse quantization's fusion
# patterns here.
def end_node_matches_reversed_fusion(
    end_node: Node,
    reversed_fusion: Tuple[Callable, Callable],
) -> bool:
    """
    Returns true if a pattern ending with `end_node` matches
    the fusion pattern.
    """
    if end_node.op == 'call_function':
        cur_node = end_node
        for fusion_idx in range(len(reversed_fusion)):
            cur_fusion_op = reversed_fusion[fusion_idx]
            if cur_node.target != cur_fusion_op:
                return False
            if len(cur_node.args) > 0 and isinstance(cur_node.args[0], Node):
                cur_node = cur_node.args[0]
            else:
                return False
        return True
    # TODO(future PR): handle call_module
    return False


class _NSGraphMatchableSubgraphsIterator:
    """
    Iterates through the graph of gm, starting with the output nodes
    and continuing backwards.
    1. Returns matchable subgraphs, in order. A subgraph is defined by
       (start_node, end_node).
    2. Skips over non-matchable subgraphs
    """
    def __init__(
        self,
        gm: GraphModule,
        non_matchable_functions: Set[Callable],
        non_matchable_modules: Set[Callable],
    ):
        self.gm: GraphModule = gm
        self.non_matchable_functions: Set[Callable] = non_matchable_functions
        self.non_matchable_modules: Set[Callable] = non_matchable_modules
        self.seen_nodes: Set[Node] = set()
        self.stack: List[Node] = []
        for start_node in _get_output_nodes(self.gm.graph):
            self.stack.append(start_node)

    def __iter__(self):
        return self

    def __next__(self) -> Tuple[Node, Node]:
        """
        Returns the next matchable subgraph, defined by (start_node, end_node)
        """
        while len(self.stack) > 0:
            cur_end_node = self.stack.pop()
            if cur_end_node in self.seen_nodes:
                continue

            # for subgraphs which are single nodes, start_node == end_node
            # for subgraphs with more than one node, start node != end_node
            cur_start_node = cur_end_node

            # Check for potential fusions. For now, we are greedy
            # and always skip all non-base nodes of a fusion.  For example,
            # if we match linear-relu backwards, we will always skip the
            # relu node and attempt to match the linear node.  This can
            # be made configurable later if needed.
            for _reverse_fusion_ops in get_reversed_fusions():
                is_match = end_node_matches_reversed_fusion(
                    cur_end_node, _reverse_fusion_ops)
                if is_match:
                    # navigate to the base node
                    for fusion_idx in range(len(_reverse_fusion_ops) - 1):
                        self.seen_nodes.add(cur_start_node)
                        # for now, assume that there are no other nodes
                        # which need to be added to the stack
                        cur_start_node = cur_start_node.args[0]  # type: ignore
                    break

            self.seen_nodes.add(cur_start_node)
            # add args of previous nodes to stack
            # TODO(future PR): handle kwargs as needed
            for arg in cur_start_node.args:
                if isinstance(arg, Node):
                    self.stack.append(arg)
                # TODO(future PR): handle other arg types such as Tuple, etc

            # skip observers, etc
            # note: this check is done on the start_node, i.e.
            # if we are matching linear-relu in reverse, this would do the matchable
            # check on the linear
            if not self._is_matchable(cur_start_node):
                continue

            return cur_start_node, cur_end_node

        raise StopIteration

    def _is_matchable(self, node: Node) -> bool:
        if node.op == 'call_function':
            return not (node.target in self.non_matchable_functions)
        elif node.op == 'call_module':
            assert isinstance(node.target, str)
            # target_mod = getattr(self.gm, node.target)
            target_mod = getattr_from_fqn(self.gm, node.target)
            return not \
                any(isinstance(target_mod, t)  # type: ignore
                    for t in self.non_matchable_modules)
        else:
            return False

class GraphMatchingException(Exception):
    """
    Exception raised when two graphs cannot be matched.
    """
    pass

def _node_a_related_to_b(
    node_a: Node,
    node_b: Node,
    gm_a: GraphModule,
    gm_b: GraphModule,
    type_a_related_to_b: Set[Tuple[Callable, Callable]],
) -> bool:
    if node_a.op != node_b.op:
        # for now, comparing call_module to call_function is not supported
        # this can be added later if needed
        return False

    if node_a.op == 'call_function':
        if node_a.target == node_b.target:
            # nodes with equivalent targets always match (i.e. F.linear and F.linear)
            return True
        key = (node_a.target, node_b.target)
        return key in type_a_related_to_b
    elif node_a.op == 'call_module':
        # for call_module, we need to look up the modules to do the type check
        assert isinstance(node_a.target, str)
        mod_a = getattr_from_fqn(gm_a, node_a.target)
        assert isinstance(node_b.target, str)
        mod_b = getattr_from_fqn(gm_b, node_b.target)
        # modules with equivalent types always match (i.e. nn.Conv2d and nn.Conv2d)
        if type(mod_a) == type(mod_b):
            return True
        key = (type(mod_a), type(mod_b))
        return key in type_a_related_to_b
    return False

def _get_name_for_subgraph_pair(
    start_node_a: Node,
    end_node_a: Node,
    start_node_b: Node,
    end_node_b: Node,
) -> str:
    if end_node_b.op == 'call_module':
        assert isinstance(end_node_b.target, str)
        return end_node_b.target
    # for now, use node name.
    # TODO(future PR): find a better solution
    return end_node_b.name

def _get_node_target_type(node: Node, gm: GraphModule) -> Optional[Callable]:
    if node.op == 'call_function':
        return node.target  # type: ignore
    elif node.op == 'call_module':
        assert isinstance(node.target, str)
        mod = getattr_from_fqn(gm, node.target)
        return type(mod)
    return None

def get_matching_subgraph_pairs(
    gm_a: GraphModule,
    gm_b: GraphModule,
) -> Dict[str, Tuple[Tuple[Node, Node], Tuple[Node, Node]]]:
    """
    Matches matchable subgraphs of graph_a to graph_b.

    For a node, "matchable" is defined as a node which is not an observer,
    fake_quants, quant or dequant.

    A subgraph can contain one or more nodes.  A subgraph is matchable if
    at least one node inside of it is matchable.  Currently, all nodes in
    a subgraph must be matchable (because we assume no observers will be
    inserted in the middle of a fusion).

    A subgraph is defined by (start_node, end_node).  We assume that only
    start_node and end_node are linked with the surrounding graph, all other
    nodes in a subgraph are self-contained.

    A pair of nodes is "related" if both nodes represent the same mathematical
    operation across different quantization flavors. For example,
    `F.linear` and `torch.ops.quantized.linear` are related, and
    `F.linear` and `torch.nn.Conv` are not related.

    For each matchable pair of nodes node_a and node_b, they will match
    if node_a and node_b are related.

    For graphs A and B, they will match iff:
    1. the number of matchable subgraphs in A and B is equivalent
    2. when iterating through the matchable subgraphs of A and B in the same order, each
       corresponding pair of base nodes is related.

    This enables us to find the corresponding subgraphs between
    graphs of related models.  For example, if we had two graphs such as:

    graph_a: x0 -> conv_0 (type: nn.Conv2d) -> obs_0 -> x1
             w  -/
             b  -/

    graph_b: x0 -> quant_0 -> qconv_0 (type: nnq.Conv2d) -> dequant_0 -> x1
           packed_params_0 -/

    This function will return the following result:
    {
        'conv_0': (  # the name of the node in graph_b
          (conv_0, conv_0),  # (start_node_a, end_node_a)
          (qconv_0, qconv_0),  # (start_node_b, end_node_b)
        ),
    }

    Or, if we have a fusion pattern,

    graph_a: x0 -> linear_0 -> relu_0 -> obs_0 -> x1
             w  -/
             b  -/

    graph_b: x0 -> quant_0 -> linear_relu_0 -> dequant_0 -> x1
           packed_params_0 -/

    This function will return the following result:
    {
        'linear_relu_0': (  # the name of the node in graph_b
          (linear_0, relu_0),  # (start_node_a, end_node_a)
          (linear_relu_0, linear_relu_0),  # (start_node_b, end_node_b)
        ),
    }
    """
    non_matchable_functions = get_non_matchable_functions()
    non_matchable_modules = get_non_matchable_modules()
    graph_a_iterator = _NSGraphMatchableSubgraphsIterator(
        gm_a, non_matchable_functions, non_matchable_modules)
    graph_b_iterator = _NSGraphMatchableSubgraphsIterator(
        gm_b, non_matchable_functions, non_matchable_modules)
    results = {}
    type_a_related_to_b = get_type_a_related_to_b()

    while True:
        # fetch the next nodes from a and b
        cur_start_node_a, cur_start_node_b = None, None
        cur_end_node_a, cur_end_node_b = None, None
        try:
            cur_start_node_a, cur_end_node_a = next(graph_a_iterator)
        except StopIteration:
            pass
        try:
            cur_start_node_b, cur_end_node_b = next(graph_b_iterator)
        except StopIteration:
            pass

        # look up types of a and b for useful error messages
        type_a, type_b = None, None
        if cur_end_node_a is not None:
            type_a = _get_node_target_type(cur_end_node_a, gm_a)
        if cur_end_node_b is not None:
            type_b = _get_node_target_type(cur_end_node_b, gm_b)

        # check for results and determine what to do next
        if cur_end_node_a is not None and cur_end_node_b is not None:
            assert isinstance(cur_start_node_a, Node)
            assert isinstance(cur_start_node_b, Node)
            # both nodes were fetched, check for relatedness
            # note: relatedness is checked on the start node, i.e.
            # if a linear-relu pattern is checked, we would check for relatedness
            # of the linear
            if not _node_a_related_to_b(cur_start_node_a, cur_start_node_b,
                                        gm_a, gm_b, type_a_related_to_b):
                msg = f"({cur_start_node_a}, {type_a}) and ({cur_start_node_b}, {type_b}) are not related"
                raise GraphMatchingException(msg)
            key_name = _get_name_for_subgraph_pair(
                cur_start_node_a, cur_end_node_a, cur_start_node_b, cur_end_node_b)
            results[key_name] = (
                (cur_start_node_a, cur_end_node_a),
                (cur_start_node_b, cur_end_node_b),
            )
            continue
        elif cur_end_node_a is None and cur_end_node_b is None:
            # we reached the end of both graphs
            break
        else:
            # only one node was fetched, no match possible, throw error
            msg = f"Matchable nodes count mismatch: ({cur_end_node_a}, {type_a}) and ({cur_end_node_b}, {type_b})"
            raise GraphMatchingException(msg)

    return results
