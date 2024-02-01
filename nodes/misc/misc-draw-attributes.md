---
layout: page
parent: Misc
grand_parent: Nodes
title: Draw Attributes
subtitle: Draw lines and points from attributes
color: red
summary: This node is used in 100% of the screenshots of this documentation.
splash: icons/icon_misc-draw-attributes.svg
preview_img: docs/splash-debug.png
toc_img: placeholder.jpg
tagged: 
    - misc
nav_order: 4
---

{% include header_card_node %}

{% include img a='details/details-draw-attributes.png' %} 

Each **Draw Attribute** node can display any number of things by fetching values from whatever point data is plugged into the input pin; in the order in which they are set up.

---
## Individual Debug Settings

{% include img a='details/details-draw-attributes-rule.png' %} 

| Property       | Description          |
|:-------------|:------------------|
|**Settings**||
| Enabled           | Whether these settings are enabled or not. Allows to quickly turn a debug display on/off without deleting the entire entry. |
| Selector          | The attribute or property to draw. |
| Expressed As          | The type of shape/form that will be used to express the selected attribute or property. |

|**Expression Settings**||
| --          | Depending on the selected expression, different settings are available. See[Expressions](#expressions). |

|**Thickness & Size**||
| Thickness          | The thickness of the debug line, when drawing a line. |
| Size          | How the `Size` is interpreted depends on the chosen expression. See[Expressions](#expressions). |
| Local Size Attribute          | When enabled, allows you to use a local attribute as a `Size`.<br>*If enabled, the fixed `Size` attribute becomes a multiplier to the local attribute.* |

|**Color**||
| Color          | The debug color. |
| Local Color Attribute          | When enabled, allows you to use a local attribute instead of the default `Color` property. |
| Color Is Linear          | Specifies whether the `Local Color` attribute is linear (0-1 based) or hex (0-255).<br>*If disabled, the attribute or property value will be divided by 255 internally.* |
| Depth Priority          | Debug draw depth priority. <br>`-1` : draw on top of everything.<br>`0` : Regular depth sorting.<br>`1` : Draw behind everything. |

---
## Expressions

As of writing time, there are a few expression available:
- [Direction](#direction)
- [Connection (Position)](#connection-position)
- [Connection (Point index)](#connection-point-index)
- [Point](#point)
- [Boolean](#boolean)
- ~~[Label](#label)~~

---
### Direction
<br>
{% include img a='docs/draw-attributes/direction.png' %} 

|**Extra Properties**||
|:-------------|:------------------|
| Normalize Before Sizing           | If enabled, the incoming vector will be normalized before it is resized and drawn. |

---
### Connection (Position)
<br>
{% include img a='docs/draw-attributes/connect.png' %} 

Draws a line between the current point' location and the selected attribute or property as a world space position.

|**Extra Properties**||
|:-------------|:------------------|
| As an offset           | If enabled, the incoming vector will be used as an offset from the current point location. |

---
### Connection (Point Index)
<br>
{% include img a='docs/draw-attributes/connect-index.png' %}  

Draws a line between the current point' location and another point within the same group as a world space position.  
The selected attribute or property is used as the index for the point to use as target position.

|**Extra Properties**||
|:-------------|:------------------|
| As an offset           | If enabled, the incoming vector will be used as an offset from the current point location. |

> Note: this is a legacy tool for drawing edges, if using graphs, use {% include lk id='Draw Edges' %} instead.
{: .comment }

---
### Point
<br>
{% include img a='docs/draw-attributes/point.png' %} 

|**Extra Properties**||
|:-------------|:------------------|
| As an offset           | If enabled, the incoming vector will be used as an offset from the current point location. |

---
### Boolean
<br>
{% include img a='docs/draw-attributes/bool.png' %} 

Boolean is similar to Point, except it is drawn at the point' location in space.  
The debug color is selected based on the input value: **If the value is `<= 0` the base color will be picked; otherwise `Secondary Color` is used.**

|**Extra Properties**||
|:-------------|:------------------|
| Secondary Color           | The color to be used for values `> 0`. |

---
### ~~Label~~
>Label is currently not working as expected, despite using engine APIs and not throwing any error when used.