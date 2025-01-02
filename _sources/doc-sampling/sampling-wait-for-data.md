---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Wait for PCG Data
name_in_editor: "Wait for PCG Data"
subtitle: Wait for PCG Components data to be available and grabs it.
color: white
summary: The **Line Trace** node performs a single line trace for each point, using a local attribute or property as direction & size.
splash: icons/icon_sampling-guided.svg
#warning: This node works with collisions and as such can be very expensive on large datasets.
tagged: 
    - node
    - sampling
nav_order: 2
inputs:
    -   name : Targets
        desc : Points that contain a list of target actors that will be inspected for PCG Component
        pin : points
outputs:
    -   name : Roaming Data
        desc : Data that wasn't part of the template. Optional output.
        pin : any
---

{% include header_card_node %}

# Properties
<br>

TBD / WIP