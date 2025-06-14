---
icon: vector-square
---

# Clusters

{% hint style="info" %}
Make sure to read [Clusters 101](../../basics/pcgex-101/clusters.md) first.
{% endhint %}

Building clusters is very straightforward — all you need are points that you want to connect.&#x20;

There are many tool available to create these connections, combine them, refine them, etc. You'll also want to see what these connections look like; and ultimately leverage all that data you just created.

> The most important thing to understand is that a cluster is hardly something usable "as-is". It's a structure on top of which you can build more intricate things.

## Building Clusters

There's a handful of nodes to create graph from scratch, depending on what your initial data looks like, as well as what you want to achieve. Different ways to build clusters are covered in the [Hello Cluster](hello-cluster/) section. You can also create [branching structures](branching-structures.md) from paths — and the best thing, **you can combine and merge cluster together to create more complex, interconnected clusters**.

<figure><img src="../../.gitbook/assets/image (6) (1).png" alt=""><figcaption></figcaption></figure>

{% hint style="info" %}
You can even turn static mesh topology into clusters!
{% endhint %}

### Combining and refining clusters

Combining and refining cluster is an important part of the cluster workflow.

Each approach has its strengths and weaknesses, and while most of the time basic diagrams gives good results, it's not enough. On the other end, more precise tools like connect points may lack some of the more organic output of other approaches — and it's fine! One node can't do it all, PCGEx is complex enough as it is.

### Display edges

PCGEx comes with a subgraph that helps visualizing edges. It's what is used everywhere in this doc whenever the image of a cluster in editor shows up.

<figure><img src="../../.gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

## Important technicalities

{% hint style="info" %}
Most cluster node have some shared settings, you can read more about these in the [Cluster](../../node-library/clusters/) node library page.
{% endhint %}

When it comes to cluster, there are a few important things to know; most importantly :&#x20;

1. **An edge dataset represents a group of connected edges, while a vtx dataset can be associated with multiple edge groups**. You can dive into the specifics about this [here](technical-note-clusters.md).
2. Vtx ordering can drastically change when there are additions & deletion; **they are sorted based on spatial positioning, in X, Y then Z order.**

{% hint style="success" %}
If you need to, you can break down the vtx in such a way that each edge group is associated to a vtx dataset that only contains the points it connects using [Partition Vtx](../../node-library/clusters/packing/partition-vtx.md).
{% endhint %}

While this seems a bit odd at first, it's very handy once you wrapped you head around it. It also makes things such as [looping over clusters](looping-over-clusters.md) easier to achieve.

### Removing Vtx or Edges points with vanilla nodes

You can work on Vtx and Edges points the same way you would any other point data in your PCG graph. However, there is one important edge case : **if your remove any points from any of these datasets, you will need to sanitize the clusters before they can be use again by PCGEx nodes**.

> Cluster operations rely on the assumption that **a single Edge dataset only contains interconnected data.** If you remove edges or vtx "manually" (_e.g without PCGEx knowing about it first-hand_) this assumption is not guaranteed to be true anymore.
>
> Sanitizing a Cluster ensures that the interconnectivity is valid, and that edge data is properly partitioned to reflect that.
>
> As a result, sanitizing clusters may create new, smaller edges datasets.

{% hint style="success" %}
**Changing points properties and attributes can be done safely,** **and does not require cluster sanitization.**
{% endhint %}

{% hint style="success" %}
Unless specified otherwise, PCGEx nodes that deal will clusters will always output working data, so **you should never have to sanitize the output of a PCGEx node**.
{% endhint %}

### Don't mess with PCGEx/

What makes PCGEx points _peeceegeeayksey_ are special attributes & tags — all of which are conveniently prefixed with `PCGEx/`.

{% hint style="danger" %}
**PCGEx rely on these to work, serialize internal data, and maintain vtx/edge relationships.**\
If you're curious, you can read more about the how's and why's in the dedicated technical note.
{% endhint %}

{% content-ref url="technical-note-clusters.md" %}
[technical-note-clusters.md](technical-note-clusters.md)
{% endcontent-ref %}
