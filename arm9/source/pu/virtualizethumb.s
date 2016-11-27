@.cpu arm7tdmi
.cpu arm946e-s
.align 2 @use always align 2 in thumb code
.code 16
.thumb
.syntax unified	@https://sourceware.org/binutils/docs-2.24/as/ARM_002dInstruction_002dSet.html	//old syntax (pop) to new ARM syntax (pop.n 16 bit vs pop.w 32 bit)

@DEBUG and dissect THUMB code
.global opcode_test_thumb
.type opcode_test_thumb STT_FUNC
opcode_test_thumb:

b 1

1:
pop.n	{r2}

bx lr
	
.align
.pool
.end