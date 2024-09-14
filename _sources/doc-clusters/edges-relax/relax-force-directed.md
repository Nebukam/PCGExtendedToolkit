---
layout: page
grand_parent: Clusters
parent: Relax
title: ☍ Force Directed
subtitle: Force-directed relaxation
summary: The **Force-directed** relaxation ...
color: white
splash: icons/icon_edges-relax.svg
see_also:
    - Relax
tagged: 
    - relax
nav_order: 2
---

{% include header_card_toc %}

Force-directed relaxation simulates physical forces on connected nodes, adjusting their positions iteratively to minimize energy, balancing attractive and repulsive forces to reveal an optimal topology. *See [Force-directed graph drawing on Wikipedia](https://en.wikipedia.org/wiki/Force-directed_graph_drawing)*
{: .fs-5 .fw-400 } 

{% include img a='placeholder-wide.jpg' %}

# Properties

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Spring Constant           | Controls the "springness" of the edges.<br>*Keep this value very small* |
| Electrostatic          | Controls the "attractiveness" of the vtx.<br>*Keep this value very large* |

> The default settings are not really meant to be changed -- editing them quickly leads to artifacts and anomalies.
{: .warning }
