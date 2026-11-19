#ifndef MYDOS_PORT_STDIO_H
#define MYDOS_PORT_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct _MYDOS_FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list args);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t size, const char *format, ...);
int vsprintf(char *buffer, const char *format, va_list args);
int vsnprintf(char *buffer, size_t size, const char *format, va_list args);
int puts(const char *text);
int fputs(const char *text, FILE *stream);
void perror(const char *prefix);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fflush(FILE *stream);
void setbuf(FILE *stream, char *buffer);
int getchar(void);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream);
int feof(FILE *stream);
int fscanf(FILE *stream, const char *format, ...);
int sscanf(const char *text, const char *format, ...);
int remove(const char *path);

#endif
