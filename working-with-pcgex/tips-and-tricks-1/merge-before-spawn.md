---
description: A General PCG Trick
icon: merge
---

# Merge before Spawn

As you may know by now if you checked the [PCGEx technical note](../tips-and-tricks/technical-note-pcgex-framework.md), vanilla PCG nodes process the input data one after the other — that's a strong design choice and it's good for your memory; but it also makes processing lots of smaller dataset _very slow_.

Most of the time, **there is a significant speed gain to be had by merging point data right before you feed them into a static mesh spawner** — the same applies to debug nodes.

{% hint style="success" %}
This is even more important if your data is about to generate multiple exclusive ISMC configurations.
{% endhint %}

<figure><img src="../../.gitbook/assets/image (14).png" alt=""><figcaption></figcaption></figure>

### Buy why?

The spawner' partitioning code is actually very efficient, but it has to be setup in a particular way each time a new data object is processed, and when there is a lot of inputs to process, it tanks the performance.

**Merging point is obviously not free, but it's definitely worth looking into if you find your spawners to be sluggish.**

## **Less Spawners is better**

{% hint style="danger" %}
Even if using the exact same configuration, using two different nodes will generate two unique components; **ultimately increasing drawcalls.**
{% endhint %}

On that same note, it's worth mentioning that less spawners is much better as well, so make sure to consolidate your point data before spawning as well.

ISMC (Instanced Static Mesh Component) are created for each unique combination of the following criterias:

* Static Mesh
* Material
* Descriptor overrides
* **Node**       ← _\*sad trombone\*_
