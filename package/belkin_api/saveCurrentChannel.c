#include <stdio.h>
#include "iwlib.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

/*------------------------------------------------------------------*/
/*
 * Wrapper to extract some Wireless Parameter out of the driver
 */
static inline int
iwGetExt(int                  skfd,           /* Socket to the kernel */
           const char *         ifname,         /* Device name */
           int                  request,        /* WE ID */
           struct iwreq *       pwrq)           /* Fixed part of the request */
{
  /* Set device name */
  strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
  /* Do the request */
  return(ioctl(skfd, request, pwrq));
}


/*START: Retrieve the channel */
static int getBasicConfig(int skfd, const char *ifname, wireless_config *info)
{
    struct iwreq          wrq;

    memset((char *) info, 0, sizeof(struct wireless_config));

    /* Get wireless name */
    if(iwGetExt(skfd, ifname, SIOCGIWNAME, &wrq) < 0)
	/* If no wireless name : no wireless extensions */
	return(-1);
    /*else
    {
	strncpy(info->name, wrq.u.name, IFNAMSIZ);
	info->name[IFNAMSIZ] = '\0';
    }*/

    /* Get frequency / channel */
    if(iwGetExt(skfd, ifname, SIOCGIWFREQ, &wrq) >= 0)
    {
	info->has_freq = 1;
	info->freq = iw_freq2float(&(wrq.u.freq));
	info->freq_flags = wrq.u.freq.flags;
    }
    return 0;
}

static int get_info(int skfd, char *ifname, struct wireless_info *info)
{
    memset((char *) info, 0, sizeof(struct wireless_info));

    /* Get basic information */
    if(getBasicConfig(skfd, ifname, &(info->b)) < 0)
    {
	/* If no wireless name : no wireless extensions */
	/* But let's check if the interface exists at all */
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(struct ifreq));

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if(ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	    return(-ENODEV);
	else
	    return(-ENOTSUP);
    }

    /* Get ranges */
    if(iw_get_range_info(skfd, ifname, &(info->range)) >= 0)
	info->has_range = 1;

    return 0;
}

void
iwPrintFreq(char *	buffer,
	      int	buflen,
	      double	freq,
	      int	channel,
	      int	freq_flags)
{
  char	vbuf[16];

  /* Print the frequency/channel value */
  iw_print_freq_value(vbuf, sizeof(vbuf), freq);

  /* Check if channel only */
  if(freq < KILO)
    snprintf(buffer, buflen, "%s", vbuf);
  else
    snprintf(buffer, buflen, "%s", vbuf);
}

static char * display_info(struct wireless_info *info, char *ifname)
{
    char          buffer[128];    /* Temporary buffer */
    char *buff = NULL;

    buff = (char *)malloc(200*sizeof(char));

    memset(buff, 0, 200);
    /* Display device name and wireless name (name of the protocol used) */
    //sprintf(buff, "%-8.16s  %s  ", ifname, info->b.name);

    /* Display frequency / channel */
    if(info->b.has_freq)
    {
	double  freq = info->b.freq;    /* Frequency/channel */
	int     channel = -1;           /* Converted to channel */

	if(info->has_range && (freq < KILO))
	    channel = iw_channel_to_freq((int) freq, &freq, &info->range);

	/* Display */
	iwPrintFreq(buffer, sizeof(buffer), freq, -1, info->b.freq_flags);
	strcpy(buff, buffer);
    }
    return buff;
}

static char *print_info(int skfd, char *ifname)
{
    struct wireless_info  info;
    int                   rc = 0;
    char *buff = NULL;

    rc = get_info(skfd, ifname, &info);
    switch(rc)
    {
	case 0:     /* Success */
	    /* Display it ! */
	    buff = display_info(&info, ifname);
	    break;

	case -ENOTSUP:
	    fprintf(stderr, "%-8.16s  no wireless extensions.\n\n",
		    ifname);
	    break;

	default:
	    fprintf(stderr, "%-8.16s  %s\n\n", ifname, strerror(-rc));
    }

    return buff;
}

char *channelRetrieve(char *iface) {
    int skfd;             /* generic raw socket desc.     */
    int sret = 0;
    char *buff = NULL;

    /* Create a channel to the NET kernel. */
    if((skfd = socket( AF_INET, SOCK_DGRAM, 0 )) < 0)
    {
	perror("socket");
	exit(-1);
    }

    buff = print_info(skfd, iface);

    /* Close the socket. */
    close(skfd);
    return buff;
}
/*END: Retrieve the channel */


int main() {
    char iface[] = "ra0";
    char *buff;
    char buf[100] = {'\0'};
    char *ParameterName = "wl0_currentChannel";

    buff = channelRetrieve(iface);
    if(buff) {
	sprintf(buf, "nvram_set %s %s", ParameterName, buff);
	//printf("%s",buf);
	system(buf);
    }

    free(buff);

    return 0;
}
