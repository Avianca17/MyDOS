#ifndef IDT_H
#define IDT_H

typedef struct idt_entry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char  zero;
    unsigned char  flags;
    unsigned short base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed)) idt_ptr_t;

typedef struct registers {
    unsigned int gs, fs, es, ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    unsigned int int_no, err_code;
    unsigned int eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

void idt_init(void);
void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags);
void isr_handler(registers_t *regs);

#endif
