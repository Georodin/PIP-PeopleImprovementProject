.text
.p2align 4
.globl pip_move_bridge
.seh_proc pip_move_bridge
pip_move_bridge:
    pushfq
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    subq $304,%rsp
    .seh_stackalloc 368
    .seh_endprologue
    movaps %xmm0,32(%rsp)
    movaps %xmm1,48(%rsp)
    movaps %xmm2,64(%rsp)
    movaps %xmm3,80(%rsp)
    movaps %xmm4,96(%rsp)
    movaps %xmm5,112(%rsp)
    movaps %xmm6,128(%rsp)
    movaps %xmm7,144(%rsp)
    movaps %xmm8,160(%rsp)
    movaps %xmm9,176(%rsp)
    movaps %xmm10,192(%rsp)
    movaps %xmm11,208(%rsp)
    movaps %xmm12,224(%rsp)
    movaps %xmm13,240(%rsp)
    movaps %xmm14,256(%rsp)
    movaps %xmm15,272(%rsp)
    stmxcsr 288(%rsp)
    movq %r13,%rcx
    movq %r15,%rdx
    leaq 160(%rsp),%r8
    leaq 368(%rsp),%r9
    callq pip_native_steer
    movl %eax,292(%rsp)
    ldmxcsr 288(%rsp)
    movaps 32(%rsp),%xmm0
    movaps 48(%rsp),%xmm1
    movaps 64(%rsp),%xmm2
    movaps 80(%rsp),%xmm3
    movaps 96(%rsp),%xmm4
    movaps 112(%rsp),%xmm5
    movaps 128(%rsp),%xmm6
    movaps 144(%rsp),%xmm7
    movaps 160(%rsp),%xmm8
    movaps 176(%rsp),%xmm9
    movaps 192(%rsp),%xmm10
    movaps 208(%rsp),%xmm11
    movaps 224(%rsp),%xmm12
    movaps 240(%rsp),%xmm13
    movaps 256(%rsp),%xmm14
    movaps 272(%rsp),%xmm15
    cmpl $0,292(%rsp)
    jne .Lcollision
    addq $304,%rsp
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
    popfq
    /* Original flag calculation, followed by the native collision branch. */
    testb %r14b,%r14b
    setne %al
    movl 0xdc(%rsp),%ecx
    xorb $1,%cl
    orb %al,%cl
    jmpq *pip_move_return(%rip)
.Lcollision:
    addq $304,%rsp
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rax
    popfq
    /* Every overridden proposal is rechecked by the game's wall/gate DDA. */
    jmpq *pip_collision_return(%rip)
.seh_endproc

.globl pip_bridge_selftest
.seh_proc pip_bridge_selftest
pip_bridge_selftest:
    pushq %r13
    .seh_pushreg %r13
    pushq %r15
    .seh_pushreg %r15
    pushq %rsi
    .seh_pushreg %rsi
    pushq %r14
    .seh_pushreg %r14
    subq $552,%rsp
    .seh_stackalloc 552
    movaps %xmm8,512(%rsp)
    .seh_savexmm %xmm8,512
    .seh_endprologue
    xorq %r13,%r13
    xorq %r15,%r15
    movl $1,%r14d
    movl $0,0xdc(%rsp)
    leaq .Lreturn(%rip),%rax
    movq %rax,pip_move_return(%rip)
    leaq .LforcedReturn(%rip),%rax
    movq %rax,pip_collision_return(%rip)
    movaps .Lvector(%rip),%xmm8
    movl $0x13579,%esi
    movl $0x24680,%r11d
    movl $0xabcdef,%eax
    movl $0x1234,%ecx
    cmpq %rax,%rax
    stc
    jmp pip_move_bridge
.Lreturn:
    pushfq
    popq %rdx
    andl $0x41,%edx
    cmpl $0,%edx
    jne .Lfail
    cmpl $0xabcd01,%eax
    jne .Lfail
    cmpl $1,%ecx
    jne .Lfail
    jmp .Lshared
.LforcedReturn:
    pushfq
    popq %rdx
    andl $0x41,%edx
    cmpl $0x41,%edx
    jne .Lfail
    cmpl $0xabcdef,%eax
    jne .Lfail
    cmpl $0x1234,%ecx
    jne .Lfail
.Lshared:
    cmpl $0x13579,%esi
    jne .Lfail
    cmpl $0x24680,%r11d
    jne .Lfail
    movaps %xmm8,%xmm0
    pcmpeqd .Lvector(%rip),%xmm0
    pmovmskb %xmm0,%eax
    cmpl $65535,%eax
    jne .Lfail
    xorl %eax,%eax
    jmp .Ldone
.Lfail:
    movl $1,%eax
.Ldone:
    movaps 512(%rsp),%xmm8
    addq $552,%rsp
    popq %r14
    popq %rsi
    popq %r15
    popq %r13
    retq
.seh_endproc
.section .rdata,"dr"
.p2align 4
.Lvector:
    .float 1.0,-2.0,3.0,-4.0
