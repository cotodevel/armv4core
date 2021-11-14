![ToolchainGenericDS](img/TGDS-Logo.png)

TGDS1.65: branch for stable features.

This is the Toolchain Generic DS armv4core project:

Compile Toolchain: To compile this project you will need to follow the steps at https://bitbucket.org/Coto88/toolchaingenericds : Then simply extract the project somewhere.

Compile this project: Open msys, through msys commands head to the directory your extracted this project. Then write: make clean make

After compiling, run the example in NDS.

Project Specific description:

Story:
Back then, near the end of 2014, I begun writing an ARMv4 core to understand better hardware (not only GBA!) overall. From this, core 1 emerged. It did absolutely nothing and most opcodes were broken.
Then in 2016, I wanted to debug GBA Stuff from the NDS hardware, so I ported old core 1, and core 2 emerged. This core 2 reflects the most recent working opcode (currentOpcodeImplemented.txt) and most opcodes are implemented. Some others are bugged.
At some point I took core 2 and created gbaarmhook (back in 2015), but I had two code bases to add armv4core stuff! It'd make no sense. Thus when I began creating the new Toolchain (TGDS).
I used gbaarmhook as base test, but since ToolchainGenericDS back then really sucked, I had lots of segfaults and I wasn't sure either side these were coming from (toolchain or gbaarmhook). 
Thus, the project began to bitrot. After that, I decided to look around old sources having the most recent armv4 implementation, only to find out, gbaarmhook had the latest one! 

The problem: Between core 1 and core 2 there was different "GBA" systems. This would cause all kind of errors of which I am sure I didn't have back then.
Merging these was the way to go, but couldn't because of the toolchain. Thus I began improving the toolchain so all critical runtime errors would be left out. And now, as of 2020, the work is done.
Now I merged both cores (1 and 2) + brought the newest changes from gbaarmhook, and the armv4core behaves exactly as the older core (core2) which was executing gba routines and drawing basic homebrew.
There are still some segfaults here and there, because of the new environment. Anyway... here's hoping I can implement the full ARMv4t set.

/release folder has the latest binary precompiled for your convenience.

Latest stable release:
https://bitbucket.org/Coto88/armv4core/get/TGDS1.65.zip

Coto


