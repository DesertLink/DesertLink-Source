.text
.globl __chkstk
.def __chkstk; .scl 2; .type 32; .endef
__chkstk:
    pushq %rcx
    pushq %rax
    cmpq $0x1000, %rax
    leaq 24(%rsp), %rcx
    jb 2f
1:
    subq $0x1000, %rcx
    orq $0, (%rcx)
    subq $0x1000, %rax
    cmpq $0x1000, %rax
    ja 1b
2:
    subq %rax, %rcx
    orq $0, (%rcx)
    popq %rax
    popq %rcx
    retq
