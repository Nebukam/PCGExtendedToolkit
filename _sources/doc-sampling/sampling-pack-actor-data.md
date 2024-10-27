---
layout: page
family: Sampler
#grand_parent: All Nodes
parent: Sampling
title: Pack Actor Data
name_in_editor: "Pack Actor Data"
subtitle: Conveniently process actors and output data using custom blueprints
color: white
summary: The **Pack Actor Data** node is a quick and dirty way to fetch custom data from actor references and write then to points.
splash: icons/icon_sampling-point.svg
tagged: 
    - node
    - sampling
nav_order: 100
inputs:
    -   name : In
        desc : Points with an actor reference
        pin : points
outputs:
    -   name : Out
        desc : Points with added data
        pin : points
---

{% include header_card_node %}

The **Pack Actor Data** provide a quick-and-dirty (yet highly customizable) way to extract per-actor data from a list of actor reference and write that data to these same points.
{: .fs-5 .fw-400 }

> If you're working with 5.5 and above, and just need a single property, have a look at the native `Get Property from Object Path` as it may fit your needs.
{: .comment }

{% include img a='details/sampling-pack-actor-data/lead.png' %}

# Properties
<br>

WIP / TBD