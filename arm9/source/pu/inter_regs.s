@--------------------------------------------------------------------------------
@.section	.vectors,"ax",%progbits @if wrong fix this
@--------------------------------------------------------------------------------

.align 4
.code 32
.ARM

@--------------------------------------------------------------------------------
@							EXCEPTION HANDLERS
@--------------------------------------------------------------------------------

@.global _Reset
@_Reset:
@	B 	inter_reset		@ reset 		
@	B 	inter_undef		@ undefined		@bugged code can cause white screens
@	B 	inter_swi 		@ SWI
@	B 	inter_prefdabt 	@ prefetch abort
@	B 	inter_dabt		@ data Abort
@	B 	.			 	@ reserved 
@	B 	inter_irq		@ irq
@	B 	.				@ fiq 

@----------------------------------software interrupt handler--------------------------------------
.global SPswi
SPswi:
.word __sp_svc

.global SPirq
SPirq:
.word __sp_irq

.global SPusr
SPusr:
.word __sp_usr

.global SPdabt
SPdabt:
.word __sp_abort


@---------------------------------------reset code----------------------------------
.global rststack
rststack:
	.word	0x00000000	@r0 	a1
	.word	0x00000000	@r1		a2
	.word	0x00000000	@r2		a3
	.word	0x00000000	@r3		a4
	.word	0x00000000	@r4		s1
	.word	0x00000000	@r5		s2
	.word	0x00000000	@r6		s3
	.word	0x00000000	@r7		s4
	.word	0x00000000	@r8		s5
	.word	0x00000000	@r9		s6
	.word	0x00000000	@r10	sl
	.word	0x00000000	@r11	ip
	.word	0x00000000	@r12	fp
	.word	0x00000000	@r13	sp
	.word	0x00000000	@r14	lr
	.word	0x00000000	@r15	pc

.global inter_reset
inter_reset:

ldr	r5, =rststack
stmia	r5, {r9,r10,r11,r12,r13,lr}
	
ldr r5,=SPdabt
ldr sp,[r5]
	
ldr	r6, =exceptrstC
ldr	r6, [r6]
mov r14,r15
bx	r6

ldr	r5, =rststack	
ldmia	r5, {r9,r10,r11,r12,r13,lr}
subs    pc, lr, #4 //(LR_<mode> depends on what kind of abort exception MPU/MMU throws) subs pc,lr,#0 causes to execute next instruction (data abort) correct 

@adr usage
@start   MOV     r0,#10
@        ADR     r4,start    @ => SUB r4,pc,#0xc

@-------------------------------------undefined code------------------------------------
.global undefstack
undefstack:
	.word	0x00000000	@r0 	a1
	.word	0x00000000	@r1		a2
	.word	0x00000000	@r2		a3
	.word	0x00000000	@r3		a4
	.word	0x00000000	@r4		s1
	.word	0x00000000	@r5		s2
	.word	0x00000000	@r6		s3
	.word	0x00000000	@r7		s4
	.word	0x00000000	@r8		s5
	.word	0x00000000	@r9		s6
	.word	0x00000000	@r10	sl
	.word	0x00000000	@r11	ip
	.word	0x00000000	@r12	fp
	.word	0x00000000	@r13	sp
	.word	0x00000000	@r14	lr
	.word	0x00000000	@r15	pc

.global inter_undef
inter_undef:
	ldr 	r5, =undefstack
	stmia 	r5,{r6-r13,r14}
	
	@PU disable
	mrc	p15,0,r0,c1,c0,0
 	bic	r0,r0,#1
	mcr	p15,0,r0,c1,c0,0
	
	mov r0,lr	@arg0 is LR
	
	@undefined exception vector callback
	ldr	r6, =exceptundC
	ldr	r6, [r6]
	bx	r6
	
	@pu enable
	mrc	p15,0,r0,c1,c0,0
 	orr	r0,r0,#1
	mcr	p15,0,r0,c1,c0,0
	
	ldr r5, =undefstack
	ldmia 	r5,{r6-r13,r14}
	movs    pc, lr, lsl #0					@restore CPSR=SPSR
	
