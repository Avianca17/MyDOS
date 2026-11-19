#ifndef MYDOS_ELF_H
#define MYDOS_ELF_H

int elf_run(const char *path, unsigned int mbi, int argc, char **argv,
            int *exit_code);

#endif
