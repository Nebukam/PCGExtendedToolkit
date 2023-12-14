# Principles

## Points Inputs

PCGEx process points inputs individually internally, and will **never merge** points set together. If you want points to be processed as a single dataset, use the built-in **Merge** node before.

> The only node that process inputs as a whole is [Partition by Values](PCGExPartitionByValues.md) as it would otherwise create duplicate partitions, which defeats the purpose of the node.

## Soft-typed-*ish* Attributes

PCG Attributes are, by nature, strongly typed. PCGEx nodes that read attributes have a slightly different approach, and will instead attempt to read the data needed for computation, no matter what the incoming type is.  
This translate as attribute picker often displaying extra options, such as *Field*, or *Axis*:

#### Field
Field is used when the data is expected to be a Scalar or Integer. If the actual data is a multi-component one, this dropdown lets you select which field you're interested in reading. If the field does not exist, it will fallback to the previous available one, in order.

#### Axis
Axis is used sometimes alone, when a direction or normal is expected; or along with **Field**.  
This helps further narrowing down which sub-sub component you're willing to read data from. For example, if the PCG element expects a *double* but the input data used is an *FQuat*, the selected Axis will be used to do a first breakdown of the data to pick a field from.

> Note that the case of FTransform is trickier, and which component is used is context-dependent. Sometimes the FTransform.Rotator will be used, sometimes FTransform.Location is preferred.

