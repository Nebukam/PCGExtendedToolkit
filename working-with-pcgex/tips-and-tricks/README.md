---
icon: circle-info
---

# README.txt

PCGEx nodes, more often than not, will expose an overwhelming amount of parameters — don't be afraid, very few of them are actually worth your attention while doing the minute-to-minute work.&#x20;

For example, [Performance](../../node-library/shared-concepts/#performance) and [Cleanup](../../node-library/shared-concepts/#cleanup) settings are for advanced uses. [Warning and Errors](../../node-library/shared-concepts/#warning-and-errors) can come in handy if you get a bunch of expected errors you'd like to silence — better safe than sorry, as they say.

{% content-ref url="../../node-library/shared-concepts/" %}
[shared-concepts](../../node-library/shared-concepts/)
{% endcontent-ref %}

> The **Working with PCGEx** section is not about specifics! Instead the goal is provide a more casual introduction to the major concepts that make PCGEx great!

### Technical notes & Inner workings of PCGEx

If you're into that, there's a dedicated technical note that covers some things in the [Framework technical note](technical-note-pcgex-framework.md).

{% content-ref url="technical-note-pcgex-framework.md" %}
[technical-note-pcgex-framework.md](technical-note-pcgex-framework.md)
{% endcontent-ref %}

### Atypical Behaviors

{% hint style="warning" %}
**There's a handful of things PCGEx does slightly differently than the canon implementation.** It's usually not an issue, but in some very rare cases these inconsistencies can introduce a lot of confusion.
{% endhint %}

### Silent discard of empty data

All PCGEx nodes automatically discard & don't process empty data (_dataset with zero entries or points_). If there is no data after discard, the node will throw an error. _The only exception to that is the_ [_Discard by Point Count_](../../node-library/filters/discard-by-point-count.md) _node, because it's designed for graph culling._

### Data De-duping

All PCGEx nodes automatically proceed to an input de-duping based on memory pointer.&#x20;

This means that if a collection has the same data multiple times and only differ in tags, only one entry will be processed.

### Tags De-duping

All PCGEx nodes automatically de-dupe input tags, but also value tags.&#x20;

This means that a data tagged with `[Tag1, Tag2, Tag2, Value:A, Value:B, OtherValue:42]` will be output with a single `Value:` tag which value will be the first in the list, and no other duplicates resulting in `[Tag1, Tag2, Value:A, OtherValue:42]` .
