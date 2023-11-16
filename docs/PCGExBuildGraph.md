# Build Graph ![Spatial](https://img.shields.io/badge/Spatial-955195)

## What is it useful for?
This node processes points and cache their neighbors based on specific [Graph Params](PCGExGraphParams.md), in order to be easily processed by other relational nodes without having to recompute graphs each time.

> Important Disclaimer: Graph are cached based on point index; meaning **if you change the order or number of point in a dataset, you will need to "Find Graph" again** to ensure they are up to date, otherwise you may end up with unexpected results.

## How to use
### Inputs