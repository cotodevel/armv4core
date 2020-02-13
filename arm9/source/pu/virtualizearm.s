@--------------------------------------------------------------------------------
@								virtualizer variables
@--------------------------------------------------------------------------------
.align 4
.code 32
.ARM
@--------------------------------ARM virtualize----------------------------------

@MOVS PC,R14 @return from exception and restore CPSR from SPSR_mode
@BL put LR=PC then jumps to subroutine
@BX changes thumb bit [1..0] then jumps
@msr CPSR_flg,#0xF0000000		@for modifying CPSR_all flags
@lsl r0,r0, #0	@set C flag

.global lslasm	
.type lslasm STT_FUNC
lslasm:
	STMFD sp!, {r1-r12,lr}		@stacks are working
	
	@opcode
	movs r0,r0, LSL r1			@old: lsls r0,r0,r1	
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global lsrasm	
.type lsrasm STT_FUNC
lsrasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	movs r0,r0, LSR r1			@old:	lsrs r0,r0,r1
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global asrasm	
.type asrasm STT_FUNC
asrasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	movs r0,r0, ASR r1 			@old:	asrs r0,r0,r1
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

@--add/s	@btw manual explicitly says: THUMB: ADD -> ARM counterpart: ADDS

.global addsasm					@#1
.type addsasm STT_FUNC
addsasm:

.global addasm					@#2
.type addasm STT_FUNC
addasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	adds r0,r0,r1
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

@--sub/s	@btw manual explicitly says: THUMB: SUB -> ARM counterpart: SUBS

.global subsasm					@#1
.type subsasm STT_FUNC
subsasm:

.global subasm					@#2
.type subasm STT_FUNC
subasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	subs r0,r0,r1
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global movasm					
.type movasm STT_FUNC
movasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	movs r0,r0					@yeah but for hw CPSR
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global cmpasm					
.type cmpasm STT_FUNC
cmpasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	cmp r0,r1					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global andasm					
.type andasm STT_FUNC
andasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ands r0,r0,r1					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global eorasm						@xor					
.type eorasm STT_FUNC
eorasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	eors r0,r0,r1					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global adcasm	
.type adcasm STT_FUNC
adcasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	adcs r0,r0,r1 			
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global sbcasm	
.type sbcasm STT_FUNC
sbcasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	sbcs r0,r0,r1 			
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global rorasm	
.type rorasm STT_FUNC
rorasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	movs r0,r0, ror r1 		
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global tstasm	
.type tstasm STT_FUNC
tstasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	tst r0,r1				
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global negasm	
.type negasm STT_FUNC
negasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	rsbs r0,r0,#0				
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global cmnasm	
.type cmnasm STT_FUNC
cmnasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	cmn r0,r1				
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global orrasm	
.type orrasm STT_FUNC
orrasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	orrs r0,r0,r1				
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global mulasm	
.type mulasm STT_FUNC
mulasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	mul r0,r1,r0				@rd = mul rd, rs, but compiler disaproves mul rd,rd,rs	
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global bicasm	
.type bicasm STT_FUNC
bicasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	bics r0,r0,r1				
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global mvnasm	
.type mvnasm STT_FUNC
mvnasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	mvns r0,r0				
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global ldsbasm	
.type ldsbasm STT_FUNC
ldsbasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldrsb r0,[r0,r1]						
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global ldshasm	
.type ldshasm STT_FUNC
ldshasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldrsh r0,[r0,r1]						
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global rscasm	
.type rscasm STT_FUNC
rscasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	rsc r0,r0,r1						
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global teqasm	
.type teqasm STT_FUNC
teqasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	teq r0,r1						
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global rsbasm	
.type rsbasm STT_FUNC
rsbasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	rsb r0,r0,r1						
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

@mults

.global mlaasm	
.type mlaasm STT_FUNC
mlaasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	mla r0,r1,r0,r2				@rd = mul rd, rs, but compiler disaproves mul rd,rd,rs	
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global mulsasm	
.type mulsasm STT_FUNC
mulsasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	muls r0,r1,r0				@rd = mul rd, rs, but compiler disaproves mul rd,rd,rs	
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global mulltasm	
.type mulltasm STT_FUNC
mulltasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	mullt r0,r1,r0				@rd = mul rd, rs, but compiler disaproves mul rd,rd,rs	
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global mlavcsasm	
.type mlavcsasm STT_FUNC
mlavcsasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	mlavcs r0,r1,r0,r2				@rd = mul rd, rs, but compiler disaproves mul rd,rd,rs	
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

@load/store
.global ldru32extasm	
.type ldru32extasm STT_FUNC
ldru32extasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldr r0,[r0,r1]					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global ldru16extasm	
.type ldru16extasm STT_FUNC
ldru16extasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldrh r0,[r0,r1]					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global ldru8extasm	
.type ldru8extasm STT_FUNC
ldru8extasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	ldrb r0,[r0,r1]					
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr


@taken from: http://www.finesse.demon.co.uk/steven/sqrt.html
@ (r0)IN :  n 32 bit unsigned integer
@ (r5)OUT:  root = INT (SQRT (n))
@ (r4)TMP:  offset
@ (r7) i: 

