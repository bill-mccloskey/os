#ifndef SERIAL_H
#define SERIAL_H

#define SERIAL_COM1_BASE                0x3F8      /* COM1 base port */

void serial_init(unsigned short com, unsigned short divisor);
void serial_write_byte(unsigned int com, char c);
void serial_write(unsigned int com, const char* s);
void serial_printf(unsigned int com, const char* fmt, ...);

#endif  // SERIAL_H
