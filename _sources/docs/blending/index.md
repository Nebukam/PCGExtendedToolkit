---
layout: page
parent: Documentation
title: Blending
subtitle: TBD
color: white
subtitle: An inventory of the available blending modules.
splash: icons/icon_sampling-point.svg
preview_img: docs/splash-fuse-points.png
nav_order: 10
has_children: true
tagged: 
    - module
---

{% include header_card %}

Blending modules are primarily used by nodes like {% include lk id='Subdivide' %}, as well as under the hood of many nodes where only a sub-selection of parameters will be exposed *(as opposed to full module selection)*
{: .fs-5 .fw-400 }

> Blending operate on *sub-points* groups, not entire groups -- what "sub-points" means is up to the node using the blending.
> **Hence, "First" and "Last" refers to the first & last of the sub-points.**
{: .infos-hl }

#### Checklist
{: .no_toc }
- TOC
{:toc}

## Modules
<br>
{% include card_childs tagged="blending" %}

---
## Common Properties
blending-common-properties

{% include img a='docs/blending-common-properties.png' %} 

| Selector       | Data          |
|:-------------|:------------------|
| Default Blending           | Pick the default blending mode for both point properties and attributes |
| Properties Overrides           | Individual overrides for point properties |
| Attributes Overrides           | Individual overrides for attributes |

>Disregard which blendmode appears when greyed out, what is selected as `Default Blending` will be used for those.
{: .comment }

### Blendmodes

| Selector       | Data          |
|:-------------|:------------------|
| None           | Keep the first processed value |
| Weight           | Weights all the processed values. **How that weight of each value is calculated depends on the node, module, and data set.**<br> *More often than not, it comes down to a basic lerp.* |
| Average           | Average all the processed values |
| Min           | Keep the minimum value (component-wise) encountered during processing |
| Max           | Keep the maximum value (component-wise) encountered during processing |
| Copy           | Overwrites the value with the last processed one |

---

{% include img a='details/modules/blendings.png' %} 