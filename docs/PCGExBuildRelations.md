# Build Relations

## What is it useful for?
This node processes points and cache their neighbors based on specific [Relational Params](docs/PCGExRelationalParams.md), in order to be easily processed by other relational nodes without having to recompute relations each time.

> Important Disclaimer: Relations are cached based on point index; meaning **if you change the order or number of point in a dataset, you will need to "Find Relations" again** to ensure they are up to date, otherwise you may end up with unexpected results.

## How to use
### Inputs