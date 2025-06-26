---
icon: circle-dashed
---

# Sub Nodes

One thing PCGEx introduces that's very alien when looking at the vanilla PCG architecture and patterns is the concept of "sub-nodes".

The closest parallel that can be drawn is, sub-nodes are like attribute sets **containing processing logic instead of data** — that's also why the visual semantic of these nodes is the same as attribute sets.&#x20;

{% hint style="info" %}
Sub-nodes exist in a variety of types, and PCGEx relies a lot on that modular approach to solve complex problems and offer great flexibility.
{% endhint %}

<figure><img src="../../../.gitbook/assets/image (15).png" alt=""><figcaption></figcaption></figure>

### A modular approach

Sub-nodes makes things more modular overall, even thought they can be a bit counter-intuitive to the pure data-flow oriented approach of the vanilla toolset, but the benefits and the way PCGEx leverages them are worth breaking the rules a bit.

#### Advantages

The major advantage of sub-nodes is all-over flexibility, inversion of control, and versatility. In a sense, they are like any other PCG nodes, meaning:

* They can be gathered
* They can be rerouted
* They can be passed around using subgraph pins

<figure><img src="../../../.gitbook/assets/image (1) (1) (1) (1) (1) (1) (1) (1) (1) (1) (1) (1) (1).png" alt=""><figcaption></figcaption></figure>

{% hint style="info" %}
In order to pass sub-nodes through a subgraph pin, `Attribute Set` type is preferred, but `Any` and `Point or Params` should work fine.
{% endhint %}

{% hint style="success" %}
If you use a recurring configuration of sub-nodes often (_i.e same nodes, same settings_), **consider wrapping them into a re-usable subgraph or use reroute nodes instead of duplicating them**.
{% endhint %}

#### Caveats

Caveats are minor, but worth mentioning:

* The output of these sub-nodes, whatever they are, **cannot be serialized.**&#x20;
* The PCG profiler fails at measuring the real performance impact of these nodes when they don't have input pins, and will instead show the time between the start of the graph execution and the moment where a sub-node was actually executed; **resulting is very high times when the real cost usually negligible as there is no processing at that point**.

{% hint style="info" %}
Sub-nodes profiling cannot be relied on, nor is representative of their costs. Instead, look at the timing of the "user" node they are plugged into, as the actual processing cost will be reflected there instead.
{% endhint %}

## Usages

Different systems & features use different types of sub-nodes. You can identify a sub-node in the documentation because their icon is a _dashed circle instead of a full circle_.

Sub-nodes are identified using a custom extra icon that's the same on both outputs & inputs.

<figure><img src="../../../.gitbook/assets/image (12).png" alt=""><figcaption><p>Sub Nodes output &#x26; input pins have an extra icon that you can't miss!</p></figcaption></figure>

{% hint style="success" %}
It's also worth nothing that whenever this pattern is used, **these input pins always support multiple sub-nodes**.
{% endhint %}

<figure><img src="../../../.gitbook/assets/image (3) (1) (1) (1) (1).png" alt=""><figcaption></figcaption></figure>

### Technical Notes

{% content-ref url="technical-note-sub-nodes.md" %}
[technical-note-sub-nodes.md](technical-note-sub-nodes.md)
{% endcontent-ref %}
