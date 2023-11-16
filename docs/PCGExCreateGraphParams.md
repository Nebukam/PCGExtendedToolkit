# Create Graph Params ![Graph](https://img.shields.io/badge/Graph-37a573)

## What is it useful for?
This node contains a single "relational model". It defines a list of relation *slots*, that each represent a direction in which a connection may exist. 

>It is mandatory to work with the **Graph** nodes. 

## How to use
Each slot represent a direction in space that can be world-based, or relative to the point it processes. By default, Graph Params contains 6 slots: *Up*, *Down*, *Left*, *Right*, *Forward* and *Backward*; meaning it samples relations like a 3D grid.

If you wanted relations to be sampled like a 2D grid, you could remove *Up* & *Down* entries; or create completely custom ones.
 
>Each slot is limited by a DotTolerance params, which effectively defines a mathematical cone in which points are considered for a relationship. **By design, this means relation definitions can overlap, resulting in the same points being referenced by multiple slot at once.**  
>
>*It's also worth noting that relation are unilateral: one point may not know another one is related to it.*
