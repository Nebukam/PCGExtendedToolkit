---
description: A list of things that are true for every PCGEx node.
icon: circle-exclamation
---

# Shared Concepts

## Common Settings

There is a handful of recurring settings blocs that are exposed by a lot of different nodes; you can find more about these in their own page. Individual nodes will be pointing back at them _and avoid duplicating the same doc in a lot of places_.

> To in order to highlight the different, common settings are accessible via buttons (_instead of regular links_) like so:
>
> <a href="comparisons.md" class="button secondary">Comparison Settings</a>  -  <a href="fitting.md" class="button secondary">Fitting Settings</a>  -  <a href="blending.md" class="button secondary">Blending Settings</a>

### Note on Dynamic Parameters

A lot of parameters support either a constant value or can grab a per-point value from the attributes. This is often presented as a dropdown that swaps two fields:

<figure><img src="../../.gitbook/assets/image.png" alt=""><figcaption></figcaption></figure>

Selecting `Constant` will read value from the constant field, which can be overridden with the pin of the same name; while selecting `Attribute` will read the value from the specified attribute. Attribute selection can be overridden using the override pin named `{property} (Attr)`

{% hint style="info" %}
If you are using override pins, **make sure that the input selection in the detail panel matches the override you want**, as it won't automatically switch for you.
{% endhint %}

***

## Shared Settings

Most PCGEx nodes share the same base class implementation when it comes to processing points, hence there are a few settings that are exposed by default on each and every node.

These lets you tweak very low-level processing behaviors & patterns.

### Performance

{% hint style="info" %}
You probably shouldn't mess with these; if you want to read more about them, check [this technical note](../../basics/pcgex-101/technical-note-pcgex-framework.md#performance-settings).
{% endhint %}

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Flatten Output</strong></td><td>If enabled, the node will output flattened data; meaning there is no more parenting or inheritance of the metadata. <br><br><strong>This takes a huge toll on memory usage and should only be used when exporting data to a <code>PCGDataAsset</code>.</strong></td></tr></tbody></table>

### Cleanup

Cleanup settings is exposed everywhere but only supported some specific cases. This idea behind cleanup is to **discard attributes from the output if those attributes were used by the node execution**. It's pure QoL/micro-optimization.

Those are called "consumable attributes" because the whole point of their existence was to be read once and then discarded.

{% hint style="info" %}
More often than not, people will just stack up attributes they don't need, or remove them with an extra "delete attribute" node. This embedded feature lets you do that during processing, removing the need for yet-another-copy of the data.
{% endhint %}

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Cleanup Consumable Attributes</strong></td><td>Whether to enable this feature or not.<br>When enabled, <strong>any attribute that is read from and not written to</strong> to during execution will be marked as "consumable", and discarded from the output data.<br><br><em>These are usually attributes used for per-point operations.</em></td></tr><tr><td><strong>Protected Attributes</strong><br><em>(String)</em></td><td>An easily overridable, comma separated list of attributes that should not be discarded even if they are registered as consumable.</td></tr><tr><td><strong>Protected Attributes</strong><br><em>(Array)</em></td><td>A static array of names, each of which will not be discarded even if they were registered as consumables during execution.</td></tr></tbody></table>

### Warning and Errors

This section exposes controls that allow you to quiet some warnings and errors. When nodes are used inside dynamic subgraphs, there are situation where some error are expected, and despite you handling them gracefully, the nodes will still complain that something is wrong (_this is also true of vanilla nodes, it's not a PCGEx thing_).&#x20;

Error/warning toggle exist to that effect : they will quiet logging code from shouting at you.

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Quiet Missing Input Error</strong></td><td>Stops the node from complaining about having no inputs or only empty inputs.</td></tr></tbody></table>

***

## Atypical Behaviors

{% hint style="warning" %}
**There's a handful of things PCGEx does slightly differently than the canon implementation.** It's usually not an issue, but in some very rare cases these inconsistencies can introduce a lot of confusion.
{% endhint %}

### Silent discard of empty data

All PCGEx nodes automatically discard & don't process empty data (_dataset with zero entries or points_).

### Data De-duping

All PCGEx nodes automatically proceed to an input de-duping based on memory pointer.&#x20;

This means that if a collection has the same data multiple times and only differ in tags, only one entry will be processed.

### Tags De-duping

All PCGEx nodes automatically de-dupe input tags, but also value tags.&#x20;

This means that a data tagged with `[Tag1, Tag2, Tag2, Value:A, Value:B, OtherValue:42]` will be output with a single `Value:` tag which value will be the first in the list, and no other duplicates resulting in `[Tag1, Tag2, Value:A, OtherValue:42]` .
