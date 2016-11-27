@--------------------------------------------------------------------------------
@								virtualizer variables
@--------------------------------------------------------------------------------
.cpu arm946e-s
.align 4
.code 32
.ARM
@--------------------------------ARM virtualize----------------------------------

#include "../settings.h"

.global romsize					@no need to allocate to dtcm , its already on dtcm
romsize:
	.word 0x00000000


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
	rscs r0,r0,r1						
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
	rsbs r0,r0,r1						
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr


@ARM v5 multiplier opcodes

.global mulasm	
.type mulasm STT_FUNC
mulasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	muls r3,r0,r1				@Rd:=Rm(0)*Rs(1). Rn is ignored
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
    mov r0,r3   @arg0 output
    
	LDMFD sp!, {r1-r12,lr}
bx lr


.global mlaasm	
.type mlaasm STT_FUNC
mlaasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
	mlas r3,r0,r1,r2			@Rd:=Rm(0)*Rs(1)+Rn(2)
	@save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r4,=cpsrasm
	str r6,[r4]
	
    mov r0,r3   @arg0 output
    
	LDMFD sp!, {r1-r12,lr}
bx lr

@signed multiply opcodes

.global umullasm	
.type umullasm STT_FUNC
umullasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
    mov r4,r0   @Rdlo 
    mov r5,r1   @Rdhi
    
	umulls r0,r1,r2,r3				@RdLo,RdHi,Rm,Rs / UMULL R1,R4,R2,R3 ; R4,R1:=R2*R3
	
    str r0,[r4]
    str r1,[r5]
    
    @save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global smullasm	
.type smullasm STT_FUNC
smullasm:
	STMFD sp!, {r1-r12,lr}		
	
	@opcode
    mov r4,r0   @Rdlo 
    mov r5,r1   @Rdhi
    
	smulls r0,r1,r2,r3				@RdLo,RdHi,Rm,Rs / UMULL R1,R4,R2,R3 ; R4,R1:=R2*R3
	
    str r0,[r4]
    str r1,[r5]
    
    @save asmCPSR results
	mrs r6,CPSR		@APSR
	ldr r2,=cpsrasm
	str r6,[r2]
	@opcode end
	
	LDMFD sp!, {r1-r12,lr}
bx lr

.global umlalasm	
.type umlalasm STT_FUNC
umlalasm:
	STMFD sp!, {r1-r12,lr}		
	
    umlals r5,r4,r2,r3				@ RdLo_addr(0), RdHi_addr(1) = Rm(2) * Rs(3) + RdLo(4), RdHi(5) / RdHi,RdLo := Rm * Rs + RdHi,RdLo
	@save asmCPSR results
	mrs r7,CPSR		@APSR
	ldr r8,=cpsrasm
	str r7,[r8]
	@opcode end
	
    str r4,[r0]             @ RdLo_addr(0)
    str r5,[r1]             @ RdHi_addr(1)
    
	LDMFD sp!, {r1-r12,lr}
bx lr

.global smlalasm	
.type smlalasm STT_FUNC
smlalasm:
	STMFD sp!, {r1-r12,lr}		
	
	smlals r5,r4,r2,r3				@ RdLo_addr(0), RdHi_addr(1) = Rm(2) * Rs(3) + RdLo(4), RdHi(5) / RdHi,RdLo := Rm * Rs + RdHi,RdLo
	@save asmCPSR results
	mrs r7,CPSR		@APSR
	ldr r8,=cpsrasm
	str r7,[r8]
	@opcode end
	
    str r4,[r0]             @ RdLo_addr(0)
    str r5,[r1]             @ RdHi_addr(1)
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

@coto
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

.pool       @we need the neccesary #imm fetches close

@--------------------------------------------------------------------------------------------------------------
@EWRAM variables
@---------------------------------------------------------------------------------------------------------------

@--------------------------------------------C IRQ handler word aligned pointer	
.global gbachunk
gbachunk:
	.word 0x00000000

@--------------------------------- emulator interruptable registers / variables
@.global cputotalticks	@emulator CPU heartbeats / moved to DTCM
@cputotalticks:
@	.word 0x00000000

@-- destroyable regs (for virtual stack opcodes)
.global dummyreg
dummyreg:
	.word 0x00000000
	
.global dummyreg2
dummyreg2:
	.word 0x00000000

.global dummyreg3
dummyreg3:
	.word 0x00000000

@so far here

.global dummyreg4
dummyreg4:
	.word 0x00000000

.global dummyreg5
dummyreg5:
	.word 0x00000000
	
@---------------------- CPU CPSR / SPSRs -------------------------
@toggle that decides if an ARM opcode is #Imm
@virtualizated cpsr (for reusing flags on whatever processor support ARM flags)
.global cpsrasm			@CPSR from hardware reads
cpsrasm:
	.word 0x00000000
@emulated cpsr
.global cpsrvirt		@CPSR from virtualized environment
cpsrvirt:
	.word 0x00000000

@spsr for each ARMv4 PSR_MODE
.global spsr_svc		@ saved, SVC(32) mode
spsr_svc: 
	.word 0x00000000

.global spsr_irq		@ saved, IRQ(32) mode
spsr_irq:
	.word 0x00000000

.global spsr_abt		@ saved, ABT(32) mode
spsr_abt:
	.word 0x00000000
	
.global spsr_und		@ saved, UND(32) mode
spsr_und:
	.word 0x00000000
	
.global spsr_fiq		@ saved, FIQ(32) mode
spsr_fiq:
	.word 0x00000000

.global spsr_usr		@ saved, USR(32) mode
spsr_usr:
	.word 0x00000000

.global spsr_sys		@ saved, SYS(32) mode
spsr_sys:
	.word 0x00000000
	
.global spsr_last		@ saved, latest SPSR cpu<mode>
spsr_last:
	.word 0x00000000
	
@---------------------------virtualizer variables------------------------------------------


.global gba_entrypoint
gba_entrypoint:
	.word 0x00000000


@The ARM9 Exception Vectors are located at FFFF0000h. The IRQ handler redirects to [DTCM+3FFCh].
@= ADDRESS of label (phys location), dereference address and you get offset content

.global vector_addr
vector_addr:
	.word __vectors_start

.global vector_end_addr
vector_end_addr:
	.word __vectors_end

@useful macro to retrieve DTCMs top reserved area
.global dtcm_end_alloced
dtcm_end_alloced:
	.word __dtcm_end


@DEBUG and dissect ARM code
.cpu arm7tdmi

.global opcode_test_arm
.type opcode_test_arm STT_FUNC
opcode_test_arm:

pop {r0,r9,r10,r11,r12,r15}

pop {r0,r9,r10,r11,r12,r15}^

bx lr



@other
@movs r15,r15 @spsr restore ARMV4+

.align
.pool
.end