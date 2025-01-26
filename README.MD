![ToolchainGenericDS](img/TGDS-Logo.png)

TGDS1.65: branch for stable features.

This is the Toolchain Generic DS armv4core project:

Compile Toolchain: To compile this project you will need to follow the steps at https://bitbucket.org/Coto88/toolchaingenericds : Then simply extract the project somewhere.

Compile this project: Open msys, through msys commands head to the directory your extracted this project. Then write: make clean make

After compiling, run the example in NDS.

Project Specific description:

Story:
Back then, near the end of 2014, I began writing an ARMv4 core to understand better hardware architectures (not only GBA!). From this, core 1 emerged. It did absolutely nothing and most opcodes were broken.
Moving forwards, 2016, I was looking to debug GBA Stuff from the NDS hardware, and core 2 emerged. This core 2 reflects the most recent working opcode (currentOpcodeImplemented.txt) and most opcodes are implemented. Some others are bugged.
At some point I took core 2 and created gbaarmhook (back in 2015), but I had two different codebases, and needed to merge them back into armv4core repo. TGDS was being written at that time, and gbaarmhook was used as base project for it, 
but since ToolchainGenericDS really sucked back then, I had lots of segfaults and wasn't sure either which one was the one to blame (toolchain or gbaarmhook).
The project began to bitrot. After that, I decided to look around old sources having the most recent armv4 implementation, gbaarmhook had remnants of the latest one. But bits got lost in the way because I had no idea git existed.  

The problem: Between core 1 and core 2 there was different "GBA" systems. This would cause all kind of errors of which I am sure I didn't have back then.
Merging these was the way to go, but couldn't because of the toolchain. Thus I began improving the toolchain so all critical runtime errors would be left out. And now, as of 2025, the work is done.
Now I merged both cores (1 and 2) + brought the newest changes from gbaarmhook, and the armv4core behaves exactly as the older core (core2) which was executing gba routines and drawing basic homebrew.
There are still some segfaults here and there. Anyway... hopefully the full ARMv4t set can be implemented.


____Remoteboot____
Also, it's recommended to use the remoteboot feature. It allows to send the current TGDS Project over wifi removing the necessity
to take out the SD card repeteadly and thus, causing it to wear out and to break the SD slot of your unit.

Usage:
- Make sure the wifi settings in the NintendoDS are properly set up, so you're already able to connect to internet from it.

- Get a copy of ToolchainGenericDS-multiboot: https://bitbucket.org/Coto88/ToolchainGenericDS-multiboot/get/TGDS1.65.zip
Follow the instructions there and get either the TWL or NTR version. Make sure you update the computer IP address used to build TGDS Projects, 
in the file: toolchaingenericds-multiboot-config.txt of said repository before moving it into SD card.

For example if you're running NTR mode (say, a DS Lite), you'll need ToolchainGenericDS-multiboot.nds, tgds_multiboot_payload_ntr.bin
and toolchaingenericds-multiboot-config.txt (update here, the computer's IP you use to build TGDS Projects) then move all of them to root SD card directory.

- Build the TGDS Project as you'd normally would, and run these commands from the shell.
<make clean>
<make>

- Then if you're on NTR mode:
<remoteboot ntr_mode computer_ip_address>

- Or if you're on TWL mode:
<remoteboot twl_mode computer_ip_address>

- And finally boot ToolchainGenericDS-multiboot, and press (X), wait a few seconds and TGDS Project should boot remotely.
  After that, everytime you want to remoteboot a TGDS Project, repeat the last 2 steps. ;-)



/release folder has the latest binary precompiled for your convenience.

Latest stable release:
https://bitbucket.org/Coto88/armv4core/get/TGDS1.65.zip

Coto


