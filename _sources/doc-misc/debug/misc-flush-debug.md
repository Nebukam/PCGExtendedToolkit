---
layout: page
family: Debug
grand_parent: Misc
parent: Debug
title: Flush Debug
name_in_editor: "Flush Debug"
subtitle: Flushes persistent debug lines
summary: Flush persistent debug lines, to be used at the start of a PCG graph.
color: red
splash: icons/icon_hidden.svg
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