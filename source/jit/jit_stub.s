# we use the system v abi regardless of platform to make our lives easier here

.intel_syntax noprefix

# [[gnu::sysv_abi]]
# void jit_invoke(void** fptable, u16 fidx)
# engine in %rdi, fptable in %rsi, fidx in %rdx
.globl jit_invoke
jit_invoke:
	# save %rbp (callee-saved register)
	push RBP

	# initialize function table pointer and invoke given function
	mov RBP, RDI
	call QWORD PTR[RBP + RSI*8]

	# restore %rbp
	pop RBP

	ret

# [[gnu::sysv_abi]]
# void* jit_compile(JitEngine* engine, u16 index)
# engine in %rdi, index in %rsi# return value in %rax
.extern jit_compile

# function pointer table entries point to this function before code has been compiled
# we perform the following steps:
# 1. save all parameters destined for the real function
# 2. find out which function we need to compile by inspecting the bytes in the 'call' instruction of our caller
# 3. call jit to compile the required function for us
# 4. restore parameters
# 5. tail-call the compiled function (as to not create a dormant stack frame)
.globl jit_stub
jit_stub:
	# at this point %rbp a pointer to the function pointer table
	# jit_compile (coincidentally!) requires the pointer to the JitEngine in %rdi (1st parameter, system-v abi)
	# so we leave it there
	# while %rbp is available as an additional scratch register on amd64 (no frame pointers) it is callee-saved

	# save parameters
	sub RSP, 120      # 7 * 8 bytes: 6 int params + 8 xmm params + stack (mis)alignment (call pushes another 8 bytes for the return address)
	mov QWORD PTR[RSP], RDI
	mov QWORD PTR[RSP +  8], RSI
	mov QWORD PTR[RSP + 16], RDX
	mov QWORD PTR[RSP + 24], RCX
	mov QWORD PTR[RSP + 32], R8
	mov QWORD PTR[RSP + 40], R9

	movq QWORD PTR[RSP +  48], xmm0
	movq QWORD PTR[RSP +  56], xmm1
	movq QWORD PTR[RSP +  64], xmm2
	movq QWORD PTR[RSP +  72], xmm3
	movq QWORD PTR[RSP +  80], xmm4
	movq QWORD PTR[RSP +  88], xmm5
	movq QWORD PTR[RSP +  96], xmm6
	movq QWORD PTR[RSP + 104], xmm7

	# find out which function we need to compile
	mov RSI, QWORD PTR[RSP + 120] # load return address
	mov ESI, DWORD PTR[RSI -   4] # get offset into function pointer table from call instruction of our caller
	shr ESI, 3                   # compute index from offset

	mov RDI, QWORD PTR[RBP - 8] # load JitEngine
	# compile the function, returns address in %rax
	# note that we do not need to save any caller-saved registers as our caller already did this
	call jit_compile

	# restore parameters
	mov RDI, QWORD PTR[RSP]
	mov RSI, QWORD PTR[RSP +  8]
	mov RDX, QWORD PTR[RSP + 16]
	mov RCX, QWORD PTR[RSP + 24]
	mov R8,  QWORD PTR[RSP + 32]
	mov R9,  QWORD PTR[RSP + 40]

	movq XMM0, QWORD PTR[RSP +  48]
	movq XMM1, QWORD PTR[RSP +  56]
	movq XMM2, QWORD PTR[RSP +  64]
	movq XMM3, QWORD PTR[RSP +  72]
	movq XMM4, QWORD PTR[RSP +  80]
	movq XMM5, QWORD PTR[RSP +  88]
	movq XMM6, QWORD PTR[RSP +  96]
	movq XMM7, QWORD PTR[RSP +  104]

	add RSP, 120

	# tail-call the compiled function
	jmp RAX

.globl jit_member_stub
jit_member_stub:
    # at this point %rbp a pointer to the function pointer table
	# jit_compile (coincidentally!) requires the pointer to the JitEngine in %rdi (1st parameter, system-v abi)
	# so we leave it there
	# while %rbp is available as an additional scratch register on amd64 (no frame pointers) it is callee-saved

	# save parameters
	sub RSP, 120      # 7 * 8 bytes: 6 int params + 8 xmm params + stack (mis)alignment (call pushes another 8 bytes for the return address)
	mov QWORD PTR[RSP], RDI
	mov QWORD PTR[RSP +  8], RSI
	mov QWORD PTR[RSP + 16], RDX
	mov QWORD PTR[RSP + 24], RCX
	mov QWORD PTR[RSP + 32], R8
	mov QWORD PTR[RSP + 40], R9

	movq QWORD PTR[RSP +  48], xmm0
	movq QWORD PTR[RSP +  56], xmm1
	movq QWORD PTR[RSP +  64], xmm2
	movq QWORD PTR[RSP +  72], xmm3
	movq QWORD PTR[RSP +  80], xmm4
	movq QWORD PTR[RSP +  88], xmm5
	movq QWORD PTR[RSP +  96], xmm6
	movq QWORD PTR[RSP + 104], xmm7

	# find out which function we need to compile
	# the previous function had the function index in %rax so we can just reuse it
	mov RSI, RAX
	mov RDI, QWORD PTR[RBP - 8] # load JitEngine

	# compile the function, returns address in %rax
	# note that we do not need to save any caller-saved registers as our caller already did this
	call jit_compile

	# restore parameters
	mov RDI, QWORD PTR[RSP]
	mov RSI, QWORD PTR[RSP +  8]
	mov RDX, QWORD PTR[RSP + 16]
	mov RCX, QWORD PTR[RSP + 24]
	mov R8,  QWORD PTR[RSP + 32]
	mov R9,  QWORD PTR[RSP + 40]

	movq XMM0, QWORD PTR[RSP +  48]
	movq XMM1, QWORD PTR[RSP +  56]
	movq XMM2, QWORD PTR[RSP +  64]
	movq XMM3, QWORD PTR[RSP +  72]
	movq XMM4, QWORD PTR[RSP +  80]
	movq XMM5, QWORD PTR[RSP +  88]
	movq XMM6, QWORD PTR[RSP +  96]
	movq XMM7, QWORD PTR[RSP + 104]


	add RSP, 120

	# tail-call the compiled function
	jmp RAX
