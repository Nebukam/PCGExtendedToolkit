---
hidden: true
icon: microchip
---

# Technical Note : Sub Nodes

### Factories

Sub-nodes are just factories. **Generally speaking, each sub-node will generate a single sub-processor per input of the node that request them** — it's more lightweight than it sounds, as these objects are regular CPP objects, not `UObject` s. They don't need to be garbage collected, and like `FPCGContext`, they're deleted as soon as they're not needed anymore.

{% hint style="info" %}
Since the context that request an instance of the "sub-processor" owns the data, it also becomes very easy to leverage PCGEx' internal [data sharing wrapper](../../../working-with-pcgex/tips-and-tricks/technical-note-pcgex-framework.md).
{% endhint %}

### Inversion of control, sort-of

Since the factory is implemented in a regular `UPCGData` object, it can be passed around like any other data object. This is especially important when developing complex toolchains where you'd want to sometime expose more control over certain conditions, you're not limited to what can be exposed anymore.

The [Filter Ecosystem](../filter-ecosystem.md) is a great example of that; but all sub-node work the same way. _It's just that filters are very uniquely benefiting from this pattern._

### Not serializable

As mentioned on the [Sub Nodes](./) page, one caveat is that these are not serializable — i.e, you can't save them as part of a PCG Data Asset, and I wouldn't expect them to work well when stored in a Graph Output either, unless executed at runtime. Try at your own risks!

The main reason behind non-serialization is that I'm trying to reduce the memory footprint, and **most of the content inside the factories only lives in memory**. Some pre-processing and dependency loading is also done when a factory is instanced, which can't be captured elegantly.

{% hint style="info" %}
On the other hand, this enable sub-nodes to have in-graph dependencies without having to bother too much about it.
{% endhint %}
