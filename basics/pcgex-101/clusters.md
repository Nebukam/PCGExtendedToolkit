---
icon: vector-square
---

# Clusters

Key takeaway:\
\- Vtx and Edges are separated but bounds by tags\
\- A single Vtx dataset covers multiple dataset by default\
\- A single Edge dataset represent a connectivity "cluster"\
\- Edges and Vtx are still "just points"

## Don't mess with PCGEx/

What makes PCGEx points _peeceegeeayksey_ are special attributes & tags — all of which are conveniently prefixed with `PCGEx/`.

{% hint style="danger" %}
PCGEx rely on these to work, serialize internal data, and maintain relationships.\
If you're curious, you can read more about the how's and why's in the dedicated technical note.
{% endhint %}

{% content-ref url="clusters/technical-note-on-clusters.md" %}
[technical-note-on-clusters.md](clusters/technical-note-on-clusters.md)
{% endcontent-ref %}