@-----------------------------------------prefecth data abort-----------------------------------------------
.global prefdabtstack
prefdabtstack:
	.word	0x00000000	@r0 	a1
	.word	0x00000000	@r1		a2
	.word	0x00000000	@r2		a3
	.word	0x00000000	@r3		a4
	.word	0x00000000	@r4		s1
	.word	0x00000000	@r5		s2
	.word	0x00000000	@r6		s3
	.word	0x00000000	@r7		s4
	.word	0x00000000	@r8		s5
	.word	0x00000000	@r9		s6
	.word	0x00000000	@r10	sl
	.word	0x00000000	@r11	ip
	.word	0x00000000	@r12	fp
	.word	0x00000000	@r13	sp
	.word	0x00000000	@r14	lr
	.word	0x00000000	@r15	pc

.global inter_prefdabt
inter_prefdabt:
	ldr	r5, =prefdabtstack
	stmia	r5, {r9,r10,r11,r12,r13,lr}
	
	ldr r5,=SPdabt
	ldr sp,[r5]
	
	ldr	r6, =exceptprefabtC
	ldr	r6, [r6]
	mov r14,r15
	bx	r6
	
	ldmia	r5, {r9,r10,r11,r12,r13,lr}
	subs    pc, lr, #4 //(LR_<mode> depends on what kind of abort exception MPU/MMU throws) subs pc,lr,#0 causes to execute next instruction (data abort) correct 
	
@--------------------------------------------data abort code------------------------------------------------
.global dabtstack
dabtstack:
	.word	0x00000000	@r0 	a1
	.word	0x00000000	@r1		a2
	.word	0x00000000	@r2		a3
	.word	0x00000000	@r3		a4
	.word	0x00000000	@r4		s1
	.word	0x00000000	@r5		s2
	.word	0x00000000	@r6		s3
	.word	0x00000000	@r7		s4
	.word	0x00000000	@r8		s5
	.word	0x00000000	@r9		s6
	.word	0x00000000	@r10	sl
	.word	0x00000000	@r11	ip
	.word	0x00000000	@r12	fp
	.word	0x00000000	@r13	sp
	.word	0x00000000	@r14	lr
	.word	0x00000000	@r15	pc

