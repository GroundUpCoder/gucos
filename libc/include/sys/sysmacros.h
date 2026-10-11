
#pragma once
/* Classic 16-bit major/minor device-number encoding (device numbers are
   cosmetic in this runtime — there are no real devices). */
#define major(dev)       ((int)(((unsigned long)(dev) >> 8) & 0xff))
#define minor(dev)       ((int)((unsigned long)(dev) & 0xff))
#define makedev(ma, mi)  ((unsigned long)(((ma) << 8) | ((mi) & 0xff)))
  