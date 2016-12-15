#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if.h>
//#include <linux/mii.h>
#include <linux/types.h>
#include <errno.h>

#include <linux/autoconf.h>
#include "ra_ioctl.h"

void show_usage(void)
{
#ifndef CONFIG_RT2860V2_AP_MEMORY_OPTIMIZATION
   printf("mii_mgr -g -p [phy number] -r [register number]\n");
   printf("  Get: mii_mgr -g -p 3 -r 4\n\n");
   printf("mii_mgr -s -p [phy number] -r [register number] -v [0xvalue]\n");
   printf("  Set: mii_mgr -s -p 4 -r 1 -v 0xff11\n\n");
   printf("mii_mgr -c -p [phy number] [-P]\n");
   printf("  test connect state: mii_mgr -c -p 3 [-P]\n");
   printf("     exit status 0 if cable is connected\n");
   printf("     -P - power down PHY if cable is not connected\n");
#endif
}

int main(int argc, char *argv[])
{
   int sk, opt, ret;
   char options[] = "cgsp:Pr:v:?t";
   int method;
   struct ifreq ifr;
   ra_mii_ioctl_data mii;
   int bShowUsage = 0;
   int bExit = 1;
   int bConnectStatus = 0;
   int phy_id = -1;
   int reg_num = -1;
   int val_in = -1;
   int loops = 0;
   int Mode = 0;
   int bPowerDown = 0;
   int i;

   while ((opt = getopt(argc, argv, options)) != -1) {
      switch (opt) {
         case 'c':
            method = RAETH_MII_READ;
            bConnectStatus = 1;
            reg_num = 1;      // read status register
            break;
         case 'g':
            method = RAETH_MII_READ;
            break;
         case 's':
            method = RAETH_MII_WRITE;
            break;
         case 'p':
            phy_id = atoi(optarg);
            break;
         case 'P':
            bPowerDown = 1;
            break;
         case 'r':
            reg_num = atoi(optarg);
            break;
         case 'v':
            val_in = strtol(optarg, NULL, 16);
            break;
         case '?':
            bShowUsage = 1;
            break;
      }
   }

   switch(method) {
      case RAETH_MII_READ:
         if(phy_id == -1 || reg_num == -1) {
            bShowUsage = 1;
         }
         break;

      case RAETH_MII_WRITE:
         if(phy_id == -1 || reg_num == -1 || val_in == -1) {
            bShowUsage = 1;
         }
         break;

      default:
         bShowUsage = 1;
         break;
   }

   if (bShowUsage) {
      show_usage();
      return 0;
   }

   sk = socket(AF_INET, SOCK_DGRAM, 0);
   if (sk < 0) {
      printf("Open socket failed\n");
      return -1;
   }

   strncpy(ifr.ifr_name, "eth2", 5);
   ifr.ifr_data = &mii;

   if(bConnectStatus) {
   // Power down all PHYs except 0 to save power
      mii.reg_num = (__u16) 0;
      for(i = 0; i < 5; i++) {
         mii.phy_id = (__u16) i;
         mii.val_in = (__u16) (i == 0 ? 0x3100 : 0x3900);

         ret = ioctl(sk, RAETH_MII_WRITE, &ifr);
         if (ret < 0) {
            printf("mii_mgr: ioctl error\n");
            break;
         }
         else {
            printf("Phy %d powered %s\n",mii.phy_id,i == 0 ? "up" : "down");
         }
      }
   }

   mii.phy_id = (__u16) phy_id;
   mii.val_in = (__u16) val_in;
   mii.reg_num = (__u16) reg_num;

   do {
      ret = ioctl(sk, method, &ifr);
      if (ret < 0) {
         printf("mii_mgr: ioctl error\n");
      }
      else if(method == RAETH_MII_WRITE) {
         printf("Set: phy[%d].reg[%d] = %04x\n",
               mii.phy_id, mii.reg_num, mii.val_in);
      }
      else if(bConnectStatus) {
      // Check if link_status (bit 2) is set in the MII status register
         if(mii.val_out & 0x4) {
            printf("Link is UP on phy %d\n",mii.phy_id);
            bExit = 1;
            ret = 0;    // return 0 on good link
         }
         else if(loops++ >= 10) {
         // Give up after one second
            printf("Link is DOWN on phy %d\n",mii.phy_id);
            bExit = 1;
         // if there's no link disable the PHY to save power.  
            if(bPowerDown) {
               mii.val_in = (__u16) 0x3900;
               mii.reg_num = (__u16) 0;
               mii.phy_id = (__u16) 0;
               ret = ioctl(sk, RAETH_MII_WRITE, &ifr);
               if (ret < 0) {
                  printf("mii_mgr: ioctl error\n");
                  break;
               }
               else {
                  printf("Phy %d powered down\n",mii.phy_id);
               }
            }
            ret = EIO;  // return IO error on no link
         }
         else {
         // Sleep for 100 millseconds and then try again
            usleep(100000);   // 100 milliseconds
            bExit = 0;
         }
      }
      else {
         printf("Get: phy[%d].reg[%d] = %04x\n",
               mii.phy_id, mii.reg_num, mii.val_out);
      }
   } while(!bExit);

   close(sk);
   return ret;
}
