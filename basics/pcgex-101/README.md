---
description: Important bits & principles
icon: hexagon
---

# PCGEx 101

PCGEx is built on top of regular PCG, and in the grand scheme of things, **PCGEx is just&#x20;**_**more of**_**&#x20;PCG**.

However, there is a few key things that are important to know, as some behaviors are oh-so-slightly different from the way the vanilla plugin handle things. Some of them are design choices, others are the result of trying to work around the existing limitation of the framework.

## It's just points!

The most important thing to keep in mind is PCGEx Clusters and Paths are _just points_. **You can manipulate them just like any other points data, everything works seamlessly with the vanilla nodes**, and this is true the other way around as well: PCGEx nodes that work with points will accept any points, not just the ones coming out of PCGEx nodes.

{% hint style="success" %}
PCGEx may have fancy words like `Vtx`, `Edges`, `Paths`, but **these are just regular points**.
{% endhint %}

You can read more about what makes Clusters & Paths points special, **and why they're not**.

<table data-card-size="large" data-view="cards"><thead><tr><th></th><th></th><th data-hidden data-card-target data-type="content-ref"></th></tr></thead><tbody><tr><td><strong>Clusters</strong></td><td>You're probably here for them.</td><td><a href="clusters.md">clusters.md</a></td></tr><tr><td><strong>Paths</strong></td><td>They're just point, mostly.</td><td><a href="paths.md">paths.md</a></td></tr><tr><td><strong>Sub Nodes</strong></td><td>A new pattern to wrap your head around.</td><td><a href="sub-nodes/">sub-nodes</a></td></tr><tr><td><strong>Instanced Behaviors</strong></td><td>You can probably skip that bit.</td><td><a href="instanced-behaviors.md">instanced-behaviors.md</a></td></tr><tr><td><strong>Filter Ecosystem</strong></td><td>They're <em>everywhere</em>. Better get to know them.</td><td><a href="filter-ecosystem.md">filter-ecosystem.md</a></td></tr><tr><td><strong>Workflow</strong></td><td>What to expect when working with PCGEx.</td><td><a href="paths-1.md">paths-1.md</a></td></tr></tbody></table>



### Getting your hands dirty

Once you're done going through the different section of that 101, you can find some more concrete material in the "**Working with PCGEx**" section.

{% content-ref url="../../working-with-pcgex/tips-and-tricks/" %}
[tips-and-tricks](../../working-with-pcgex/tips-and-tricks/)
{% endcontent-ref %}

This is a bit of an odd section, but the goal is to provide some easing into a spectrum of situation, as well as some recipe to tackle some very specific, recurring problems.

{% content-ref url="../../working-with-pcgex/clusters/" %}
[clusters](../../working-with-pcgex/clusters/)
{% endcontent-ref %}

{% content-ref url="../../working-with-pcgex/paths/" %}
[paths](../../working-with-pcgex/paths/)
{% endcontent-ref %}

{% content-ref url="../../working-with-pcgex/tensors.md" %}
[tensors.md](../../working-with-pcgex/tensors.md)
{% endcontent-ref %}

{% content-ref url="../../working-with-pcgex/tips-and-tricks-1/" %}
[tips-and-tricks-1](../../working-with-pcgex/tips-and-tricks-1/)
{% endcontent-ref %}



