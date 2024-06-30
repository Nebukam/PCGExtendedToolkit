---
layout: page
#grand_parent: All Nodes
parent: Clusters
title: Draw Edges
subtitle: Draw debug edge lines of a given vtx/edge pair.
summary: The **Draw Edges** node ...
color: red
splash: icons/icon_edges-draw.svg
preview_img: docs/splash-edges-draw.png
toc_img: placeholder.jpg
tagged: 
    - node
    - edges
nav_order: 1000
inputs:
    -   name : Vtx
        desc : Endpoints of the input Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the input Vtxs
        pin : points
outputs:
    -   name : Vtx
        desc : Endpoints of the output Edges
        pin : points
    -   name : Edges
        desc : Edges associated with the output Vtxs
        pin : points
---

{% include header_card_node %}

{% include img a='details/details-draw-edges.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Color           | Primary debug color  |
| Secondary Color           | If enabled, each cluster will have a different color ranging from the primary color to the secondary color, based on the dataset index in the inputs. |
| Thickness           | Line thickness  |
| Depth Priority          | Debug draw depth priority. <br>- `-1` : draw on top of everything.<br>- `0` : Regular depth sorting.<br>- `1` : Draw behind everything. |

---
# Inputs & Outputs
> This nodes forwards its inputs as if it was disabled.  

## Vtx & Edges
See {% include lk id='Working with Clusters' %}