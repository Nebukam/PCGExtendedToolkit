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

> Pathfinding modules for {% include lk id='Pathfinding' %} nodes. Key steps include heuristic computation, goal picking, and search algorithms finding the best path based on weighted connections. {% include lk id='🝰 Heuristics' %} and their weights are key to the operation. *Note: Plot nodes handle point datasets differently, finding a path through each point in order.*

---
## {% include lk id='Pathfinding' %}  
<br>  
<div class="card-ctnr duo" markdown="1">
{% include card_single reference="Edges Pathfinding" %}
{% include card_single reference="Plot Edges Pathfinding" %}
</div>

---

## How pathfinding works
Although details vary a bit depending on the selected {% include lk id='⊚ Search' %} algorithm, the basic gist is, for each path & cluster:
1. {% include lk id='🝓 Goal Pickers' %} will find a suitable `Seed` and `Goal` point within the cluster.
2. The Search Algorithm will then find the best path that goes from `Seed` to `Goal`, accounting for its internal maths, and using {% include lk id='🝰 Heuristics' %} as to determine whether one connection is better than another.

{% include imgc a='pathfinding/heuristic-score.png' %} 

>Note: The `Seed` and `Goal` node are picked based on closest distance to input *positions*.
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
{% include card_childs tagged='node' %}

---
{% include embed id='all-pathfinding-modules' %}