.global sqrtasm
.type sqrtasm STT_FUNC
sqrtasm:
	STMFD sp!, {r1-r12,lr}		
	mov r2,r0	@IN
	
	mov r7,#0x0	@i
	mov r8,#2	@imm #2
	mov r4, #3 << 30
	mov r5, #1 << 30
	innerloop:
		muls	r9,	r8,	r7
		CMP    	r2, r5, ROR	r9
		SUBHS  	r2, r2, r5, ROR	r9
		ADC    	r5, r4, r5, LSL #1
		cmp 	r7	,#0xf
	addne r7,#1
	bne innerloop
	BIC    r5, r5, #3 << 30  	@ for rounding add: CMP n, root  ADC root, #1
	mov r0,r5
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global shift_word
shift_word:
	movs r6,r5,lsl #16	
	movs r6,r6,lsr #16	@0xabcd
	
	movs r7,r5,ror #16	@0xefgh0000
	movs r7,r7,lsl #16
	
	orr r6,r7,r6		@0xabcdefgh
bx lr

.global wramtstasm
.type wramtstasm STT_FUNC
wramtstasm:
	STMFD sp!, {r2-r12,lr}		
	
	mov r2,#0
	mov r3,#0
	ldr r4,=0xc172c374
	mov r5,#0
	@r0= wram_start / r1= wram_top / r2 = i / r3 = *(i+iwram) / r4 = stub
	@r5 = any
	b0: 
		cmp r2,r1		@i == top
		bne b1
		beq b3

	b1:  
		add r3,r0,r2
		str r4,[r3]
		ldr r3,[r3]		@r3 = *(i+iwram)
		
			//mov r5,r3
			//bl shift_word	@r5 = io
		
			cmp r3,r4 	@and test read
			bne b3
			add r2,#4	@i++
	b b0
		
	b3:
	mov r0,r2
	
	LDMFD sp!, {r2-r12,lr}
bx lr

@---------------------------------------------------------------------------------




@--------------------------------------------------------------------------------------------------------------
@EWRAM variables
@---------------------------------------------------------------------------------------------------------------

@--------------------------------- emulator interruptable registers / variables
	
@-- address patches
.global addrpatches
addrpatches:			@init - end
	.word 0x00000000	@bios
	.word 0x00003FFF	
	.word 0x02000000	@ewram
	.word 0x0203FFFF
	.word 0x03000000	@internal wram
	.word 0x03007FFF
	.word 0x04000000	@GBA I/O map
	.word 0x040003FE
	.word 0x05000000	@BG/OBJ Palette RAM
	.word 0x050003FF
	.word 0x06000000	@vram
	.word 0x06017FFF
	.word 0x07000000	@object attribute memory
	.word 0x070003FF
	.word 0x00000000	@leftover
	.word 0x00000000	@

.global addrfixes
addrfixes:				@init - end
	.word 0x00000000	@bios
	.word 0x00000000	
	.word 0x00000000	@ewram
	.word 0x00000000
	.word 0x00000000	@internal wram
	.word 0x00000000
	.word 0x00000000	@GBA I/O map
	.word 0x00000000
	.word 0x00000000	@BG/OBJ Palette RAM
	.word 0x00000000
	.word 0x00000000	@vram
	.word 0x00000000
	.word 0x00000000	@object attribute memory
	.word 0x00000000
	.word 0x00000000	@rom
	.word 0x00000000	@rom top

@---------------------- CPU flags / CPSR / SPSRs -------------------------

.global z_flag
z_flag:
	.word 0x00000000

.global n_flag
n_flag:
	.word 0x00000000

.global c_flag
c_flag:
	.word 0x00000000

.global v_flag
v_flag:
	.word 0x00000000

.global i_flag
i_flag:
	.word 0x00000000

.global f_flag
f_flag:
	.word 0x00000000
	
.global immop_arm
immop_arm:
	.word 0x00000000
	
.global	setcond_arm
setcond_arm:
	.word 0x00000000
	
.global cpsrasm			@CPSR from hardware reads -> exRegs[0x10] CPSR
cpsrasm:
	.word 0x00000000
	
@---------------------------virtualizer variables------------------------------------------

.global armstate		@0 ARM / 1 THUMB 
armstate:
	.word 00000000

.global armirqstate		@0 disabled / 1 enabled
armirqstate:
	.word 00000000
	
.global armswistate 	@0 disabled / 1 enabled
armswistate:
	.word 00000000

@The ARM9 Exception Vectors are located at FFFF0000h. The IRQ handler redirects to [DTCM+3FFCh].
@= ADDRESS of label (phys location), dereference address and you get offset content


@------------------------for ARM code calls
.global gbastackarm
gbastackarm:
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000
	.word 0x00000000


@----------------------------------------------------------------------------------------------------------------------------
@ DTCM variables / DTCM IS NOT MEANT TO STORE ASM opcodes because caches must be coherent for that! Otherwise CPU will crash
@--------------------------------------------------------------------------------
@.section	.dtcm,"ax",%progbits
@--------------------------------------------------------------------------------

@useful macro to retrieve DTCMs top reserved area
.global dtcm_end_alloced
dtcm_end_alloced:
	.word _dtcm_end

.align
.pool
.end