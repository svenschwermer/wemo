/*
 * © 2013 Belkin International, Inc. and/or its affiliates. All rights reserved.
 */


#ifndef _SYSCFG_MTD_H_
#define _SYSCFG_MTD_H_

int mtd_get_hdrsize ();
long int mtd_get_devicesize ();
int mtd_write_from_file (const char *mtd_device, const char *file);
int load_from_mtd (const char *mtd_device);
int commit_to_mtd (const char *mtd_device);
int mtd_hdr_check (const char *mtd_device);

#endif // _SYSCFG_MTD_H_
