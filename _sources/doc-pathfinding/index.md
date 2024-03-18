---
layout: page
title: Pathfinding
subtitle: Modules & Documentation
summary: This section is dedicated to broader pathfinding topics & modules. Node specifics can be found on their dedicated node page.
color: white
splash: icons/icon_cat-pathfinding.svg
preview_img: placeholder.jpg
nav_order: 30
has_children: true
---

{% include header_card %}

> Pathfinding modules for {% include lk id='Pathfinding' %} nodes. Key steps include heuristic computation, goal picking, and search algorithms finding the best path based on weighted connections. Heuristic modifiers heavily influence outcomes. Note: Plot nodes handle point datasets differently, finding a path through each point in order.

---
## {% include lk id='Pathfinding' %}  
<br>  
<div class="card-ctnr duo" markdown="1">
{% include card_single reference="Edges Pathfinding" %}
{% include card_single reference="Plot Edges Pathfinding" %}
</div>

---

## How pathfinding works
Although details vary a bit depending on the selected {% include lk id='Search' %} algorithm, the basic gist is, for each path & cluster:
1. {% include lk id='Heuristic Modifiers' %} are computed and cached for each node and edge.
2. {% include lk id='Goal Pickers' %} will find a suitable `Seed` and `Goal` point within the cluster.
3. The Search Algorithm will then find the best path that goes from `Seed` to `Goal`, accounting for its internal maths, and using {% include lk id='Heuristic Modifiers' %} as additive weight to determine whether one connection is better than another.

{% include imgc a='pathfinding/heuristic-score.png' %} 

>Note: The `Seed` and `Goal` node are picked based on closest distance to input *positions*.
{: .comment }

Starting from the seed, each "next step" is weighted according to the `V` Vertex weight and the `E` Edge weight that connects to it.  
The search returns the path found **with the lowest possible weight**, or *score*.

{% include imgc a='pathfinding/pathing.png' %}  

While the selected search algorithm is important, {% include lk id='Heuristic Modifiers' %} are even more critical to the operation, as user-defined weights can drastically change and shape the path deemed best by the search.

>Note: The `Plot` nodes variations don't have a goal picker and instead process each point data set as *a list of points to go through from start to finish*. The first point is the initial seed, the last point is the final goal, and then a path is found that goes through each point in-between, in order.
{: .comment }

{% include imgc a='pathfinding/ploting.png' %}  

---
## Pathfinding Nodes
<br>
{% include card_childs tagged='node' %}

---
{% include embed id='all-pathfinding-modules' %}