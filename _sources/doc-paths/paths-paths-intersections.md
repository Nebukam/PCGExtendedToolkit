---
layout: page
family: Path
#grand_parent: All Nodes
parent: Paths
title: Path × Path Crossings
name_in_editor: "Path × Path Crossings"
subtitle: Find crossings within & against other paths.
summary: The **Path × Path Crossings** node detects intersections between path segments, either within the same path or against others, blending attributes from intersecting points and providing detailed control over how crossings and blending are handled.
color: white
splash: icons/icon_edges-intersections.svg
tagged: 
    - node
    - paths
nav_order: 2
inputs:
    -   name : Paths
        desc : Paths which edges will be checked for crossings
        pin : points
outputs:
    -   name : Paths
        desc : Paths with additional points & informations
        pin : points
    -   name : Can Cut Conditions
        desc : Filters used to determine whether a segment can "cut" other segments
        pin : params
    -   name : Can Cut Conditions
        desc : Filters used to determine whether a segment can be "cut" by cutter segments
        pin : params
---

{% include header_card_node %}

The **Path × Path Crossings** find all paths segments intersections either against all other inputs, or only against itself. Created intersection can capture attributes from the the foreign path' point, amongst other useful things.
{: .fs-5 .fw-400 } 

{% include img a='details/paths-solidify/lead.png' %}

# Properties
<br>

{% include embed id='settings-closed-loop' %}

| Self-intersection Only | If enabled, input paths will only check for crossing against themselves, as opposed to against every other input path. |

---
## Intersection Details
<br>

{% include embed id='settings-intersect-edge-edge' %}

---
# Blendings
<br>

Blending on this node happen in a very specific order:
- First, crossing point property are calculated as a blend between the start & end point of the segment that's being cut.*This considers internal properties & attributes only.*
- *Then*, if enabled, cross-blending is processed: this pass blends attributes from the foreign crossing data to bleed onto the crossing point.*This only considers external properties & attributes only.*
{: .fs-5 .fw-400 } 

---
## Blending
<br>

{% include card_childs tagged="blending" %}

---
## Cross Blending

> If there is an overlap, cross-blending will overrides any previously blended values.
{: .warning }

### Crossing Carry Over
{% include embed id='settings-carry-over' %}

### Crossing Blending
See {% include lk id='Blending' %}
{: .fs-5 .fw-400 } 

---
# Outputs
<br>

| Output       | Description          |
|:-------------|:------------------|
| <span class="eout">Crossing Alpha</span><br>`double` | Write the normalized position (alpha) of the crossing.*The alpha of a crossing is a `0..1` value representing where it sits between start and end point of the original segment.* |
| Crossing Orient Axis | If enabled, lets you pick a local transform axis of the crossing point to orient in the crossing' segment direction. |
| <span class="eout">Cross Direction</span><br>`FVector` | Similar to the option above, only writes the crossing segment' direction to an attribute instead of applying it to the point rotation. |
{: .soutput }

---
## Tagging
Some high level tags may be applied to the data based on overal intersections results.
<br>

| Tag       | Description          |
|:-------------|:------------------|
| <span class="etag">Has Crossings Tag</span>     | If enabled, add the specified tag to the output data **if at least a single crossing** has been found. |
| <span class="etag">Has No Crossings Tag</span>     | If enabled, add the specified tag to the output data **if no crossings** have been found. |

> Note that crossing detection tagging will be affected by points filter as well; since filtered out points are omitted from processing.
{: .warning }