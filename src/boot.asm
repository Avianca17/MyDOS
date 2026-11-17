bits 32

global _start
extern KernelEntryPoint

section .text
	align 4
	multiboot_header:
		dd 0x1BADB002
		dd 0x00000007
		dd -(0x1BADB002 + 0x00000007)
		dd 0
		dd 0
		dd 0
		dd 0
		dd 0
		dd 0
		dd 1024
		dd 768
		dd 32

	_start:
		cli
		mov esp, stack_top

		push ebx
		push eax
		call KernelEntryPoint

		cli
	.halt_after_kernel:
		hlt
		jmp .halt_after_kernel

section .rodata
	boot_message db 'Hello!', 0

section .bss
	align 16
	stack_bottom:
		resb 16384
	stack_top:
