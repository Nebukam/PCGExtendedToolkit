---
icon: hand-wave
cover: .gitbook/assets/1080Alpha.png
coverY: 0
layout:
  cover:
    visible: true
    size: full
  title:
    visible: true
  description:
    visible: false
  tableOfContents:
    visible: true
  outline:
    visible: true
  pagination:
    visible: true
---

# Welcome

## What is PCGEx?

The PCG Extended Toolkit (_PCGEx for short_) is an open source plugin that extends Unreal Engine’ Procedural Content Generation pipeline, with a focus on **building spatial relationships between points** and **path manipulation**.

It also comes with a lot of quality of life nodes, with support for a wide range of situations — even some very unique and niche things.

{% hint style="success" %}
PCGEx is available for Unreal **5.6, 5.5**, **5.4**, and **5.3**
{% endhint %}

### Jump right in

<table data-view="cards"><thead><tr><th></th><th></th><th data-hidden data-card-cover data-type="files"></th><th data-hidden></th><th data-hidden data-card-target data-type="content-ref"></th></tr></thead><tbody><tr><td><strong>Quickstart</strong></td><td>Requirements &#x26; Installation</td><td></td><td></td><td><a href="basics/quickstart/">quickstart</a></td></tr><tr><td><strong>Basics</strong></td><td>Learn the basics of PCGEx</td><td></td><td></td><td><a href="basics/pcgex-101/">pcgex-101</a></td></tr><tr><td><strong>Join the community!</strong></td><td>Come join the Discord</td><td></td><td></td><td><a href="https://discord.gg/U6FkuwTn">https://discord.gg/U6FkuwTn</a></td></tr></tbody></table>

## Highlights

How can PCGEx help you?

<figure><img src=".gitbook/assets/pcgex_users.png" alt=""><figcaption><p>Some of the studios that have integrated PCGEx into their pipelines</p></figcaption></figure>

### Directed & Undirected graphs and diagrams

PCGEx introduces undirected graphs, allowing to create, manipulate & leverage connectivity between points. Find cells, contours, pathfinding and more.

<figure><img src=".gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

{% content-ref url="node-library/clusters/" %}
[clusters](node-library/clusters/)
{% endcontent-ref %}

### Edge-based pathfinding & modular heuristics

Leveraging diagrams & connected graphs, PCGEx enable highly customizable pathfinding using a modular and flexible heuristic ecosystem.

<figure><img src=".gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

{% content-ref url="node-library/pathfinding/" %}
[pathfinding](node-library/pathfinding/)
{% endcontent-ref %}

### Tensor Ecosystem

PCGEx comes with a tensor field ecosystem that seamlessly integrate with all other features. It lets you create complex spatial effectors to transform points, constrain heuristics, floodfill, or simply generate paths.

<figure><img src=".gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

{% content-ref url="node-library/tensors/" %}
[tensors](node-library/tensors/)
{% endcontent-ref %}

### Modular filters

PCGEx has a very large library of useful filters than can be combined to create infinitely complex conditions with branching and more — but more importantly, the plugin leverages filters as a powerful tool to create conditional behavior allowing tight control in intricate situations.

<figure><img src=".gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

{% content-ref url="node-library/filters/" %}
[filters](node-library/filters/)
{% endcontent-ref %}

### Alternative, extended ways to sample & blend data

PCGEx has a variety of data samplers for points, splines, textures that streamline & consolidate common use-cases into much more straightforward setups.

<figure><img src=".gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

{% content-ref url="node-library/sampling/" %}
[sampling](node-library/sampling/)
{% endcontent-ref %}

### Paths Manipulation

PCGEx has a lot to offer when it comes to manipulating paths (think splines before they're splines), refining them, splitting them, cutting them, bevelling them, you name it.

<figure><img src=".gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

{% content-ref url="node-library/paths/" %}
[paths](node-library/paths/)
{% endcontent-ref %}

### Quality of Life

Last but not least, there is a lot of QoL nodes designed to reduce the need & use of loop subgraphs as much as possible, extract information & statistics in new ways, manage & prepare data for spawning, creating graph dependency chains at runtime, ... At the time of writing, there's more than 200 nodes in the library.

<figure><img src=".gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>
