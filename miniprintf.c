#include <stdarg.h>
#include <NDS/jtypes.h>

// Minimal sprintf implementation to avoid libc bloat
// Supports %s, %c, %d/%i, %x/%X with simple 0-padding

static char* num(char *str, unsigned int num, int base, int width, int pad_zero, int caps) {
    char buf[32];
    int i = 0;
    char *digits = caps ? "0123456789ABCDEF" : "0123456789abcdef";
    
    if (num == 0) {
        if (width == 0) buf[i++] = '0'; // Special case 0 if no width
        else {
            // If padding, we handle in loop, but raw 0 digit needed?
            // logic: usually print 0.
            buf[i++] = '0';
        }
    } else {
        while (num != 0) {
            buf[i++] = digits[num % base];
            num /= base;
        }
    }
    
    while (i < width) {
        buf[i++] = pad_zero ? '0' : ' ';
    }
    
    while (i-- > 0) {
        *str++ = buf[i];
    }
    return str;
}

int mini_sprintf(char *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *s = str;
    
    while (*fmt) {
        if (*fmt != '%') {
            *s++ = *fmt++;
            continue;
        }
        fmt++; // skip %
        
        int width = 0;
        int pad_zero = 0;
        
        if (*fmt == '0') {
            pad_zero = 1;
            fmt++;
        }
        
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        
        switch (*fmt) {
            case 's': {
                char *p = va_arg(args, char*);
                while (*p) *s++ = *p++;
                break;
            }
            case 'd':
            case 'i': {
                int v = va_arg(args, int);
                if (v < 0) {
                    *s++ = '-';
                    v = -v;
                }
                s = num(s, (unsigned int)v, 10, width, pad_zero, 0);
                break;
            }
            case 'x': s = num(s, va_arg(args, unsigned int), 16, width, pad_zero, 0); break;
            case 'X': s = num(s, va_arg(args, unsigned int), 16, width, pad_zero, 1); break;
            default:  *s++ = *fmt; break;
        }
        fmt++;
    }
    *s = 0;
    va_end(args);
    return s - str;
}