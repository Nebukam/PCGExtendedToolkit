---
icon: vector-square
---

# Clusters

Key takeaway:\
\- Vtx and Edges are separated but bounds by tags\
\- A single Vtx dataset covers multiple dataset by default\
\- A single Edge dataset represent a connectivity "cluster"\
\- Edges and Vtx are still "just points"

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

## Don't mess with PCGEx/

What makes PCGEx points _peeceegeeayksey_ are special attributes & tags — all of which are conveniently prefixed with `PCGEx/`.

{% hint style="danger" %}
**PCGEx rely on these to work, serialize internal data, and maintain vtx/edge relationships.**\
If you're curious, you can read more about the how's and why's in the dedicated technical note.
{% endhint %}

{% content-ref url="clusters/technical-note-clusters.md" %}
[technical-note-clusters.md](clusters/technical-note-clusters.md)
{% endcontent-ref %}
