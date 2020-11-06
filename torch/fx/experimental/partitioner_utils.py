from typing import NamedTuple, Dict, List
from torch.fx.node import Node, map_arg
from torch.fx.experimental.Partitioner import Partition

class NodeLatency(NamedTuple):
    # Latency due to the memory bandwidth
    mem_latency_sec: float
    # Latency due to the computation
    computer_latency_sec: float

class PartitionLatency(NamedTuple):
    # Sum of all nodes' memory latency on the critical path
    mem_latency_sec: float
    # Sum of all nodes' compute latency on the critical path
    computer_latency_sec: float
    # Latency of the critical path
    overall_latency_sec: float

def get_latency_of_one_partition(
    partition: Partition,
    node_to_latency_mapping: Dict[Node, NodeLatency]
) -> PartitionLatency:
    """Given a partiton and its nodes' latency, return a PartitionLatency for this partition"""

    def get_top_nodes(partition: Partition) -> List[Node]:
        """Given a partition, return a list of nodes on the top bfs level"""
        top_nodes: List[Node] = []
        for node in partition.nodes:
            # Skip placeholder and get_attr nodes
            if node.op in {'placeholder', 'get_attr'}:
                continue
            input_nodes: Dict[Node, None] = {}
            map_arg(node.args, lambda n: input_nodes.setdefault(n))
            map_arg(node.kwargs, lambda n: input_nodes.setdefault(n))
            # If a node has no input nodes in this partition,
            # or its input nodes in this partition are placeholders and get_attrs
            # this node is on the top bfs level in this partition
            if not any([n in partition.nodes and n.op not in {'placeholder', 'get_attr'} for n in input_nodes]):
                top_nodes.append(node)
        return top_nodes

    def dfs_helper(node: Node, partition_latency) -> PartitionLatency:
        """Given a top node of a partition, this function returns
           the latency of the critical path in the partition
        """
        node_latency = node_to_latency_mapping[node]
        # Calculate the current overall latency of the partition
        overall_latency_sec = partition_latency.overall_latency_sec + \
            max(node_latency.computer_latency_sec, node_latency.mem_latency_sec)
        # Update the mem latency of this path
        mem_latency_sec = partition_latency.mem_latency_sec + node_latency.mem_latency_sec
        # Update the compute latency of this path
        computer_latency_sec = partition_latency.computer_latency_sec + node_latency.computer_latency_sec
        # Get all users of this node that are in this partition
        users = set(node.users).intersection(partition.nodes)
        if users:
            max_latency = PartitionLatency(mem_latency_sec=0., computer_latency_sec=0., overall_latency_sec=0.)
            for n in users:
                # Get new partition latency recursively
                new_partition_latency = dfs_helper(n, PartitionLatency(mem_latency_sec, computer_latency_sec, overall_latency_sec))
                if new_partition_latency.overall_latency_sec > max_latency.overall_latency_sec:
                    max_latency = new_partition_latency
            return max_latency
        # If there is no user, the node is at bottom of the partition
        return PartitionLatency(mem_latency_sec, computer_latency_sec, overall_latency_sec)
    # Main part starts
    # Get all top level nodes of this partition
    top_nodes = get_top_nodes(partition)
    critical_path_latency = PartitionLatency(mem_latency_sec=0., computer_latency_sec=0., overall_latency_sec=0.)
    # Go through all top nodes and find the largest latency (critical pass latency)
    for node in top_nodes:
        partition_latency = dfs_helper(node, PartitionLatency(mem_latency_sec=0., computer_latency_sec=0., overall_latency_sec=0.))
        if partition_latency.overall_latency_sec > critical_path_latency.overall_latency_sec:
            critical_path_latency = partition_latency
    return critical_path_latency

def get_partition_to_latency_mapping(
    partitions: List[Partition],
    node_to_latency_mapping: Dict[Node, NodeLatency]
) -> Dict[Partition, PartitionLatency]:
    """Given all the partitions and node_to_latency_mapping dictionary,
       return a mapping dictionary of each partition to its overall latency
    """
    partition_to_latency_mapping: Dict[Partition, PartitionLatency] = {}
    # Go through each partition and get its latency
    for partition in partitions:
        partition_latency = get_latency_of_one_partition(partition, node_to_latency_mapping)
        partition_to_latency_mapping[partition] = partition_latency
    return partition_to_latency_mapping

def get_comm_latency_between(parent_partition: Partition, child_partition: Partition, transfer_rate_bytes_per_sec: float):
    """Given two partitions (parent and child),
       calculate the communication latency between the two.
    """
    # Keep tracking the communication size between parent and child
    comm_size = 0
    # Keep tracking all the counted node
    visited_nodes = set()
    # Go through all nodes in the child partition
    # If a node has input nodes from the parent partition,
    # the output size of those input nodes will be counted
    # and added to comm_size
    for node in child_partition.nodes:
        input_nodes: Dict[Node, None] = {}
        map_arg(node.args, lambda n: input_nodes.setdefault(n))
        map_arg(node.kwargs, lambda n: input_nodes.setdefault(n))
        for n in input_nodes:
            if n in parent_partition.nodes and n not in visited_nodes:
                size_bytes = getattr(n, "size_bytes", None)
                if size_bytes is not None:
                    comm_size += size_bytes.output_size
                visited_nodes.add(n)
    return comm_size * transfer_rate_bytes_per_sec

def get_latency_of_partitioned_graph(
    partitions: List[Partition],
    partition_to_latency_mapping: Dict[Partition, PartitionLatency],
    transfer_rate_bytes_per_sec: float
):
    """Given all paritions in a graph, find the critical path among all partitions
       and return its latency as the latency of the whole graph
    """
    def dfs_helper(partition: Partition, latency_so_far_sec: float) -> float:
        """This function helps to recursively get the latency of a path of partitions
        """
        # Update latency by adding current partition's latency
        latency_so_far_sec += partition_to_latency_mapping[partition].overall_latency_sec
        children = partition.children
        if partition.children:
            max_latency_sec = 0.
            for child in partition.children:
                # Calculate latency between
                comm_latency_sec = get_comm_latency_between(partition, child, transfer_rate_bytes_per_sec)
                new_latency_sec = dfs_helper(child, latency_so_far_sec + comm_latency_sec)
                if new_latency_sec > max_latency_sec:
                    max_latency_sec = new_latency_sec
            return max_latency_sec
        return latency_so_far_sec

    def get_top_partitions(partitions: List[Partition]) -> List[Partition]:
        """This function is to return all the partitions without parents
           as the starting points of all the paths
        """
        top_partitions = []
        for partition in partitions:
            # If a partition has no parents, then it is a top partition
            if len(partition.parents) == 0:
                top_partitions.append(partition)
        return top_partitions

    top_partitions = get_top_partitions(partitions)
    critical_path_latency_sec = 0.
    for partition in top_partitions:
        latency_sec = dfs_helper(partition, 0.)
        if latency_sec > critical_path_latency_sec:
            critical_path_latency_sec = latency_sec
    return critical_path_latency_sec
