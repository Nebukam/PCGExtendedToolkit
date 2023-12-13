# General Informations

## Points Inputs

PCGEx process points inputs individually internally, and will **never merge** points set together. If you want points to be processed as a single dataset, use the built-in **Merge** node before.

> The only node that process inputs as a whole is [Partition by Values](PCGExPartitionByValues.md) as it would otherwise create duplicate partitions, which defeats the purpose of the node.

## Performance

PCGEx Nodes are designed for performance, and generally **rely heavily on multi-threading & async work** to avoid freezing the editor during heavy processing.  
There is one major caveat to parallelization however, which is a *less deterministic* order of operation -- most node are index-bound by design, which the expection of a few which will yield slitgtly different outputs every time they are executed *(there is a disclaimer on the individual documentation pages for these nodes).*

Each PCGEx node comes with a "Performance" settings category, allowing you to tweak:

#### Do Async Processing
Checked by default, you can toggle it off to force synchronous execution of the code.
> Note that synchronous execution still processes data in *chunks*, it just does it with guaranteed order, as opposed to parallel processing.

#### Chunk Size
The chunk size usually represents the number of point a node will process in a single parallel batch. There is no ideal value: too small and you loose the gain of parallelization, too high and you're just hogging thread ressources. **Ultimately, it depends on your specific setup.**

> Unreal PCG plugin recommend a minimum batch size of 256, which is the default value I'm using for most of the nodes. Heavier operations go as low as 32.

#### Cache Result
Under the hood, all PCG node come with the ability to cache their result; but the system is designed so it's a compile-time choice, not an editor-time one. I exposed the ability to cache on-demand at the price of some harmless asserts, because once you're done iterating on certain settings, it's worth caching the results.
> Be aware that the cache is easily corrupted, and sometime leads to missing points or data; *it's still a small price to pay when you're working iteratively with hundreds of thousands points.*

## Soft-typed-*ish* Attribute Getters

PCG Attributes are, by nature, strongly typed. PCGEx nodes that read attributes have a slightly different approach, and will instead attempt to read the data needed for computation, no matter what the incoming type is.  
This translate as attribute picker often displaying extra options, such as *Field*, or *Axis*:

#### Field
Field is used when the data is expected to be a Scalar or Integer. If the actual data is a multi-component one, this dropdown lets you select which field you're interested in reading. If the field does not exist, it will fallback to the previous available one, in order.

#### Axis
Axis is used sometimes alone, when a direction or normal is expected; or along with **Field**.  
This helps further narrowing down which sub-sub component you're willing to read data from. For example, if the PCG element expects a *double* but the input data used is an *FQuat*, the selected Axis will be used to do a first breakdown of the data to pick a field from.

> Note that the case of FTransform is trickier, and which component is used is context-dependent. Sometimes the FTransform.Rotator will be used, sometimes FTransform.Location is preferred.

