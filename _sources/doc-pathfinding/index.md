---
layout: page
title: Pathfinding
subtitle: Modules & Documentation
summary: This section is dedicated to broader pathfinding topics & modules. Node specifics can be found on their dedicated node page.
color: red
splash: icons/icon_cat-pathfinding.svg
preview_img: previews/index-pathfinding.png
nav_order: 21
has_children: true
tagged:
    - category-index
---

{% include header_card %}

## {% include lk id='Pathfinding' %}  
<br>  
<div class="card-ctnr duo" markdown="1">
{% include card_single reference="Edges Pathfinding" %}
{% include card_single reference="Plot Edges Pathfinding" %}
</div>

---

## How pathfinding works
Although details vary a bit depending on the selected {% include lk id='⊚ Search' %} algorithm, the basic gist is, for each path & cluster:
1. A suitable `Seed` and `Goal` point (Vtx) are picked within the cluster.
2. The Search Algorithm will then find the best path that goes from `Seed` to `Goal`, accounting for its internal maths, relying on {% include lk id='🝰 Heuristics' %} to determine whether one connection is better than another.
3. Once a path is found between a given `Seed` and its `Goal`, traversed points are then added to a dataset, in order from `Seed` (start) to `Goal` (end).

{% include imgc a='pathfinding/heuristic-score.png' %} 



>Note: The `Seed` and `Goal` node are picked based on closest distance to input *positions* only (point bounds are ignored)
{: .comment }

Starting from the seed, each "next step" is weighted according to the `V` Vertex weight and the `E` Edge weight that connects to it.  
The search returns the path found **with the lowest possible weight**, or *score*.

{% include imgc a='pathfinding/pathing.png' %}  

While the selected search algorithm is important, {% include lk id='🝰 Heuristics' %} are more critical to the operation, as user-defined weights can drastically change and shape the path deemed best by the search.

>Note: The `Plot` nodes variations don't have a goal picker and instead process each point Dataset as *a list of points to go through from start to finish*. The first point is the initial seed, the last point is the final goal, and then a path is found that goes through each point in-between, in order.
{: .comment }

{% include imgc a='pathfinding/ploting.png' %}  

---
## Pathfinding Nodes
<br>
{% include card_childs tagged='pathfinder' wrappercss="duo" %}
<br>

> Note that there is also two hidden node that enable pathfinding using the existing navmesh: {% include lk id='Navmesh Pathfinding' %} and {% include lk id='Plot Navmesh' %}. They're not part of the main pool because they're *very* legacy. *They can still be very useful if you do level blocking with a navmesh, not so much for open world.*
{: .infos-hl }

---
## Available {% include lk id='🝰 Heuristics' %} modules
<br>
{% include card_any tagged="heuristics" %}

---
## Available {% include lk id='⊚ Search' %} modules
<br>
{% include card_any tagged="search" %}