---
hidden: true
icon: microchip
---

# Technical Note : PCGEx Framework

## Parallelized input processing

Something PCGEx does fundamentally different from the vanilla toolset is that each node processes all inputs in parallel, asynchronously, outside of the game thread.

This comes with some extra wall time overhead as there's some back-and-forth going and waiting here and there, and may be a bit more greedy than the original toolkit in some places. **This choice is primarily driven by the fact that working with PCGEx often means dealing with lots of small dataset as opposed to a few bigger ones. And this makes everything&#x20;**_**extremely slow**_**&#x20;when daisy-chained.**

{% hint style="info" %}
A handful of nodes still process input one after the other due to the nature of the task they're handling which can be hard to properly parallelize.
{% endhint %}



## Performance Settings

There are a few performance options available to get a bit of a low-level influence on how PCGEx do things. **Tweaking these is in the realm of micro-optimization, so don't look into too much**.

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Work Priority</strong></td><td>Work Priority lets you choose the priority of the async tasks spawned by that node.</td></tr><tr><td><strong>Cache Data</strong></td><td>Lets you choose the caching behavior for that node.<br>- <code>Default</code> will use the value set in the plugin settings (<em>Disabled by default</em>)<br>- <code>Enabled</code> will enable data caching.<br>- <code>Disabled</code> will disable data caching.<br><br><em>Not all nodes react well to caching, and a handful of nodes simply don't support it and won't change behavior based on the setting value.</em></td></tr><tr><td><strong>Flatten Output</strong></td><td>If enabled, the node will output flattened data; meaning there is no more parenting or inheritance of the metadata. <br><br><strong>This takes a huge toll on memory usage and should only be used when exporting data to a <code>PCGDataAsset</code>.</strong></td></tr><tr><td><strong>Scoped Attribute Get</strong></td><td>This is a highly circumstantial setting: when enabled (and supported), the node will read attribute values it relies on in small chunks on a "per-need" basis; while if disabled, the entirety of the values is cached in a single go. <a href="technical-note-pcgex-framework.md#scoped-chunked-attribute-reading"><em>Read more about that here.</em></a><br><br><br>You may choose the following values:<br>- <code>Default</code> will use the value set in the plugin settings (<em>Enabled by default</em>)<br>- <code>Enabled</code> will enable chunked read.<br>- <code>Disabled</code> will disable chunked read.</td></tr></tbody></table>

## Scoped/Chunked Attribute Reading

Ramble about the impact of scoped getters

### Advantages

### Caveats

## Data Sharing during processing

Make a note on facade and shared attribute buffers\
\- Access attribute once, make data available for all sub nodes and instanced behavior that are currently connected\
\- Higher memory footprint, faster read time, especially when there's a lot of operation involved. Massive gains at scale
