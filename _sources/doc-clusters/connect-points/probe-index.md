---
layout: page
grand_parent: Clusters
parent: Connect Points
title: 🝆 Index
subtitle: Connects to a point at a given index
summary: The **Index** probe ...
color: white
splash: icons/icon_misc-write-index.svg
preview_img: placeholder.jpg
toc_img: placeholder.jpg
tagged: 
    - node
    - probe
    - clusters
nav_order: 7
inputs:
    -   name : None
        desc : 
        pin : 
outputs:
    -   name : Probe
        desc : A single probe, to be used with the 'Connect Points' node
        pin : params
---

{% include header_card_node %}

This probes creates a single connections to another point at a given index.
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
| Mode          | How the node is to interpret the target index value. |
| Index Safety           | *Index safety is a recurring mechanism in PCGEx, see below for more infos.* |
|: **Target Index Settings** :|
| Target Index           | The type of value used for this probe' index source; either a `Constant` value or fetched from an`Attribute` |
| Target Constant           | Constant index value; or constant offset depending on the selected `Mode`. |
| Target Attribute           | Per-point attribute value for the index/offset. |

## Mode

|: Favor     ||
|:-------------|:------------------|
| Target           | Use the target index value as a raw index to connect to. |
| One-way Offset         | Use the target index value as an offset to the currently probed point' index. |
| Two-way Offset         | Use the target index value as an offset to the currently probed point' index; and creates a second connection but this time using the **target index value multiplied by -1.** |

{% include embed id='settings-index-safety' %}