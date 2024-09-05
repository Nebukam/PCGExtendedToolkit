---
layout: page
#grand_parent: All Nodes
parent: Misc
title: Points to Bounds
subtitle: Convert point clouds to a single bounding point
summary: The **Points to Bounds** node ...
color: white
splash: icons/icon_misc-points-to-bounds.svg
preview_img: docs/splash-points-to-bounds.png
toc_img: placeholder.jpg
tagged:
    - node
    - misc
nav_order: 5
---

{% include header_card_node %}

# Properties
<br>

**Points to bound** has no dedicated properties and is pretty straighforward to use.
It embeds a data blender module, which you can read more about in the specific {% include lk id='Blending' %} section.

---
# Inputs
## In
Any number of point datasets.

---
# Outputs
## Out
As many outputs as there are inputs. Each output contains a single point representing the bounds of the corresponding input dataset.  
*Reminder that empty inputs will be ignored & pruned*.