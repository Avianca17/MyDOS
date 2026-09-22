bits 32

global _start
extern KernelEntryPoint

section .text
	align 4
	multiboot_header:
		dd 0x1BADB002
		dd 0x00000003
		dd -(0x1BADB002 + 0x00000003)

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
	boot_message db 'MyDOS bootloader started', 0 ; Please ignore this as it won't appear anywhere and I don't want to remove it.

section .bss
	align 16
	stack_bottom:
		resb 16384
	stack_top:
