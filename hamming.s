@ Task: compute the Hamming distance on 2 words
@       Given:  2 integer arrays xs and ys, where len(xs)==len(ys)
@       Return: the number of positions where the two arrays differ
@               i.e. | { i | i <- 0..len(xs), xs[i]!=ys[i] } |
	
@ Follows ARM subroutine calling conventions
	
	@ Entry point (Callable from C): 
	.global hamming
	
hamming:		  @ Input: 2 ptrs to int arrays in R0 and R1, length in R2
	@ don't forget to push relevant registers here
	PUSH {r4, r5, r6, r7, lr}

	@ Main Program
	@ Initialise loop counter
	MOV r3, #0
	@ Initialise hamming distance
	MOV r7, #0

	@ Start Loop
	loop_start:
		@ If (i == length)
		CMP r3, r2
		@ Exit loop
		BEQ loop_end

		@ Calculate offset
		LSL r6, r3, #2

		@ Load numbers from array
		LDR r4, [r0, r6]
		LDR r5, [r1, r6]

		@ Compare the two numbers
		CMP r4, r5
		@ If they dont match, increment r7
		ADDNE r7, r7, #1

		@ i++
		ADD r3, r3, #1
		@ Loop back
		B loop_start

	loop_end:
		@ Set output to r0
		MOV r0, r7

		@ don't forget to pop relevant registers here
		POP {r4, r5, r6, r7, lr}
		BX LR

@ Indicate to the linker that the code in this file does not need the stack
@ to be executable. (Recent versions of GNU ld warn if this is not present.)
.section .note.GNU-stack,"",%progbits
	
  