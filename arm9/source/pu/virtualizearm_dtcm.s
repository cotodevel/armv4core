@------------------------ DTCM Code -------------------
@----------------------------------------------------------------------------------------------------------------------------
@ DTCM variables / DTCM IS NOT MEANT TO STORE ASM opcodes because caches must be coherent for that! Otherwise CPU will crash
@--------------------------------------------------------------------------------
.section	.dtcm,"ax",%progbits
@--------------------------------------------------------------------------------

.global rom						@GBA virtual Program Counter, Instruction Pointer
rom:
	.word 0x00000000
	
.global romsize					@no need to allocate to dtcm , its already on dtcm
romsize:
	.word 0x00000000

.global rom_entrypoint			@gba entrypoint on header
rom_entrypoint:
	.word 0x00000000

.global cputotalticks			@emulator CPU heartbeats DTCM
cputotalticks:
	.word 0x00000000

.global hblankdelta				@GBA HBLANK counter
hblankdelta:
	.word 0x00000000

.global nds_vcount				@NDS Vertical counter
nds_vcount:
	.word 0x00000000
	
