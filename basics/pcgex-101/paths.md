---
icon: ellipsis
---

# Paths

Path is just yet another fancy name for more points.

Unlike clusters, they don't even have special data stored in attributes. In the eyes of a PCGEx node that asks for "paths", any points are valid. Why then? Well, **"paths" in the context of PCGEx represents&#x20;**_**assumptions**_**, rather than actual data.**

<figure><img src="../../.gitbook/assets/placeholder-wide.jpg" alt=""><figcaption></figcaption></figure>

{% stepper %}
{% step %}
#### Point order matters

More often than not, the spatial information stored in a point is what's important — its position, rotation, scale, etc. When dealing with paths, **the order of point in the dataset is equally as important :** each point is considered to be connected to the next one in order.


{% endstep %}

{% step %}
#### Points as segments

In a path, individual points are processed as _segments_, as if there was a line drawn between each point, in order.

A segment "data" is read from the starting point of the segment.


{% endstep %}

{% step %}
#### Open or closed loop

Like splines, paths can be processed as open-ended or closed loop.\
When closed, the last point is considered to be a segment that wraps toward the first point.
{% endstep %}
{% endstepper %}

{% hint style="info" %}
The output of a Spline Sampler is actually a valid paths in PCGEx terms!
{% endhint %}

### Why would you need paths?

Being able to work making the above assumptions enables new way of interacting with points as _paths_, on top of their existing spatial properties. **Also, they're naturally laid out to make great splines right off the bat.** And you know what eat splines for breakfast? _**PCG Grammar!**_

### **Dive into paths**

Paths are also a great way to create clusters, as well as being created from them with tools like pathfinding. You can read more about it in the [Working with Paths](../../working-with-pcgex/paths/) section.

{% content-ref url="../../working-with-pcgex/paths/" %}
[paths](../../working-with-pcgex/paths/)
{% endcontent-ref %}

