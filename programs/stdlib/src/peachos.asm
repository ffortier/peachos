[BITS 32]

section .asm

global print:Function
global peachos_getkey:Function
global peachos_malloc:Function
global peachos_free:Function
global peachos_putchar:Function

; void print(const char* filename);
print:
    push ebp
    mov ebp, esp
    push dword[ebp+8]
    mov eax, 1
    int 0x80
    add esp, 4
    pop ebp
    ret

; char peachos_getkey();
peachos_getkey:
    push ebp
    mov ebp, esp
    mov eax, 2
    int 0x80
    pop ebp
    ret

; void peachos_putchar(char c);
peachos_putchar:
    push ebp
    mov ebp, esp
    push dword[ebp+8]
    mov eax, 3
    int 0x80
    add esp, 4
    pop ebp
    ret

; void *peachos_malloc(size_t size);
peachos_malloc:
    push ebp
    mov ebp, esp
    push dword[ebp+8]
    mov eax, 4
    int 0x80
    add esp, 4
    pop ebp
    ret

; void peachos_free(void* ptr);
peachos_free:
    push ebp
    mov ebp, esp
    push dword[ebp+8]
    mov eax, 5
    int 0x80
    add esp, 4
    pop ebp
    ret