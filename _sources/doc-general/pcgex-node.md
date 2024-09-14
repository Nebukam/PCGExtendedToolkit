---
layout: page
parent: ∷ General
title: PCGEx Nodes
subtitle: Shared concepts
summary: A summary of the informations & parameters available on every PCGEx node.
splash: icons/icon_component.svg
nav_order: 1
tagged:
    - basics
#has_children: true
---

{% include header_card %}

Almost every node in the PCGEx inherit from the same point processor, and as such they have some shared capabilities.
{: .fs-5 .fw-400 } 

PCGEx has a focus on performance and multithreading -- very few nodes are actively computing anything on the main thread, and instead the bulk of the tasks is handled asynchronously; and parallelized whenever possible.  
This helps keeping the editor *relatively* smooth when performing heavy tasks.
{: .fs-5 .fw-400 } 

#### Common Tweaks
{: .no_toc }
- TOC
{:toc}

---
## Performance

{% include img a='docs/pcgex-performance.png' %} 

> Side note that most PCGEx nodes are greedy, and contrary to vanilla nodes, **they process all inputs in parallel instead of one after another**. Most of the time it's a non-issue, but may be noticeable if you have a lot of very large datasets plugged into the same node.

### Do Async Processing
Checked by default, you can toggle it off to force synchronous/unparallelized execution of the code.  
*This is a very legacy option, best leave it to `true`.*

### Chunk Size
The chunk size usually represents the number of point a node will process in a single parallel batch. There is no ideal value: too small and you loose the gain of parallelization, too high and you're just hogging thread ressources. **Ultimately, it depends on your specific setup.**  
A value of `-1` fallbacks to that specific' node default value under the hood.

> Unreal PCG plugin recommend a minimum batch size of 256, which is the default value I'm using for most of the nodes. Heavier operations can go as low as 32.

### Cache Result
Under the hood, all PCG node come with the ability to cache their result; but the system is designed so it's a compile-time choice, not an editor-time one. I exposed the ability to cache on-demand at the price of some harmless asserts, because once you're done iterating on certain settings, it's worth caching the results.

> Be aware that the cache is easily corrupted, and sometime leads to missing points or data; *it's still a small price to pay when you're working iteratively with hundreds of thousands points.*
{: .warning }

### Flatten Output
Flatten the output of this node. On `5.3` this is a very expensive operation, it's better in `5.4` and should be even faster in `5.5`.  Flattening ensure all inherited attribute values are copied to the output, and metadata parenting/inheritance is forfeited in the process. **This is a required step to ensure attribute values are properly saved to PCG Data Assets!**

---
## Input Pruning & De-duping

### De-duplication
Datasets are de-duplicated by design in PCGEx -- this means that if you plug the exact same data (as in *same memory pointer*) it will be only processed once -- even if it has different tags.

### Pruning
Datasets that are empty and contains no points will be ignored, **and won't be forwarded to the ouput pins**.
