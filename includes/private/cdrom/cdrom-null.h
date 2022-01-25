#ifndef CDROM_NULL_H
#define CDROM_NULL_H

/* this header file lists the functions provided by
   various platform specific cdrom-ioctl files */

int cdrom_null_open(char d);
void cdrom_null_reset();
void null_close();

#endif /* ! CDROM_NULL_H */
