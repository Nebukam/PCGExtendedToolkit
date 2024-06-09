---
layout: page
grand_parent: All Nodes
parent: Custom Graphs
title: Delete
subtitle: Delete custom graph attributes.
color: blue
summary: The **Delete Custom Graph** node helps with custom graph attributes that have become garbage. It's important to delete them to speed up data processing once you don't need them.
splash: icons/icon_custom-graphs-delete.svg
preview_img: docs/splash-customgraph-delete.png
toc_img: placeholder.jpg
tagged: 
     - node
     - customgraph
nav_order: 6
---

{% include header_card %}

*This node has no specific properties.*

This node removes all the extra properties associated with custom graphs from a set of points.  **Depending on the complexity or amount of custom graph params associated with a set of points, it is a welcome optimization to just clean up the data as soon as you don't need it anymore.**  

>Note that by default, the {% include lk id='Promote Edges' %} does this cleanup internally.
{: .infos }

---
# Inputs & Outputs
See {% include lk id='Working with Graphs' %}