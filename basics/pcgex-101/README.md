---
description: Important bits & principles
icon: hexagon
---

# PCGEx 101

PCGEx is built on top of regular PCG, and in the grand scheme of things, **PCGEx is just&#x20;**_**more of**_**&#x20;PCG**.

However, there is a few key things that are important to know, as some behaviors are oh-so-slightly different from the way the vanilla plugin handle things. Some of them are design choices, others are the result of trying to work around the existing limitation of the framework.

## It's just points

The most important thing to keep in mind is PCGEx Clusters and Paths are _just points_. **You can manipulate them just like any other points data, everything works seamlessly with the vanilla nodes**, and this is true the other way around as well: PCGEx nodes that work with points will accept any points, not just the ones coming out of PCGEx nodes.

{% hint style="success" %}
PCGEx may have fancy words like `Vtx`, `Edges`, `Paths`, but **these are just regular points**.
{% endhint %}

You can read more about what makes Clusters & Paths points special, **and why they're not**.

<table data-card-size="large" data-view="cards"><thead><tr><th></th><th></th><th data-hidden data-card-target data-type="content-ref"></th></tr></thead><tbody><tr><td><strong>Clusters</strong></td><td>How to work with clusters</td><td><a href="clusters.md">clusters.md</a></td></tr><tr><td><strong>Paths</strong></td><td>What's up with "Paths" points</td><td><a href="paths.md">paths.md</a></td></tr></tbody></table>
