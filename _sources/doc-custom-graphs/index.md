---
layout: page
title: ∷ Custom Graphs
subtitle: Modules & Documentation
summary: This section is dedicated to broader custom graph topics & modules. Node specifics can be found on their dedicated node page.
color: white
splash: icons/icon_cat-custom-graphs.svg
preview_img: placeholder.jpg
nav_order: 5
has_children: true
---

{% include header_card %}

> Contrary to mathematical graphs built with the {% include lk id='Graphs' %} nodes, **Custom Graphs** have no intrinsinc --nor guaranteed-- properties.
{: .infos-hl }

## Overview

The basic workflow is as follow:
1. Define custom graph params using {% include lk id='Custom Graph Params' %}. These params are a list of *sockets* that define an opportunity for connection.
2. Build a custom graph using the previously defined params, and {% include lk id='Build Custom Graph' %}, using any points.

## How Sockets Works



---
{% include card_deep_childs parent="Graph Solvers" %}