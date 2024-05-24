---
layout: page
title: ∷ Installation
subtitle: How to install
summary: The best way is to clone the repo as a submodule; though you can also download pre-packaged versions.
splash: icons/icon_directory-download.svg
preview_img: placeholder.jpg
has_children: true
nav_order: 1
---

{% include header_card %}

## Drop-in Package

The easiest way to install PCGExtendedToolkit is to download the packaged version of the plugin.  
However, for the sake of simplicity (and size), a packaged version only exist for the latest *launcher* minor release of the engine, at the time the PCGEx release was published.  

{% include link_btn title="PCGEx for Unreal **5.4** (Package)" color="blue" link="https://github.com/Nebukam/mkfont/releases/latest/download/PCGExtendedToolkit-5.4.zip" icon="load-arrow" %}
{% include link_btn title="PCGEx for Unreal **5.3** (Package)" color="blue" link="https://github.com/Nebukam/mkfont/releases/latest/download/PCGExtendedToolkit-5.3.zip" icon="load-arrow" %}

Simply download one of the .zip above and put `PCGExtendedToolkit` directly in your `YourProject/Plugins/` folder.
{: .fs-5 }

> Note that **these packages are created in windows, for windows** -- you can always compile the plugin yourself from the sources if the latest package doesn't work for your version of the editor.
{: .comment }

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
