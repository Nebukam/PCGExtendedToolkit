# Write Index ![Misc](https://img.shields.io/badge/Misc-0a0a0a)

## What is it useful for?
This node writes point index to an attribute.  
It's not immediately useful, but can be leveraged to "cache" point order at a given point, do a bunch of operations and later one restore the original order by using [Sort Points](PCGExSortPoints.md) using the cached attribute.  
It can also be useful when paired with [Partition by Values](PCGExPartitionByValues.md) if you need a rather agnostic attribute to chunk content with.

## How to use
### Inputs