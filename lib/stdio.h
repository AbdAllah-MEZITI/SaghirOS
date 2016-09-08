/* Macros.  */

/* Some screen stuff.  */
/* The number of columns.  */
#define COLUMNS			80
/* The number of lines.  */
#define LINES			24
/* The attribute of an character.  */
#define ATTRIBUTE		7
/* The video memory address.  */
#define VIDEO			0xB8000

/* Forward declarations.  */
void cls (void);
void itoa (char *buf, int base, int d);
void putchar (int c);
void printf (const char *format, ...);

void os_putchar (int yp, int xp, unsigned char attribute, int c);
void os_printf (int yp, int xp, unsigned char attribute, const char *format, ...);
