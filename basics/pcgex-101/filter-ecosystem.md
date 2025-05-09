---
icon: filter-list
---

# Filter Ecosystem

A lot of PCGEx nodes have filter pins. These pins accept any number & combinations of [Filters](../../node-library/filters/), the largest family of [sub-nodes](sub-nodes/) in the framework.&#x20;

**A PCGEx filter represents something can either be true or false for a given point or dataset** — there's filters available for a bunch of things.&#x20;

> The majority of them are [Point filters](../../node-library/filters/filters-points/), meaning they'll work with any points (_remember, everything is just points_), but there are also sub-categories that can work against more specific things (_i.e, cluster connectivity_)

<figure><img src="../../.gitbook/assets/image (17).png" alt=""><figcaption></figcaption></figure>

{% hint style="success" %}
Filters can be combined using AND/OR groups to create complex conditions, and sometime can greatly simplify graphs by removing the need for computing attributes for the sole purpose of filtering point. The [Distance](../../node-library/filters/filters-points/spatial/distance.md) filter is a good example of that.
{% endhint %}

### Filter as Conditions

PCGEx leverages that pattern to create conditional node behaviors — in other words, **whether something should happen or not to a point, if the network of connected filters says "yes" or "no"**.

> A lot of nodes also have simple "Point filters" inputs, instructing the node to only process points that meet the provided requirements.
>
> This is especially handy when you want to do chirurgical editing.

<figure><img src="../../.gitbook/assets/image (19).png" alt=""><figcaption></figcaption></figure>

<details>

<summary>Technical comment</summary>

The original motivation behind introducing that pattern in PCG (_which I know goes against native PCG paradigms under the hood_) is the fact that **most node that modify data (point or attribute) will create output a new copy of the incoming data**.

If you're trying to do something remotely complex, this can stack up really quick and fill up the RAM before you know it.

* Being able to batch complex conditional processing reduces the need for daisy-chaining filter nodes, and duplicating data all over the place.
* It also enable recurring checks that usually require writing attributes to do comparison later on to be processed on the fly, further removing the need for garbage/consumable attributes.

{% hint style="info" %}
On the note of garbage attributes, make sure to check the [Cleanup](../../node-library/shared-concepts/#cleanup) settings of PCGEx nodes!
{% endhint %}

</details>
