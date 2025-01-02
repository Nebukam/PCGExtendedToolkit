---
layout: page
family: MiscWrite
#grand_parent: All Nodes
parent: Misc
title: Iterations
name_in_editor: "Iterations"
subtitle: Artificially drive loop subgraph iteration count
summary: The **Iterations** node outputs a number of collections that can be used to drive a Loop subgraph' iterations count, without the need for more shenaningans.
color: white
splash: icons/icon_misc-write-index.svg
tagged: 
    - node
    - misc
outputs:
    -   name : Iterations
        desc : A single lightweight, dummy data object
        pin : any
nav_order: 7
---

{% include header_card_node %}

Iterations is the `Loop Count` parameter that doesn't exist.
{: .fs-5 .fw-400 } 

{% include img a='details/misc/iterations.png' %}

# Properties
<br>

| Property       | Description          |
|:-------------|:------------------|
|:**Settings**||
| Type                  | Type of data collection generated.*This is only useful to avoid the need for an additional filter node if you already set up your loop pins a certain way.*  |
| Iterations           | Number of individual entries to output. |