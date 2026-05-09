.text

.globl __syscall0
.type __syscall0, @function
__syscall0:
    mov %rdi, %rax
    syscall
    ret
.size __syscall0, . - __syscall0

.globl __syscall1
.type __syscall1, @function
__syscall1:
    mov %rdi, %rax
    mov %rsi, %rdi
    syscall
    ret
.size __syscall1, . - __syscall1

.globl __syscall2
.type __syscall2, @function
__syscall2:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    syscall
    ret
.size __syscall2, . - __syscall2

.globl __syscall3
.type __syscall3, @function
__syscall3:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    syscall
    ret
.size __syscall3, . - __syscall3
