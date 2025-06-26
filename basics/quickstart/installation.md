---
description: How to install PCGEx in your project
icon: power-off
---

# Installation

### FAB Plugin

The easiest way to install PCGEx is to get it from FAB. (it's free!)

<a href="https://www.fab.com/listings/3f0bea1c-7406-4441-951b-8b2ca155f624" class="button primary">PCGEx on FAB</a>

<figure><img src="../../.gitbook/assets/EpicMarketplace-Splash-1080 (2).png" alt=""><figcaption></figcaption></figure>



{% stepper %}
{% step %}
#### Go to the FAB website

Add PCGEx to your library — select whichever license matches your usecase.
{% endstep %}

{% step %}
#### Go to the Epic Games Launcher

Navigate to the Library tab, and scroll all the way down. If PCGEx isn't showing there already, make sure to refresh using the <img src="../../.gitbook/assets/image (5) (1) (1).png" alt="" data-size="line"> refresh icon.
{% endstep %}

{% step %}
#### Install to your preferred engine version

You should now see PCGEx and be able to install the plugin for the editor version of your choice!\
![](<../../.gitbook/assets/image (6) (1) (1).png>)
{% endstep %}
{% endstepper %}

{% hint style="info" %}
The FAB release usually closely matches the latest on Git.\
&#xNAN;_&#x42;ugfixes are pushed on FAB as soon as they are pushed to Git, new features/nodes can sometime be a bit behind._
{% endhint %}

***

### Build From Source

<a href="https://github.com/Nebukam/PCGExtendedToolkit" class="button primary">GitHub repo</a>

{% hint style="warning" %}
In order to build from source, **you will need the C++ build stack installed on your machine**. Usually, the Unreal installer does a good job at installing all the pre-requisites for that, but it's not always the case.
{% endhint %}

#### Clone & Build using Git

The best way is to clone the repository to a submodule (if you're using git); that way makes it easy to get the latest pushes!

```bash
> cd YourProject
> git submodule add https://github.com/Nebukam/PCGExtendedToolkit Plugins/PCGExtendedToolkit
> git add .gitmodules
> git commit
```

{% hint style="info" %}
_Note that the .gitmodules file is located at the root of the git repo of your project, which may not be the root of the project itself depending on your setup._
{% endhint %}

***

### Install & build from the Source' ZIP

{% stepper %}
{% step %}
#### Download the ZIP

<a href="https://github.com/Nebukam/PCGExtendedToolkit/zipball/main" class="button primary">Download from GitHub</a>
{% endstep %}

{% step %}
#### Go to your Project folder

If there is no `Plugins` folder, go on and create one
{% endstep %}

{% step %}
#### Paste the content of the ZIP into Plugins/

You should now have a `YourProject/Plugins/PCGExtendedToolkit` folder.
{% endstep %}

{% step %}
#### Launch project and follow the popups

You will be prompted that there is now uncompiled plugins that requires to be built — this is normal! Click "Build" and _wait_.

{% hint style="success" %}
Building PCGEx for the first time can take quite some time depending on your hardware (5-20min!), so be patient if nothing seems to be happening.
{% endhint %}
{% endstep %}
{% endstepper %}



***

### Finding Nodes

Once the plugin is installed and compiled, nodes are available in any PCG Graph along with vanilla nodes. You can either find them in the explorer on the left, or in the list when right-clicking any empty space in the graph.

{% hint style="info" %}
All PCGEx nodes are prefixed with `PCGEx |` – which is a bit annoying at first, but quickly comes in handy to ensure nodes are clearly identifiable.
{% endhint %}

<figure><img src="../../.gitbook/assets/image (7).png" alt=""><figcaption></figcaption></figure>
