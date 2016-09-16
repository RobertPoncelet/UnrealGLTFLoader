# Unreal Engine glTF Loader Plugin

Overview
------
This plugin makes use of Tiny glTF Loader [(link)](https://github.com/syoyo/tinygltfloader) to allow the user to import glTF files as static meshes. It has been tested with Unreal Engine version 4.12.3.

Installation and Usage
------
To install, simply copy the repository folder to "Plugins" in your project root. You may need to recompile your project if your engine version differs from 4.12.3.

To use, firstly enable the plugin in your settings if you don't see the glTF button in the top toolbar. Clicking it will open the plugin window, where you can modify transformation settings applied to the mesh before importing. The "Import File" button here will open a browser to import a glTF file as a static mesh to the current folder in the content browser.

[Video Link](https://vimeo.com/182935578)
