#include "idt.h"
#include "../panic.h"
#include "../../lib/string.h"

extern void idt_flush(unsigned int);

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

static idt_entry_t idt_entries[256];
static idt_ptr_t   idt_ptr;

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt_entries[num].base_low = (base & 0xFFFF);
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel = sel;
    idt_entries[num].zero = 0;
    idt_entries[num].flags = flags;
}

void idt_init(void) {
    idt_ptr.limit = (sizeof(idt_entry_t) * 256) - 1;
    idt_ptr.base = (unsigned int)&idt_entries;

    memset(idt_entries, 0, sizeof(idt_entry_t) * 256);

    // Populate CPU exception handlers 0 to 31 (0x08 = Kernel Code Segment, 0x8E = 32-bit Interrupt Gate)
    idt_set_gate(0,  (unsigned int)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (unsigned int)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (unsigned int)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (unsigned int)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (unsigned int)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (unsigned int)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (unsigned int)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (unsigned int)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (unsigned int)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (unsigned int)isr9,  0x08, 0x8E);
    idt_set_gate(10, (unsigned int)isr10, 0x08, 0x8E);
    idt_set_gate(11, (unsigned int)isr11, 0x08, 0x8E);
    idt_set_gate(12, (unsigned int)isr12, 0x08, 0x8E);
    idt_set_gate(13, (unsigned int)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned int)isr14, 0x08, 0x8E);
    idt_set_gate(15, (unsigned int)isr15, 0x08, 0x8E);
    idt_set_gate(16, (unsigned int)isr16, 0x08, 0x8E);
    idt_set_gate(17, (unsigned int)isr17, 0x08, 0x8E);
    idt_set_gate(18, (unsigned int)isr18, 0x08, 0x8E);
    idt_set_gate(19, (unsigned int)isr19, 0x08, 0x8E);
    idt_set_gate(20, (unsigned int)isr20, 0x08, 0x8E);
    idt_set_gate(21, (unsigned int)isr21, 0x08, 0x8E);
    idt_set_gate(22, (unsigned int)isr22, 0x08, 0x8E);
    idt_set_gate(23, (unsigned int)isr23, 0x08, 0x8E);
    idt_set_gate(24, (unsigned int)isr24, 0x08, 0x8E);
    idt_set_gate(25, (unsigned int)isr25, 0x08, 0x8E);
    idt_set_gate(26, (unsigned int)isr26, 0x08, 0x8E);
    idt_set_gate(27, (unsigned int)isr27, 0x08, 0x8E);
    idt_set_gate(28, (unsigned int)isr28, 0x08, 0x8E);
    idt_set_gate(29, (unsigned int)isr29, 0x08, 0x8E);
    idt_set_gate(30, (unsigned int)isr30, 0x08, 0x8E);
    idt_set_gate(31, (unsigned int)isr31, 0x08, 0x8E);

    idt_flush((unsigned int)&idt_ptr);
}

void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        panic_exception((unsigned char)regs->int_no, regs->err_code, "Unhandled CPU Exception");
    }
}
