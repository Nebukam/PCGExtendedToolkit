---
layout: page
title: ∷ Installation
subtitle: How to install
summary: The best way is to clone the repo as a submodule; though you can also download pre-packaged versions.
splash: icons/icon_directory-download.svg
preview_img: placeholder.jpg
has_children: true
nav_order: 1
hidden_link: '{% include link_btn title="PCGEx on **FAB**" color="white" link="https://www.fab.com/listings/a5061972-6875-4052-a7fd-7c33f6531ec3" icon="load-arrow" %}'
---

{% include header_card %}

## FAB Plugin

The easiest way to install PCGEx is to get it from FAB. (it's free!)   

{% include link_btn title="PCGEx on FAB" color="red" link="https://www.fab.com/listings/3f0bea1c-7406-4441-951b-8b2ca155f624" icon="left" %}

{% include img a='EpicMarketplace-Splash-1080.png' %} 

> The FAB release matches the `FAB-` branches on github, due to the submission & validation process they will always be out of date compared to github' latest.

---

## Build from Source
   
{% include link_btn title="Github" color="red" link="https://github.com/Nebukam/PCGExtendedToolkit" icon="load-arrow" %}

> If building from source, make sure your project & computer is set-up for C++ dev. [See Epic Documentation on the topic](https://docs.unrealengine.com/4.26/en-US/ProductionPipelines/DevelopmentSetup/VisualStudioSetup/).
{: .comment }

---

## Cloning & Build using Git
   
The best way is to clone the repository to a submodule; that way you can contribute pull requests if you want.

```console
> cd YourProject
> git submodule add https://github.com/Nebukam/PCGExtendedToolkit Plugins/PCGExtendedToolkit
> git add ../.gitmodules
> git commit
```

---

## Install from the Source' ZIP 
   
{% include link_btn title="Download from Github (.zip)" color="white" link="https://github.com/Nebukam/PCGExtendedToolkit/zipball/main" icon="load-arrow" %}

Alternatively you can download the ZIP of this repo and place it in `YourProject/Plugins/PCGExtendedToolkit`

---

## Finding Nodes

Once the plugin is installed and compiled, nodes are available in any PCG Graph along with vanilla nodes. You can either find them in the explorer on the left, or in the list when right-clicking any empty space in the graph.
{: .fs-5 .fw-400 }

> All PCGEx nodes are prefixed with `PCGEx |` -- which is a bit annoying at first, but quickly comes in handy to ensure nodes are clearly identifiable.
{: .infos-hl }

{% include img a='installation/finding-nodes.png' %}