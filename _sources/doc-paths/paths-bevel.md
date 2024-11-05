---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Bevel
name_in_editor: "Path : Bevel"
subtitle: Bevel path points
color: white
summary: The **Fuse Collinear** node removes points that are collinear, with control over thresholds. It can also optionally fuse points based on their proximity.
splash: icons/icon_path-fuse.svg
tagged: 
    - node
    - paths
nav_order: 3
inputs:
    -   name : Paths
        desc : Paths which points will be bevelled
        pin : points
    -   name : Bevel Conditions
        desc : Optional filters used to determine whether a point will be bevelled or not
        pin : params
    -   name : Profile
        desc : Custom profile path
        pin : points
outputs:
    -   name : Paths
        desc : Bevelled paths
        pin : points
---

{% include header_card_node %}

# Properties
<br>

> DOC TDB
{: .warning }

| Mode       | |
|:-------------|:------------------|
| {% include img a='details/paths-bevel/enum-bvl-line.png' %} | **None**<br>Outputs canon Delaunay sites. |
| {% include img a='details/paths-bevel/enum-bvl-arc.png' %} | **Merge Sites**<br>The output site is the average of all the canon Delaunay sites that are abstracted by the Urquhart pass. |
| {% include img a='details/paths-bevel/enum-bvl-custom.png' %} | **Merge Edges**<br>The output site is the average of all the edges removed by the Urquhart pass. |
{: .enum }