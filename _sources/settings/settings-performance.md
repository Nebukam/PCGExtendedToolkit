---
title: settings-weighting
has_children: false
nav_exclude: true
---

---
## Performance Settings

|**Performance Settings**||
|Do Async Processing| Checked by default, you can toggle it off to force synchronous/unparallelized execution of the code. *It's usually a bad idea performance-wise.*<br> > Note that synchronous execution still processes data in *chunks*, it just does it with guaranteed order, as opposed to parallel processing. |
|Chunk Size| The chunk size represents the number of point/operations a node will process in a single parallel batch. There is no ideal value: too small and you loose the gain of parallelization, too high and you're just hogging thread ressources. **Ultimately, it depends on your specific setup.**<br>A value of `-1` fallbacks to that specific' node default value under the hood, which is usually a generic sweet spot; ranging from 64 to 4096 depending on the node.|
|Cache Result| Under the hood, all PCG node come with the ability to cache their result; but the system is designed so it's a compile-time choice, not an editor-time one **(at least in 5.3)**. I exposed the ability to cache on-demand at the price of some harmless asserts, because once you're done iterating on certain settings, it's worth caching the results.|
|Flatten Output| When enabled, all of the node output attributes will be flattened; primarily meaning that their metadata (attributes) are not inheriting parents, and the node can be safely saved to a data asset.<br> *It's a rather costly operation, so make sure you need it, and for the sake of your RAM, don't enable it otherwise.* |