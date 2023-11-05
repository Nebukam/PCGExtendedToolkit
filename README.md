# PCGExtendedToolkit

# What is it?
 The PCG Extended Toolkit is a plugin for [Unreal engine 5](https://www.unrealengine.com/en-US/) (5.3.1+) that contains a collection of low-level PCG Graph elements offering additional ways to manipulate and control PCG Data.

## PCGEx Nodes
Generally speaking, each node is meant to encapsulate very atomic behaviors in order to stay flexible and maintainable. This means there isn't a single "magic" node, but rather a toolkit of small-scoped elements that works together.

###  Relational
- [Find Relations](docs/PCGExFindRelations.md) | *Build relations between points based on [Relation Params](docs/PCGExRelationalParams.md)*
- [Find Paths](docs/PCGExFindPaths.md) | *Find paths between nodes using previously cached relations*

- *& Other lower-level nodes and params to support the above*

### Sorting
- [Sort Points](docs/PCGExSortPoints.md) | *Powerful multi-value sorting*

### Misc
- [Partition by Values](docs/PCGExPartitionByValues.md) | *A more controllable partitioning node*

## Getting Started
* [Installation](docs/Installation.md)
* [Examples](docs/Examples.md)


### See Also
* [Full Documentation Index](docs/Index.md)
* [Frequently Asked Questions](docs/FAQ.md)

### Footnotes
- Effort to build this toolkit was originally inspired by the [PCG Pathfinding](https://github.com/spood/PCGPathfinding) repo 
- Since the lack of documentation around PCG at the time of writing, my approach to build things may not be the right one, nor the most optimal.