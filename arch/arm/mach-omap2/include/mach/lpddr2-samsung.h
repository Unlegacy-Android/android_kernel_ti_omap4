/*
* SAMSUNG LPDDR2 timings.
*
* Copyright (C) 2010 Texas Instruments
*
* Kamel Slimani <k-slimani@ti.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef _LPDDR2_SAMSUNG_H
#define _LPDDR2_SAMSUNG_H

extern const struct lpddr2_timings lpddr2_samsung_timings_200_mhz;
extern const struct lpddr2_timings lpddr2_samsung_timings_400_mhz;
extern const struct lpddr2_timings lpddr2_samsung_timings_466_mhz;
extern const struct lpddr2_min_tck lpddr2_samsung_min_tck;
extern struct lpddr2_device_info lpddr2_samsung_4G_S4_dev;
extern struct lpddr2_device_info lpddr2_samsung_2G_S4_dev;

#endif
