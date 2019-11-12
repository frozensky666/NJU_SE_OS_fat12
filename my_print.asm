global _Z9my_printfPKci

section .text
_Z9my_printfPKci:
    mov rax,1
    mov rdx,rsi
    mov rsi,rdi
    mov rdi,1
    syscall

    ret