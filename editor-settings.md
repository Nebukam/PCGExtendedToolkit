---
icon: gear
---

# Editor Settings

PCGEx expose a few settings, mostly default values & node colors. You can find them under **Project Settings → Plugins → PCGEx.**

## **Performance**

Tweaking performance settings usually have a limited impact in your project. There is no "one size fits all" when it comes to PCG — only good enough defaults.

Measuring their impact will require tweaking some of them in concert, and some time to dig into Unreal Insight :)

### **Defaults**

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Default Cache Node Output</strong></td><td>The default caching behavior for PCGEx nodes. <br>See <a href="working-with-pcgex/tips-and-tricks/technical-note-pcgex-framework.md#performance-settings">Caching</a> for more info.<br><em>It is recommended to leave this value to default <code>false</code> value.</em></td></tr><tr><td><strong>Default Scoped Attribute Get</strong></td><td>The default attribute read behavior for PCGEx nodes that support it.<br><em>See</em> <a href="working-with-pcgex/tips-and-tricks/technical-note-pcgex-framework.md#performance-settings"><em>Performance Settings</em></a> <em>for more info.</em></td></tr></tbody></table>

{% hint style="info" %}
Scoped Attribute get can sometimes make a huge difference in processing time. While it's definitely better to enable/disable it on a per-node basis, this option make it handy to do some quick and dirty testing without manually going through a bunch of nodes.
{% endhint %}

### **Cluster**

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Small Cluster Size</strong></td><td>What number of point should be considered a "small" cluster. <br>Small clusters are batched together instead of being broken down into smaller chunked for parallel processing.</td></tr><tr><td><strong>Cluster Default Batch Chunk Size</strong></td><td>The default chunk size when splitting input points into chunks for parallel processing.<br><br><em>Impact is usually more measurable when <code>Scoped Attribute Get</code> is enabled; as initial attribute read is often the worst offender.</em></td></tr><tr><td><strong>Cluster Scoped Index Lookup Build</strong></td><td>Controls whether rebuilding the index lookup for cluster should use scoped get or not.<br><br><em>Building the index lookup for cluster happens once per cluster whenever it needs to be processed, so it's pretty important that you check what kind of impact this has with your specific setup: it can either tank performance or greatly improve processing times.</em></td></tr><tr><td><strong>Cache Cluster</strong></td><td>Cluster are expensive to rebuild from attribute readouts — PCGEx can cache "pre-built" clusters so any cluster processor down the stream doesn't start from scratch.<br><br><strong>While it speeds things up significantly, it requires a lot more memory, and the cached clusters are only released when the edge data is garbage collected.</strong></td></tr><tr><td><strong>Default Build &#x26; Cache Cluster</strong></td><td>The default value for Cluster nodes regarding building &#x26; caching clusters. This can be changed on a per-node basis. </td></tr></tbody></table>

{% hint style="info" %}
Nodes that do very heavy processing & math usually override these values internally to stay on the safe side (read : slower side) — especially when it comes to intersections, recursive methods etc.
{% endhint %}

### **Points**

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Small Points Size</strong></td><td>What number of point should be considered a "small" number. <br>Small quantities are batched together of being chunked into smaller parallel groups.</td></tr><tr><td><strong>Points Default Batch Chunk Size</strong></td><td>The default chunk size when splitting input points into chunks for parallel processing.<br><br><em>Impact is usually more measurable when <code>Scoped Attribute Get</code> is enabled; as initial attribute read is often the worst offender.</em></td></tr></tbody></table>

### **Async**

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Default Work Priority</strong></td><td>The default Async work priority for all PCGEx nodes.<br><em>Can be changed on a per-node basis.</em></td></tr></tbody></table>

## **Debug**

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Assert on Empty Thread</strong></td><td><mark style="background-color:red;">Only enable that if you have a debugger attached to the editor.</mark><br><br>Enabling this option calls a check in low level code when a point processor attempts to start a batch of async tasks and instead schedule nothing, which cause the culprit node to stall graph execution forever. <br><br><strong>The check makes it very easy to know which node decided to fail, which needs to be fixed on my end.</strong> </td></tr></tbody></table>

{% hint style="info" %}
Hanging tasks bugs are very difficult to track, and what cause them is always an edge case I have overlooked. They're often highly setup dependent, and **if that happens, I would be infinitely grateful if you could send me a graph to repro the issue on my end.**
{% endhint %}

## **Collections**

<table><thead><tr><th width="210">Setting</th><th>Description</th></tr></thead><tbody><tr><td><strong>Disable Collision by Default</strong></td><td>Whether to force-disable collision on meshes when they're added to a <a href="node-library/assets-management/collections/mesh-collection.md">Mesh Collection</a> for the first time. <br><br><em>This is often desired, so checking that saves you from expanding the descriptor and changing the collision settings.</em></td></tr></tbody></table>

## **Blending**

## **Node Colors**

