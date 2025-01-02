---
layout: page
family: Sampler
grand_parent: Sampling
parent: Textures
title: Get Texture Data
name_in_editor: "Get Texture Data"
subtitle: Can output multiple textures object from material parameters and texture paths
color: white
summary: The **Get Texture Data** node can create PCG Textures in batches, either from texture reference paths, or by parsing material' texture parameters.
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
    -   name : Texture Params
        extra_icon : OUT_Tex 
        desc : Reusable texture parameter definition
        pin : params
outputs:
    -   name : Out
        desc : Points with added data
        pin : points
    -   name : Texture Data
        desc : PCG texture data
        pin : textures
---

{% include header_card_node %}

The **Get Texture Data** provide a quick-and-dirty (yet highly customizable) way to create new PCG textures data from either material parameters or texture paths.
{: .fs-5 .fw-400 }

{% include img a='details/textures/get-texture-data.png' %}

# Properties
<br>

WIP / TBD