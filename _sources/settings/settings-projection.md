---
title: settings-projection
has_children: false
nav_exclude: true
---


The projection settings control how the point position is translated to a 2D space before the graph is computed; *and how this projection will translate back to the original space, if relevant.*  

| Property       | Description          |
|:-------------|:------------------|
| Projection Normal           | Normal vector of the plane used for projection.<br>By default, the projection plan normal is `Up`; so the graph is computed over the `X Y` plane.  |
| Local Projection Normal           | If enabled, uses a per-point projection vector. |
| Local Normal           | Attribute ti read normal from, |

> Local projection normal is very powerful but can also be very clunky to use -- **it's very easy to end up with singularities that will prevent the graph from being properly computed.**
{: .warning-hl }