#ifndef MYDOS_CMD_H
#define MYDOS_CMD_H

#include <mydos.h>

typedef void (*cmd_fn_t)(char *args, int *cursor_y);

int cmd_run(const char *name, char *args, int *cursor_y);

char *cmd_next_token(char **input);
int cmd_parse_drive_root(const char *path, BYTE *volume);

void cmd_set_mbi(unsigned int mbi);
unsigned int cmd_get_mbi(void);

void cmd_uptime_init(void);
unsigned int cmd_elapsed_uptime_seconds(void);
void cmd_format_short_uptime(unsigned int elapsed, char *out);

void cmd_help(char *args, int *cursor_y);
void cmd_clear(char *args, int *cursor_y);
void cmd_change_theme(char *args, int *cursor_y);
void cmd_loadkeys(char *args, int *cursor_y);
void cmd_dhcp(char *args, int *cursor_y);
void cmd_ping(char *args, int *cursor_y);
void cmd_echo(char *args, int *cursor_y);
void cmd_set(char *args, int *cursor_y);
void cmd_fetch(char *args, int *cursor_y);
void cmd_flint(char *args, int *cursor_y);
void cmd_date(char *args, int *cursor_y);
void cmd_doom(char *args, int *cursor_y);
void cmd_uptime(char *args, int *cursor_y);
void cmd_beep(char *args, int *cursor_y);
void cmd_shlogo(char *args, int *cursor_y);
void cmd_mkfile(char *args, int *cursor_y);
void cmd_cp(char *args, int *cursor_y);
void cmd_mv(char *args, int *cursor_y);
void cmd_rm(char *args, int *cursor_y);
void cmd_mkdir(char *args, int *cursor_y);
void cmd_ls(char *args, int *cursor_y);
void cmd_lsdrv(char *args, int *cursor_y);
void cmd_mkfat(char *args, int *cursor_y);
void cmd_cd(char *args, int *cursor_y);
void cmd_read(char *args, int *cursor_y);
void cmd_myeditor(char *args, int *cursor_y);
void cmd_calc(char *args, int *cursor_y);
void cmd_pwd(char *args, int *cursor_y);
void cmd_larry(char *args, int *cursor_y);
void cmd_mk_mbr_part(char *args, int *cursor_y);
void cmd_shtdwn(char *args, int *cursor_y);
void cmd_kp(char *args, int *cursor_y);
void cmd_rbt(char *args, int *cursor_y);

#endif
