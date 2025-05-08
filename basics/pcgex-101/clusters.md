---
icon: vector-square
---

# Clusters

Clusters in PCGEx refers to two intertwined piece of data, Vtx and Edges, that once combined form a transient graph structure that represent connectivity between points.

That connectivity makes it easy to do things like **pathfinding**, **zoning**, finding **concave hulls**,  **layouts**, and more -- all with a very precise level of control, as opposed to purely distance-based approximations.

<figure><img src="../../.gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

Vtx represent the points (or _nodes,_ in [graph theory](https://en.wikipedia.org/wiki/Graph_theory)), and a single edge represent a connection between two unique vertices. There is a few high level rules that are always true:

{% stepper %}
{% step %}
#### Each edge is unique

Each edge represent a unique connection between two points, and there can be no duplicate. You don't have to do anything about that, it's just the way it works — _edge data simply cannot contain more than once a connection between the same two points._
{% endstep %}

{% step %}
#### Edges are undirected in nature

Edges don't store any "direction" data per-se. AB is the same as BA (_see the rule above_).

If direction matters, nodes will let you choose how that direction should be determined at the time of processing.
{% endstep %}

{% step %}
#### Clusters are not geometry

This is an important one in the context of real-time graphics : **PCGEx clusters are not geometry**. There is no concept of triangles, surface or anything like that. Just pure, abstract relationships.

> The goal however is to leverage those relationships and transform them into something that's immediately useful — but having that initial abstract connectivity data is key.
{% endstep %}
{% endstepper %}

### Why would you need clusters?

Being able to know and manipulate the idea of connectivity between points _precisely_ opens up a world of possibility when it comes to procedural generation.

> Emphasis on precisely — there's a lot of what PCGEx outputs that you might be able to approximate roughly with the vanilla framework! And there's also a lot that's simply not possible to achieve without PCGEx.&#x20;
>
> Associating data to the connections themselves as opposed to only endpoints enables a world of possibilities., since connections don't rely on distance or proximity anymore.

### Again, just points.

{% hint style="success" %}
**Vtx and Edges points, despite their custom labels, are no different from any other points.** \
Their key data is stored in regular attributes for everyone to see — no catch!
{% endhint %}

It's worth hammering that PCGEx is leveraging point data to store and represent that connectivity in a way that's spatially meaningful (_i.e., edges default position is exactly between the two vertices it connects_).

Hence, both Vtx and Edges are "just points", exactly like the you get out of a landscape sampler.\
There's obviously more technicalities when it comes to using them with PCGEx, which you can read about in more depth in the [Working with Clusters](../../working-with-pcgex/clusters/) section.

### Dive into clusters

{% content-ref url="../../working-with-pcgex/clusters/" %}
[clusters](../../working-with-pcgex/clusters/)
{% endcontent-ref %}

