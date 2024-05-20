---
layout: page
title: Misc
subtitle: All-around, generic utility nodes.
#summary: This section is dedicated to the Uber filter module and its inputs.
color: white
splash: icons/icon_cat-custom-graphs.svg
preview_img: placeholder.jpg
nav_order: 40
has_children: true
---

{% include header_card %}

> Contrary to mathematical graphs built with the {% include lk id='Graphs' %} nodes, **Custom Graphs** have no intrinsinc --nor guaranteed-- properties.
{: .infos-hl }

{% include blueprint id='akhveepm' %} 

## Overview
<br>
{% include img_link a='docs/customgraph/basic.png' %} 
<br>
The basic workflow is as follow:
1. Define custom graph params using {% include lk id='Custom Graph Params' %}. These params are a list of *sockets*, each of which defines a "probe" for a single connection.
2. Process points dataset using the previously defined params, and {% include lk id='Build Custom Graph' %}.
3. You now have an abstract graph -- it's super useful just yet
4. Push the custom graph a {% include lk id='Find Edge Clusters' %} node so you have `Vtx` and `Edges` to work with the rest of the toolkit!

## How Sockets Works



---
## Misc Nodes
<br>
{% include card_childs tagged='node' %}