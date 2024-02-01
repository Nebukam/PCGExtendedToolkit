---
layout: page
parent: Documentation
title: PCGEx Nodes
subtitle: Shared concepts
summary: A summary of the informations & parameters available on every PCGEx node.
splash: icons/icon_component.svg
preview_img: docs/splash-pcgex-nodes.png
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
{: .fs-5 .fw-300 } 

>Because of the parallelization, a *very few* select nodes have non-deterministic outputs; meaning they will yield slightly different results between two runs.  
>**These nodes are flagged in the doc.**  
>*This is something I'm working to fix to ensure all nodes have deterministic results.*
{: .error}

#### Common Tweaks
{: .no_toc }
- TOC
{:toc}

---
## Performance

{% include img a='docs/pcgex-performance.png' %} 

### Do Async Processing
Checked by default, you can toggle it off to force synchronous/unparallelized execution of the code. *(This is a cheap workaround to ensure deterministic result in nodes that are otherwise non-deterministic due to parallel processing)*

> Note that synchronous execution still processes data in *chunks*, it just does it with guaranteed order, as opposed to parallel processing.

### Chunk Size
The chunk size usually represents the number of point a node will process in a single parallel batch. There is no ideal value: too small and you loose the gain of parallelization, too high and you're just hogging thread ressources. **Ultimately, it depends on your specific setup.**  
A value of `-1` fallbacks to that specific' node default value under the hood.

> Unreal PCG plugin recommend a minimum batch size of 256, which is the default value I'm using for most of the nodes. Heavier operations can go as low as 32.

### Cache Result
Under the hood, all PCG node come with the ability to cache their result; but the system is designed so it's a compile-time choice, not an editor-time one. I exposed the ability to cache on-demand at the price of some harmless asserts, because once you're done iterating on certain settings, it's worth caching the results.

> Be aware that the cache is easily corrupted, and sometime leads to missing points or data; *it's still a small price to pay when you're working iteratively with hundreds of thousands points.*
{: .warning }

---
## Input Pruning & De-duping

### De-duplication
Datasets are de-duplicated by design in PCGEx -- this means that if you plug the exact same data (as in *same memory pointer*) it will be only processed once -- even if it has different tags.

### Pruning
Datasets that are empty and contains no points will be ignored, **and won't be forwarded to the ouput pins**.
