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
## {% include lk id='Filter Ecosystem' %}
<br>
{% include card_childs reference="Filter Ecosystem" tagged='filter-ecosystem' %}

---
## {% include lk id='Clusters' %}
<br>
{% include card_childs reference="Clusters" tagged='clusters' %}

---
## {% include lk id='Pathfinding' %}
<br>
{% include card_childs reference="Pathfinding" tagged='pathfinder' %}

---
## {% include lk id='Paths' %}
<br>
{% include card_childs reference="Paths" tagged='paths' %}

---
## ## {% include lk id='Staging' %}
<br>
{% include card_childs reference="Staging" tagged='staging' %}

---
## Misc Nodes
<br>
{% include card_childs reference="Misc" tagged='misc' %}

---
## Sampling Nodes
<br>
{% include card_childs reference="Sampling" tagged='sampling' %}


---
## Individual Filters
*There are here to enable `ctrl+f` users. You can find all about them in the {% include lk id='Filter Ecosystem' %} section.*
<br>
{% include card_any tagged='filter' %}