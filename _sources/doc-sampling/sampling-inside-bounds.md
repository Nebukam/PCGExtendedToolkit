---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Sample Inside Bounds
name_in_editor: "Sample : Inside Bounds"
subtitle: Sample target points contained within bounds.
color: white
summary: The **Sample Nearest Bounds** node explore points within input bounds.
splash: icons/icon_sampling-point.svg
tagged: 
    - node
    - sampling
nav_order: 100
inputs:
    -   name : In
        desc : Points that will sample data from targets
        pin : points
    -   name : Point Filters
        extra_icon: OUT_Filter
        desc : Points filters used to determine which points will be processed. Filtered out points will be treated as failed sampling.
        pin : params
    -   name : Targets
        desc : Targets to read data from if they are within an input point bounds.
        pin : point
outputs:
    -   name : Out
        desc : In with extra attributes and properties
        pin : points
---

{% include header_card_node %}

> NODE TBD
{: .warning }