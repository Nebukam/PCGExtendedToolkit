---
layout: page
family: Sampler
grand_parent: Sampling
parent: Textures
title: Sample Texture
name_in_editor: "Sample : Texture"
subtitle: Sample multiple, per-point textures
color: white
summary: The **Sample Texture** can sample multiple texture sources at the same time, using per-point selection
splash: icons/icon_OUT_Tex.svg
tagged: 
    - node
    - sampling
nav_order: 20
inputs:
    -   name : In
        desc : Points with asset reference attribute
        pin : points
    -   name : Point Filters
        extra_icon : OUT_Filter 
        desc : Filter which points will be read
        pin : params
    -   name : Texture Data
        desc : PCG texture data
        pin : textures
    -   name : Texture Params
        extra_icon : OUT_Tex 
        desc : Reusable texture parameter definition
        pin : params
outputs:
    -   name : Out
        desc : Points with added data
        pin : points
---

{% include header_card_node %}

The **Sample Texture** node offers a very efficient way to sample multiple texture sources at the same time without the need for loops.
{: .fs-5 .fw-400 }

{% include img a='details/textures/sample-texture-data.png' %}

# Properties
<br>

WIP / TBD