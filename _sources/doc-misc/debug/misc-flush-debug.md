---
layout: page
grand_parent: Misc
parent: Debug
title: Flush Debug
subtitle: Flushes persistent debug lines
summary: Flush persistent debug lines, to be used at the start of a PCG graph.
color: red
splash: icons/icon_hidden.svg
preview_img: docs/splash-flush-debug.png
toc_img: placeholder.jpg
tagged: 
    - node
    - misc
nav_order: 1
inputs:
    -   name : In
        desc : Anything
        pin : points
outputs:
    -   name : Out
        desc : Unaltered (gathered) inputs
        pin : points
---

{% include header_card %}

See {% include lk id='Debug' %}

---
# Inputs & Outputs
Anything. This node forwards out whatever goes in.