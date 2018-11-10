#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

extern "C" {

/** outb:
 *  Sends the given data to the given I/O port. Defined in io.s
 *
 *  @param port The I/O port to send the data to
 *  @param data The data to send to the I/O port
 */
void outb(unsigned short port, unsigned char data);

/** inb:
 *  Read a byte from an I/O port.
 *
 *  @param  port The address of the I/O port
 *  @return      The read byte
 */
unsigned char inb(unsigned short port);

} // extern "C"

class IoPorts {
public:
  virtual void Out(int port, unsigned char data);
  virtual int In(int port);
};

#endif /* INCLUDE_IO_H */
