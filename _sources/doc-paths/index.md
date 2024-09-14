---
layout: page
title: Paths
subtitle: Smooth, orient, tangents, ...
summary: Manipulate points as linearily ordered datasets
splash: icons/icon_cat-clusters.svg
nav_order: 31
has_children: true
preview_img: previews/index-paths.png
tagged:
    - category
---

{% include header_card %}

This section contains path-like data utilities.  

### Paths are just points
Like every other PCGEx thing, **Paths** are really just Points Dataset -- however, **Path nodes assume that the input data represent a "path", a.k.a an ordered list of point**. Each point represent a "step" inside that path, from the first to last point in the Dataset.  

> Paths nodes accept any inputs, **and do not rely on any custom attributes to work.**.

All path nodes have a `Closed Path` checkbox to indicate whether it should process its input as closed or open paths. 

> At the time of writing, there is no way to handler per-input closed/open state. **Either all inputs are considered closed, or they are not.**. 
> This will change in the future, and will likely rely on user-defined tags to know whether a path is to be considered closed or not.
{: .warning }



---
## Paths Nodes
<br>
{% include card_childs tagged='node' %}