.global inter_dabt
inter_dabt:
	ldr	r5, =dabtstack
	stmia	r5, {r9,r10,r11,r12,r13,lr}
	
	ldr r5,=SPdabt
	ldr sp,[r5]
	
	ldr	r6, =exceptdatabtC	@removed temporarily til lockups go away
	ldr	r6, [r6]
	mov r14,r15
	bx	r6
	
	ldr r5,=SPdabt
	str sp,[r5]
	
	ldr	r5, =dabtstack
	ldr sp,[r5,#(4*4)]
	
	@restore registers
	ldr	r5, =dabtstack
	
	//for two args
	//str r0, [r5,#(4*0)] @store arg0 from callee into callers arg0 caller @str r0, [r5,#(4*0)] @store arg0 from callee into callers arg0 caller
	
	//experimental
	//ldr r0,[r0]
	
	ldmia	r5, {r9,r10,r11,r12,r13,lr}
	movs    pc, lr, lsl #0 //(LR_<mode> depends on what kind of abort exception MPU/MMU throws) subs pc,lr,#0 causes to execute next instruction (data abort) correct 
	
@-------------------------------------------------------------------------------------------
@return from data abort exception : PC=R14_abt-8, and CPSR=SPSR_abt
@--------------------------------------------------------------------

@-------------------------------------------------swi code--------------------------------------------------
.global swiexcepstack
swiexcepstack:
	.word	0x00000000	@r0 	a1
	.word	0x00000000	@r1		a2
	.word	0x00000000	@r2		a3
	.word	0x00000000	@r3		a4
	.word	0x00000000	@r4		s1
	.word	0x00000000	@r5		s2
	.word	0x00000000	@r6		s3
	.word	0x00000000	@r7		s4
	.word	0x00000000	@r8		s5
	.word	0x00000000	@r9		s6
	.word	0x00000000	@r10	sl
	.word	0x00000000	@r11	ip
	.word	0x00000000	@r12	fp
	.word	0x00000000	@r13	sp
	.word	0x00000000	@r14	lr
	.word	0x00000000	@r15	pc

@andeq   r0, r1, r7, asr #2

@how to use swicaller:
@swicaller(arg) if arg being 0x08000000-0x08000000+romsize then gbaread saves into chunk
@otherwise swi exception will occur depending on u32 swiaddress case

.global swicaller
swicaller:
	ldr r5,=swiexcepstack
	STMIA    r5,{r0-r3,r12,r13,lr}
		swi 	=r0
	ldr r5,=swiexcepstack
	LDMIA    r5, {r0-r3,r12,r13,r15}
	
@---------------------------------------------
.global inter_swi
inter_swi:
	
@OLD swi handling (does not rely on NDS9 bios)
	ldr r5,=swiexcepstack
	STMIA    r5,{r0-r3,r12,r13,lr}    @ Store registers.

    MOV      r1, sp               @ Set pointer to parameters.
    MRS      r0, spsr             @ Get spsr.
    STMFD    sp!, {r0}            @ Store spsr onto stack. This is only really needed in case of
                                  @ nested SWIs.
	@ the next two instructions only work for SWI calls from ARM state.
	@ See Example 5.17 for a version that works for calls from either ARM or Thumb.
    LDR      r0,[lr,#-4]          @ Calculate address of SWI instruction and load it into r0.
    BIC      r0,r0,#0xFF000000    @ Mask off top 8 bits of instruction to give SWI number.
    @ r0 now contains SWI number
    @ r1 now contains pointer to stacked registers
	
    ldr r6,=exceptswiC			@ Call C routine to handle the SWI.
	ldr r6,[r6]
	bx r6  
	
	ldr 	r5,=swiexcepstack
    LDMIA  	r5, {r0-r3,r12,r13,r14} @ Restore registers and return.
	
	LDMFD    sp!, {r0}            @ Get spsr from stack.
    MSR      spsr_cf, r0          @ Restore spsr.
	
	movs pc,lr

//@new SWI handle 
	
	//ldr r6,=SPswi
	//ldr sp, [r6] 	@ use the new stack
	
	//mov r1, SP @u32 *R
	
	//ldr r6,=exceptswiC
	//ldr r6,[r6]
	
	//ldr r5,=swiexcepstack
	//str r14,[r5,#(4*7)]
	
	//ldrb r0,[lr,#-2] 	@swi #num from thumb
	
	//mov r14,r15			@replaces blx (armv4 and lower)
	//bx  r6				@(int op,  s32 *R)
	
	//ldr r5,=swiexcepstack
	//ldr r15,[r5,#(4*7)]
	
	//movs pc,lr
	//bx lr
	
@----------------------------------interrupt request handler---------------------------------------------
@IRQ BIOS Setup
@Note: MUST be on WRAM that is not protected, like VECTORS

.global irqbiosinst
.type   irqbiosinst STT_FUNC
irqbiosinst:
	
	mov r0,#0
	MRC P15, 0 ,r0, c9,c1,0 			@TCM Data TCM Base and Virtual Size
	MOV r0, r0, LSR #0xc
	MOV r0, r0, LSL #0xc
	LDR R1,=0x3ffc						@IRQ handler
	add r0,r0,r1
	
	//test: ldr r1,=0xc070c070	@test should make dtcm+0x3ffc jump to garbage
	
	str r2,[r0]
	bx lr

.global irqexcepstack
irqexcepstack:
	.word	0x00000000	@r0 	a1
	.word	0x00000000	@r1		a2
	.word	0x00000000	@r2		a3
	.word	0x00000000	@r3		a4
	.word	0x00000000	@r4		s1
	.word	0x00000000	@r5		s2
	.word	0x00000000	@r6		s3
	.word	0x00000000	@r7		s4
	.word	0x00000000	@r8		s5
	.word	0x00000000	@r9		s6
	.word	0x00000000	@r10	sl
	.word	0x00000000	@r11	ip
	.word	0x00000000	@r12	fp
	.word	0x00000000	@r13	sp
	.word	0x00000000	@r14	lr
	.word	0x00000000	@r15	pc
	
@-------------------------- vector variables

.global exceptrstC
exceptrstC:
	.word 0x00000000		

.global exceptundC
exceptundC:
	.word 0x00000000		

.global exceptswiC
exceptswiC:
	.word 0x00000000		

.global exceptprefabtC
exceptprefabtC:
	.word 0x00000000		

.global exceptdatabtC
exceptdatabtC:
	.word 0x00000000		

.global exceptreservC
exceptreservC:
	.word 0x00000000		

.global exceptirqC
exceptirqC:
	.word 0x00000000		

@------------------------------nds IRQ vectors directly
.global vblank_thread
.global hblank_thread
.global vcount_thread
.global fifo_thread


.align
.pool
.end