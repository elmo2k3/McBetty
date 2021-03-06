/*
    serial.h - serial port functions
    Copyright (C) 2007  Ch. Klippel <ck@mamalala.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BOOP_SERIAL_H
#define BOOP_SERIAL_H

extern int fDebug;

#define	USRRxData      	(1 << 0)
#define	USRTxHoldEmpty 	(1 << 6)


#define TX_READY(s)    	((s) & USRTxHoldEmpty)
#define RX_DATA(s)     	((s) & USRRxData)

#define PUT_CHAR(p,c)  	(p= (unsigned )(c))

int serial_init (int baudrate);

int serial_getc (void);

#ifdef TRACE
int serial_flush_output(void) ;
void serial_outs (const char *s);
void serial_out_hex(unsigned char v);
#endif

void debug_out(char *s, unsigned int v);
void dbg(char *s);
PT_THREAD (serial_out(struct pt *pt));

#endif /* BOOP_SERIAL_H */
