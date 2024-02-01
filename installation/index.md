---
layout: page
title: Installation
subtitle: How to install
summary: The best way is to clone the repo as a submodule; though you can also download sources as a .zip to ensure long term stability.
splash: icons/icon_directory-download.svg
preview_img: placeholder.jpg
has_children: true
nav_order: 1
---

{% include header_card %}
  
## Sources  
   
{% include link_btn title="Latest (.zip)" color="red" link="https://github.com/Nebukam/PCGExtendedToolkit/zipball/main" icon="load-arrow" %}
{% include link_btn title="Github" color="white" link="https://github.com/Nebukam/PCGExtendedToolkit" icon="load-arrow" %}

---

## Cloning
   
The best way is to clone the repository to a submodule; that way you can contribute pull requests if you want. The project should placed in your project's Plugins folder.

```console
> cd YourProject
> git submodule add https://github.com/Nebukam/PCGExtendedToolkit Plugins/PCGExtendedToolkit
> git add ../.gitmodules
> git commit
```

---

## Install from the ZIP 
   
Alternatively you can download the ZIP of this repo and place it in `YourProject/Plugins/PCGExtendedToolkit`

---
