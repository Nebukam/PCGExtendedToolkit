---
description: A list of things that are true for every PCGEx node.
icon: circle-exclamation
---

# Shared Concepts

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

***

## Common Settings

Most PCGEx nodes share the same base class implementation when it comes to processing points, hence there are a few settings that are exposed by default on each and every node.

These lets you tweak very low-level processing behaviors & patterns.

### Performance

There are a few performance options available to get a bit of a low-level influence on how PCGEx do things. Have a look at the [PCGEx Framework technical note](../../basics/pcgex-101/technical-note-pcgex-framework.md) to get a better sense of their context.

{% hint style="info" %}
Defaults performance settings are all good — they are mostly in the realm of micro-optimization and much else.
{% endhint %}

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Work Priority</strong></td><td>Work Priority lets you choose the priority of the async tasks spawned by that node.</td></tr><tr><td><strong>Cache Data</strong></td><td>Lets you choose the caching behavior for that node.<br>- <code>Default</code> will use the value set in the plugin settings (<em>Disabled by default</em>)<br>- <code>Enabled</code> will enable data caching.<br>- <code>Disabled</code> will disable data caching.<br><br><em>Not all nodes react well to caching, and a handful of nodes simply don't support it and won't change behavior based on the setting value.</em></td></tr><tr><td><strong>Flatten Output</strong></td><td>If enabled, the node will output flattened data; meaning there is no more parenting or inheritance of the metadata. <br><br><strong>This takes a huge toll on memory usage and should only be used when exporting data to a <code>PCGDataAsset</code>.</strong></td></tr><tr><td><strong>Scoped Attribute Get</strong></td><td>This is a highly circumstantial setting: when enabled (and supported), the node will read attribute values it relies on in small chunks on a "per-need" basis; while if disabled, the entirety of the values is cached in a single go. <a href="../../basics/pcgex-101/technical-note-pcgex-framework.md#scoped-chunked-attribute-reading"><em>Read more about that here.</em></a><br><br><br>You may choose the following values:<br>- <code>Default</code> will use the value set in the plugin settings (<em>Enabled by default</em>)<br>- <code>Enabled</code> will enable chunked read.<br>- <code>Disabled</code> will disable chunked read.</td></tr></tbody></table>

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
