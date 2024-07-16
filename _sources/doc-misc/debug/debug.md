---
layout: page
parent: Misc
title: Debug
subtitle: Working with PCGEx Debug nodes
color: red
summary: Need-to-know stuff regarding internal visual debugging tools available in PCGEx.
splash: icons/icon_misc-draw-attributes.svg
preview_img: docs/splash-debug.png
nav_order: 100
tagged:
    - basics
has_children: true
---

{% include header_card %}

- TOC
{:toc} 

---
## Visual Debugging

PCGEx Debug node draw debug information flagged as `persistent`, and as such needs to be flushed.  
However, there is currently no way in Unreal to selectively flush or tag debug line -- hence they needs to be flushed before they are redrawn *(Or thousands of thousands of line willl stack and bring the editor to its knees)*.  
This means you need to use {% include lk id='Flush Debug' %} in your flow before using other PCGEx' debug nodes.

{% include img a='docs/debug.png' %}  

*The Flush Debug is basically there to manage execution order and ensure stuffs aren't flushed from the buffer right after they're drawn. It's non-intrusive, and sometimes needs an update or two to work right.*

---
## Debugging inside subgraphs
<br>
{% include img a='details/details-pcgex-debug.png' %} 

**When disabled, the input data of a node becomes a simple passthrough.**
The `PCGExDebug` property in the `Debug` details of the node is overridable and basically allows you to remotely disable the PCGEx debug nodes.  

---
## Available debug nodes
At the time of writing, there are three main debug nodes:
1. {% include lk id='Draw Attributes' %}
1. {% include lk id='Draw Edges' %}

`Draw Edges` is specific to Edges & Clusters, however {% include lk id='Draw Attributes' %} is designed to work with any input.