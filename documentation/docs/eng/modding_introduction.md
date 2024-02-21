---
sidebar_position: 2
description: The purpose of this article is to explain what Half-Life 1 modding is like in the 202x years, and where to even start diving into this topic. This article is more theoretical than practical.
---

# Introduction to modern Half-Life 1 modding
Despite the fact that almost 30 years have passed since the release of this game, the community continues to this day be interested in the game and use it as a tool for their creativity. Maps, models, sounds, sprites, textures, and even full-fledged completed projects that include everything listed earlier: an uncountable number of gigabytes of content have been created by the community, and this number is growing every year.
  
## The most important event in modding history
Perhaps the most important event in Half-Life 1 modding was the release of the Xash3D engine. It is almost completely compatible with the original Half-Life 1 engine, called GoldSrc. But Xash3D has a number of very important differences: it has completely open source, many internal limits have been expanded or removed entirely, a lot of new functionality has been added for modding, and it also supports many different platforms, such as Nintendo Switch, PS Vita, Android, and many more. It was even run on smartwatches, and in the browser (using Emscripten), as well as in DOS.

This engine has opened up unprecedented possibilities for players and mod developers (you probably already saw the news this year, that they [added real ray tracing to HL1](https://www.pcgamer.com/ray-traced-half-life-mod-is-finally-here-and-looks-incredible/)). Initially, the engine was developed by one person well known in narrow circles under the nickname *g-cont* or *Uncle Mike*. But in 2019, he stopped engine development and made many of his projects publicly available, and switched to his another unrelated project. At the moment, the engine is maintained and developed by a team of enthusiasts called [FWGS](https://github.com/FWGS), but sometimes other contributors also take part in the development, such as *Ivan "provod/w23" Avdeev*, who has been developing a Vulkan renderer with ray tracing support since the beginning of 2021 and streaming all process on his [Twitch](https://www.twitch.tv/provod).

Also, in 2016, the FWGS team released a port of the Xash3D engine for the Android platform, and at the same time ports of the original Half-Life 1 and Counter Strike 1.6 were also made. This event caused an incredible wave of interest from the gaming community. As a result of these events, [Xash3D FWGS port](https://play.google.com/store/apps/details?id=in.celest.xash3d.hl) received over a million installations and 30+ thousand reviews on Google Play. [CS16Client](https://play.google.com/store/apps/details?id=in.celest.xash3d.cs16client) has accumulated over a million installations and 20+ thousand reviews on Google Play. Not that bad for a game almost 30 years old, right? In addition to HL1 and CS 1.6, many other mods were also ported: Opposing Force, They Hunger, Afraid of Monsters: Director's Cut, Poke646, etc.

## Basic modding (replacing or adding content)
Let's pretend that you came up with an idea for a game based on the GoldSrc/Xash3D engine to replace sounds, models, or textures. This is done very simply: you just find the file you need in the game folder and replace it with the desired one. You can either make it yourself or find something suitable on sites such as [ModDB](https://www.moddb.com/games/half-life/downloads) or [GameBanana](https://gamebanana.com/games/34). As for maps (or locations/levels, as they are also called), everything is more complicated: a compiled map can be changed in a rather limited ways; for this purpose the [newbspguy](https://github.com/UnrealKaraulov/newbspguy) program can be used. But even despite limits of editing compiled map, more likely you'll want to change something minor, and this software can do it.

If you want to create your own game locations/maps completely from scratch, you will need to understand the editor and map compiling tools. Separate, detailed articles will be made on this topic later. Working with models also requires a separate article that will cover all aspects and tools necessary for work (by the way, everything is free and open source).

## Advanced modding
If you want to make your own project based on HL1, without limiting yourself to just replacing content, then you will have to deal with the Half-Life SDK. HLSDK is a set of source codes for utilities, client and server HL1 libraries. All this is written in C++, so if you are already familiar with this language, it will be much easier for you to get started. HLSDK became publicly available shortly after the release of HL1, that is, around 1999. People took HLSDK as a basis, implemented their ideas, and made the modified sources available to the public. As a result, there are many different variations of HLSDK with different additional capabilities. But in this article I will describe only the most actual options at this time, which makes sense to use in your projects:

### hlsdk-portable
It is a regular HLSDK without additional features and gameplay changes relative to the standard HL, but it is ported to many platforms, and also contains a considerable number of various bugfixes that are not in the original HLSDK. Developed by the FWGS team. A good option if you don’t need to go beyond the capabilities of HL1, and you, for example, just want to somehow change the gameplay or something else. Can be used to create a mod for both GoldSrc and Xash3D FWGS. Can be used for both single-player and multiplayer mods.

### PrimeXT
A modern version of HLSDK for the Xash3D FWGS engine, ported to many modern platforms, has enhanced graphics and physics and a lot of new features for mod-makers, while retaining all the features and approaches to work inherent from GoldSrc and Xash3D. It is based on XashXT, so it inherits all the functionality from XashXT and Spirit Of Half-Life. Suitable for creating both single-player and multiplayer mods. In addition, it contains a huge variety of new features and tools for creating mods, you can read more about this on the [project website](https://snmetamorph.github.io/PrimeXT/docs/eng/intro), since detailed description is not within the scope of this article. If you want to go far beyond HL1 technologically and "squeeze all the juice" out of the engine, PrimeXT was created for exactly this.

## Other information sources
While we are working on our own articles, we highly recommend paying attention to existing sources of knowledge. It is likely that among them you will find the answer to your question.

- [Tutorials by The303](https://the303.org/tutorials/)
- [Tutorials on GameBanana](https://gamebanana.com/tuts/games/34?_nPage=4)
- [Tutorials on ModDB](https://www.moddb.com/tutorials?filter=t&kw=&subtype=&meta=&game=1)
- [Tutorials on Sourcemodding](https://www.sourcemodding.com/tutorials/goldsrc/)
- [HLSDK tutorials by Admer456](https://www.youtube.com/playlist?list=PLZmAT317GNn19tjUoC9dlT8nv4f8GHcjy)
- [TWHL Wiki](https://twhl.info/wiki/index)
