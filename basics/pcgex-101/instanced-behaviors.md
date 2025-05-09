---
icon: sliders
---

# Instanced Behaviors

Much like [sub-nodes](sub-nodes/) and the vanilla plugin, PCG make use of instanced behaviors in a few places.&#x20;

These are logical "pieces" that you will be required to select from a list in order for the a given node to be fully functional. This pattern is used when **only a single behavior can be handled by a node at any given time**; as opposed to the more flexible, additive nature of the sub-nodes.

<figure><img src="../../.gitbook/assets/image (16).png" alt=""><figcaption></figcaption></figure>

This is very handy to avoid node bloating _(\*cough\*)_ however it comes with a few downsides:

* Instanced behavior cannot be dynamically overridden.
* Instanced behaviors parameters cannot be _easily_ overridden.
* It requires a few extra clicks to access their exposed settings, if any.

### Overriding Instance Behavior Parameters

Notice how "cannot be _easily_ overridden" is italicized — it's a bit tedious but it is entirely possible to override any exposed setting.

{% hint style="info" %}
Overridable settings have a unique override pin that's hidden by default.\
That pin accepts any number of attribute set that will be inspected during execution in order to search & find for any attribute which type matches the target property.
{% endhint %}

{% stepper %}
{% step %}
#### Expand the node to reveal overrides

If the behavior supports it, a special override pin will be visible. That pin accepts multiple inputs.
{% endstep %}

{% step %}
#### Find the internal property name

Expand the settings and right-click on the property you want to override. Select "Copy Internal Name" — the instanced behavior will look for attributes with that exact name.
{% endstep %}

{% step %}
#### Name that attribute & plug it in!

That's it. It's limiting and tedious because it forces you to use a specific name in the attribute set, _I don't like that, but that's a workaround for the time being._


{% endstep %}
{% endstepper %}

