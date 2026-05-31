#include <stdio.h>
#include <stdarg.h>

void putf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
void putint(int n)  { printf("%d", n); }
void putch(int c)   { putchar(c); }
void putarray(int n, int a[]) {
    putint(n);
    putchar(':');
    putchar(' ');
    for (int i = 0; i < n; i++) {
        putint(a[i]);
        if (i + 1 < n) putchar(' ');
    }
    putchar('\n');
}
int getarray(int a[]) {
    int n;
    scanf("%d", &n);
    for (int i = 0; i < n; i++) {
        scanf("%d", &a[i]);
    }
    return n;
}
int getint(void)    { int n; scanf("%d", &n); return n; }
int getch(void)     { return getchar(); }
void starttime(void){}
void stoptime(void){}