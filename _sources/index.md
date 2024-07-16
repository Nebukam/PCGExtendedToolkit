---
layout: page
title: PCG Extended Toolkit
nav_order: 0
#permalink: /features/
---
<div class="product-header large" style="--img:url('{{ site.baseurl }}/assets/images/logo.png');"><div class="infos" markdown="1">
# PCGEx {% include github.html repo="PCGExtendedToolkit" %}  
The PCG Extended Toolkit is a free (libre) plugin that extends Unreal Engine' Procedural Content Generation pipeline, with a focus on **building clusters & pathfinding**.  
{: .fs-6 .fw-300 }  
   
{% include link_btn title="Installation" color="red" link="installation" %} 
{% include link_btn title="Guides" color="blue" link="guides" icon="left" %}
</div></div>

--- 
> **This documentation is still heavily work-in-progress, and matches non-release branch. Sorry >.<**
> However, every property already has helpful tooltips in editor ;)
{: .error }

> **Make sure to check the [Example Project](https://github.com/Nebukam/PCGExExampleProject) on github**
{: .infos-hl }

---
# All Nodes

---
## Clusters Nodes
<br>
{% include card_childs reference="Clusters" tagged='clusters' %}

---
## Edges Nodes
<br>
{% include card_childs reference="Edges" tagged='edges' %}

---
## Pathfinding Nodes
<br>
{% include card_childs reference="Pathfinding" tagged='pathfinder' %}

---
## Paths Nodes
<br>
{% include card_childs reference="Paths" tagged='paths' %}

---
## Misc Nodes
<br>
{% include card_childs reference="Misc" tagged='misc' %}

---
## Sampling Nodes
<br>
{% include card_childs reference="Sampling" tagged='sampling' %}
