/*
    adc.h - adc readout for battery status
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

#ifndef ADC_H
#define ADC_H

#define BAT_NORMAL	0x01
#define BAT_CHARGE	0x02
#define BAT_CHARGE_DONE	0x04
#define BAT_DEBUG	0x80


void startADC(void);
void calcBat(void);
void showBat(void);

#endif
