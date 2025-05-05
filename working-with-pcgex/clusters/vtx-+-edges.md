---
icon: question
---

# Vtx + Edges

Having Vtx & Edges data as separate pins & collections is important and useful most of the time. However, there come situations where you need to mix everything up into a single pin.

A _switch_, or _select_, various flow gates — or simply a subgraph that deals with clusters — **you don't have to duplicate all the logic & pins, every single time :** [**Find Clusters**](../../node-library/clusters/find-clusters-data.md) **is here for that.**

## Use _Find Cluster_ All

{% hint style="warning" %}
You may have be tempted to use [Packing](../../node-library/clusters/packing/) nodes to work around that issue — **don't do that**.\
&#xNAN;_&#x50;acking nodes are designed for exporting clusters and writing them to assets, and they do very expensive operations, where Find Clusters is practically free._
{% endhint %}

<figure><img src="../../.gitbook/assets/image (9).png" alt=""><figcaption></figcaption></figure>

### Convenient reroute

This approach makes rerouting cluster much simpler — sure it requires an extra node, but in most case it's simply tidier to a single "_My Cluster_" than two separate "_My Cluster (Vtx)_" and "_My Cluster (Edges)_"

<figure><img src="../../.gitbook/assets/image (10).png" alt=""><figcaption></figcaption></figure>

### Convenient flow control

Same thing apply to flow control nodes — just plug all that cluster data and find your chickens afterward.

<figure><img src="../../.gitbook/assets/image (11).png" alt=""><figcaption></figcaption></figure>

{% hint style="info" %}
The same approach is used, with a different node to [loop over clusters](looping-over-clusters.md).
{% endhint %}
