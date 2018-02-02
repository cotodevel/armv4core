@@@@@@@@@@@@@@@@@@@@@@@payload to setup stacks in gba mode@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@required
.cpu arm7tdmi
.arm
.word

.global gba_setup
.type gba_setup STT_FUNC
gba_setup:

@Write arm7tdmi code here so when you disable ROMTEST define in settings.h, it will cause to armv4core to execute this code before
@executing GBA Code

rsbs r12,r0,r1,lsr #3
@ldr r3,[r0,r1,lsl #0x2] ok
mul r1,r0,r0
mla r1,r0,r1,r0
umull r0,r1,r2,r3
smull r0,r1,r2,r3
umlal r0,r1,r2,r3
smlal r0,r1,r2,r3
swp r1,r2,[r0]
swpb r1,r2,[r0]
swpeq r1,r2,[r0]
@bx r0 @works
strh r0,[r1]
strh r0,[r1,#0x10]
ldrh r0,[r1]
ldrh r0,[r1,#0x10]
mrs r0,spsr
msr cpsr,r0
ldr r0,=0x123
ldr r0,[r0,#0x10]
str r0,[r0]
str r0,[r0,#0x10]
stmia r0!,{r1-r12}^
ldmia r0!,{r1-r12}^
@b gba_setup    @works
@bx =r0
LDCEQL p2,c3,[R5,#24]!
STCEQL p2,c3,[R5,#24]!
CDP p1,10,c1,c2,c3
CDPEQ p2,5,c1,c2,c3,0
MRC p2,5,R1,c5,c6,1
MRCEQ p3,5,R1,c5,c6,0
SWI 0X100
and r0,r0,r0
eor r0,r0,r0
sub r0,r0,r0
rsb r0,r0,r0
add r0,r0,r0
adc r0,r0,r0
sbc r0,r0,r0
rsc r0,r0,r0
tst r0,#(1<<31)
teq r0,#(1<<30)
cmp r0,#(1<<29)
cmn r0,#(1<<28)
orr r0,r0,#(1<<27)
mov r0,#(0x07fffffc)
bic r0,r0,r0
mvn r0,#(0x07fffffc)
mov	r2, #0x12           @ Set IRQ stack
msr	cpsr, r2
ldr	sp, =0x03007FA0

@push pop ARM test
ldr r0,=0xc070ffff
ldr r1,=0xc171ffff
ldr r2,=0xc272ffff
ldr r3,=0xc373ffff
ldr r4,=0xc474ffff
ldr r5,=0xc575ffff
ldr r6,=0xc676ffff
ldr r7,=0xc777ffff
ldr r8,=0xc878ffff
ldr r9,=0xc979ffff
ldr r10,=0xca7affff
ldr r11,=0xcb7bffff
ldr r12,=0xcc7cffff
ldr r14,=0xce7effff

@test / bugged
push {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r14}

@test2 ok
@push {r0,r9,r10,r11,r12,r14}

mov r0,#0
mov r1,#0
mov r2,#0
mov r3,#0
mov r4,#0
mov r5,#0
mov r6,#0
mov r7,#0
mov r8,#0
mov r9,#0
mov r10,#0
mov r11,#0
mov r12,#0

pop {r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r14}

@test2 ok
@pop {r0,r9,r10,r11,r12,r14}^
@there should be now old values!
@bl wramtstasm @relative branches do not work from ROP unless the function / address we call is inside this ROP
mov	r2, #0x13			@ Set SVC stack
msr	cpsr, r2
ldr	sp, =0x03007FE0
mov	r2, #0x10           @ Set USR stack
msr	cpsr, r2
ldr sp,=0x03007F00
mov	r2, #0x1F           @ Set SYS stack (inherits USR stack)
msr	cpsr, r2
@clean regs
mov r0,#0
mov r1,#0
mov r2,#0
mov r3,#0
mov r4,#0
mov r5,#0
mov r6,#0
mov r7,#0
mov r8,#0
mov r9,#0
mov r10,#0
mov r11,#0
mov r12,#0

ldr pc,=#(0x08000000-4)             @entrypoint - 4 because prefetch

.pool       @we need the #imm fetches close
@@@@@@@@@@@@@@@@@@@@@@@payload to setup stacks in gba mode end@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
