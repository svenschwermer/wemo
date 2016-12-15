/* 2013 WNC Corp. uartf/uartlite terminals controller package */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <poll.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <termios.h>
#include <linux/serial.h>

//#define UARTF_POLL_INTERVAL_MSEC       1000
#define UARTF_POLL_INTERVAL_MSEC       10
#define WNC_UARTF_TTY                  "/dev/ttyS0"
#define WNC_UARTF_BAUDRATE             115200  //57600  //115200
#define WNC_UARTF_BAUDRATE_TERMBITS    B115200 //B57600 //B115200
int     uartf_fd = -1;
struct  pollfd uartf_poll;
char    uartf_send_data[80] = "";
int     uartf_rts_cts = 1;
int     uartf_rx_dump = 1;
int     uartf_rx_dump_ascii = 1;
int     uartf_auto_test = 0;
int     uartf_cl_baud = 0;

//WNC_Roger 2013/07/30 ttyS1 is initailized as /dev/console
//#define UARTLITE_POLL_INTERVAL_MSEC    1000
//#define WNC_UARTLITE_TTY               "/dev/ttyS1"
//#define WNC_UARTLITE_BAUDRATE          57600
//#define WNC_UARTLITE_BAUDRATE_TERMBITS B57600
//int     uartlite_fd = -1;
//struct  pollfd uartlite_poll;
//char    uartlite_send_data[80] = "";
void process_options(int argc, char * argv[]) ;

void uartlite_dump_data(unsigned char * b, int count) {
	printf("%d bytes: ", count);
	int i;
	for (i=0; i < count; i++)
		printf("%02x ", b[i]);
	printf("\n");
}

void uartlite_dump_data_ascii(unsigned char * b, int count) {
	int i;
	for (i=0; i < count; i++)
		printf("%c", b[i]);
		//uartlite_write_data("%c", b[i]);
}

void uartf_read_data() {
	unsigned char rb[1024];
	int c = read(uartf_fd, &rb, sizeof(rb));
	if (c > 0) {
		if (uartf_rx_dump) {
			if (uartf_rx_dump_ascii)
				uartlite_dump_data_ascii(rb, c);
			else
				uartlite_dump_data(rb, c);
		}
	}
}

char uartf_data_sent=0;
void uartf_write_data() {
	int c;
	unsigned char write_data[80] = {0};
	if (uartf_data_sent)
	    return;
	sprintf(write_data, "%s", uartf_send_data );
	*(write_data+strlen(write_data))='\n';
	c=write(uartf_fd, &write_data, strlen(write_data)+1);
	if (c>0) {
	    uartf_data_sent=1;
	    return;
	}
}

struct termios orig_uartf_termios;
void setup_uartf_termios(int baud_termbits, int cl_baud) {
	struct termios newtio;
	//int baud=WNC_UARTF_BAUDRATE_TERMBITS;
	uartf_fd = open(WNC_UARTF_TTY, O_RDWR | O_NONBLOCK);
	if (uartf_fd < 0) {
		printf("error open uartf(zigbee:%s)\n", WNC_UARTF_TTY);
		exit(-1);
	}
	printf("\nuartf %s baud=%d %s %s %s\n", WNC_UARTF_TTY, cl_baud, uartf_auto_test?"(Auto-TX)":"", uartf_rx_dump_ascii?"(RX ascii)":"(RX raw)" , uartf_rts_cts?"CTS/RTS":"");
	tcgetattr(uartf_fd, &newtio);
	memcpy(&orig_uartf_termios, &newtio, sizeof(struct termios));
	
	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = baud_termbits | CS8 | CLOCAL | CREAD;
	if (uartf_rts_cts)
		newtio.c_cflag |= CRTSCTS;
	newtio.c_iflag = 0;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0;
	// block for up till 128 characters
	//newtio.c_cc[VMIN] = 128;
	newtio.c_cc[VMIN] = 1;
	// 0.5 seconds read timeout
	//newtio.c_cc[VTIME] = 5;
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VINTR] = _POSIX_VDISABLE;      // Turn off CTRL-C, so we can trap it
	/* now clean the modem line and activate the settings for the port */
	tcflush(uartf_fd, TCIFLUSH);
	tcsetattr(uartf_fd,TCSANOW,&newtio);
#if 0
	{
        struct termios options;
        tcgetattr(uartf_fd, &options);
		options.c_cflag |= CRTSCTS;
        cfsetispeed(&options, B115200);
        cfsetospeed(&options, B115200);
        tcsetattr(uartf_fd, TCSANOW, &options);
        printf("setting %s B115200\n", WNC_UARTF_TTY);
	}
#endif
}

//WNC_Roger 2013/07/30 ttyS1 is initailized as /dev/console
//void setup_uartlite_termios(void) {
//	struct termios newtio;
//	int baud=WNC_UARTLITE_BAUDRATE_TERMBITS;
//	uartlite_fd = open(WNC_UARTLITE_TTY, O_RDWR | O_NONBLOCK);
//	if (uartlite_fd < 0) {
//		printf("error open uartlite(console:%s)\n", WNC_UARTLITE_TTY);
//		exit(-1);
//	}
//	printf("\nuartlite %s baud=%d\n", WNC_UARTLITE_TTY, WNC_UARTLITE_BAUDRATE);
//	bzero(&newtio, sizeof(newtio));
//	newtio.c_cflag = baud | CS8 | CLOCAL | CREAD;
//	newtio.c_iflag = 0;
//	newtio.c_oflag = 0;
//	newtio.c_lflag = 0;
//	// block for up till 128 characters
//	newtio.c_cc[VMIN] = 128;
//	// 0.5 seconds read timeout
//	newtio.c_cc[VTIME] = 5;
//	//newtio.c_cc[VINTR] = _POSIX_VDISABLE;      // Turn off CTRL-C, so we can trap it
//	/* now clean the modem line and activate the settings for the port */
//	tcflush(uartlite_fd, TCIFLUSH);
//	tcsetattr(uartlite_fd,TCSANOW,&newtio);
//}
//#include <stdarg.h>
//#define VAARG_PARSE(x,y)    {va_list ap; va_start(ap, x);   vsnprintf(y, sizeof(y), x, ap);  va_end(ap); }
//inline void uartlite_write_data(const char* fname, const char *fmt, ...) {
//	int c;
//    char tmp[1024];
//    VAARG_PARSE(fmt, tmp)
//	c=write(uartlite_fd, &tmp, strlen(tmp)+1);
//}


#define CONSOLE_COMMAND_MAX_LENGTH  4096
#define FLUSH_STR(x)     {printf( x ); fflush(stdout);}
#define FLUSH_STR2(x,y)  {printf( x , y ); fflush(stdout);}
#define FLUSH_CHAR(x)    {printf( "%c" , x ); fflush(stdout);}
int command_line_edit(char *cmd_str);

struct termios orig_console_termios;
void *uartlite_read_thread(void *arg) {
//    unsigned char rb[128];
    char cmd_str[CONSOLE_COMMAND_MAX_LENGTH];
    struct termios termios_p;
    tcgetattr( 0, &termios_p );
    memcpy(&orig_console_termios, &termios_p, sizeof(struct termios));
	termios_p.c_lflag &= ~ICANON;                 // unbuffered input
	termios_p.c_lflag &= ~(ECHO | ECHONL | ISIG); // Turn off echoing and CTRL-C, so we can trap it
	termios_p.c_cc[VMIN] = 1;
	termios_p.c_cc[VTIME] = 0;
	termios_p.c_cc[VINTR] = _POSIX_VDISABLE;      // Turn off CTRL-C, so we can trap it
	tcsetattr(0, TCSANOW, &termios_p);

    while (1){

        FLUSH_STR("Command: ");
		if (command_line_edit(cmd_str)!=0)
		    continue;
#if 0
        if (read(0, &rb, sizeof(rb)) > 0) {
            sprintf(uartf_send_data, "%s", rb);
            memset(rb, 0, sizeof(rb));
            if (strchr(uartf_send_data, '\n')) {
                uartf_poll.events |= POLLOUT;
                uartf_data_sent=0;
            }
        }
#endif
    }
}
void setup_uartf_poll(void) {
	uartf_poll.fd = uartf_fd;
	if (1)
		uartf_poll.events |= POLLIN;
	else
		uartf_poll.events &= ~POLLIN;
	if (uartf_auto_test)
		uartf_poll.events |= POLLOUT;
	else
		uartf_poll.events &= ~POLLOUT;
}
// converts integer baud to Linux define
// linux-xxx/include/uapi/asm-generic/termbits.h
int get_baud(int baud) {
	switch (baud) {
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 500000:
		return B500000;
	case 576000:
		return B576000;
	case 921600:
		return B921600;
	case 1000000:
		return B1000000;
	case 1152000:
		return B1152000;
	case 1500000:
		return B1500000;
	case 2000000:
		return B2000000;
	case 2500000:
		return B2500000;
	case 3000000:
		return B3000000;
	case 3500000:
		return B3500000;
	case 4000000:
		return B4000000;
	default: 
		return -1;
	}
}
int main(int argc, char* argv[]) {
    pthread_t uartlite_thread;
    int baud_termbits = WNC_UARTF_BAUDRATE_TERMBITS;
	process_options(argc, argv);
	if (uartf_cl_baud)
		baud_termbits = get_baud(uartf_cl_baud);
	else
		uartf_cl_baud = WNC_UARTF_BAUDRATE;

	setup_uartf_termios(baud_termbits, uartf_cl_baud);
	setup_uartf_poll();
    pthread_create(&uartlite_thread, NULL, &uartlite_read_thread, NULL);
	while (1) {
		int uartf_ret = poll(&uartf_poll, 1, UARTF_POLL_INTERVAL_MSEC);
		if (uartf_ret==-1) {
			perror("error poll() uartf");
		}
		else if (uartf_ret) {
			if (uartf_poll.revents & POLLIN)
				uartf_read_data();
			if (uartf_poll.revents & POLLOUT)
				uartf_write_data();
		}
	}
	if (uartf_fd>0)
	    close(uartf_fd);
	//if (uartlite_fd>0) //use stdin
	//    close(uartlite_fd);
}
void display_help(void) {
	printf("Usage: Home-Automation-Zigbee Serial Test [OPTION]\n"
			"\n"
			"  -b, --baud        Baud rate (%d is default)\n"
			"\n", WNC_UARTF_BAUDRATE );
	exit(0);
}
void process_options(int argc, char * argv[]) {
	for (;;) {
		int option_index = 0;
		static const char *short_options = "b:R:ct:";
		static const struct option long_options[] = {
			{"baud", required_argument, 0, 'b'},
			{"rx_dump", required_argument, 0, 'R'},
			{"rts-cts", no_argument, 0, 'c'},
			{"trx_test", required_argument, 0, 't'},  //require send text data
			{0,0,0,0},
		};
		int c = getopt_long(argc, argv, short_options, long_options, &option_index);
		if (c == EOF)
			break;
		switch (c) {
		case 'h':
			display_help();
			break;
		case 'b':
			uartf_cl_baud = atoi(optarg);
			break;
		case 'R':
			uartf_rx_dump = 1;
			uartf_rx_dump_ascii = !strcmp(optarg, "ascii");
			break;
		case 't':
			uartf_auto_test = 1;
            if (optarg)
              if (strlen(optarg))
                strncpy(uartf_send_data, optarg, sizeof(uartf_send_data));
			uartf_rx_dump = 1;
			uartf_rx_dump_ascii = 1;
            //printf("send_data='%s' (auto=%d dump=%d ascii=%d)\n", uartf_send_data, uartf_auto_test, uartf_rx_dump, uartf_rx_dump_ascii);
            break;
		case 'c':
			uartf_rts_cts = 1;
			break;
		}
	}
}

void exit_program(void) {
	if (uartf_fd>0)
	    close(uartf_fd);

	tcsetattr(0, TCSANOW, &orig_uartf_termios);
	tcsetattr(0, TCSANOW, &orig_console_termios);
	printf("\nbye\n");
	exit(0);
}

void process_command(char *cmd_str) {
    if (!strcmp(cmd_str, "bye")||!strcmp(cmd_str, "quit"))
        exit_program();
    else {
        sprintf(uartf_send_data, "%s\n", cmd_str); //append LF
        uartf_poll.events |= POLLOUT;
        uartf_data_sent=0;
    }
}

//WNC_Roger 2013/07/30 add command line editor
typedef enum 
{
	txt_keypad = 0,
	txt_stop ,
	txt_q ,
	TEXT_MENU_COUNT,
} enumTextMenu;
#define CMD_READ          "read"
#define CMD_STOP          "stop"
#define CMD_QUIT          "q"
typedef struct
{
	int  index;
	char cmd[15];
	char op[15];
	char desc[80];
} UARTLITE_TEXT_COMMAND;

typedef struct
{
    char op[15];
} GENERIC_OPS;

#define OP_ON                  "on"
#define OP_OFF                 "off"
#define OP_ANTENNA_DIV         "div"
#define OP_ANTENNA_MAIN        "main"
#define OP_ANTENNA_AUX         "aux"
GENERIC_OPS generic_ops[]=
{
    { OP_ON },
    { OP_OFF },
    { OP_ANTENNA_DIV },
    { OP_ANTENNA_MAIN },
    { OP_ANTENNA_AUX },
};
UARTLITE_TEXT_COMMAND Text_Menu[] = {
    {   txt_keypad      , CMD_READ,        "",   "" },
    {   txt_stop        , CMD_STOP,        "",    "[stop any read action]" },
    {   txt_q           , CMD_QUIT,        "",    "[exit debugger]\n" },
};
char total_text_items=TEXT_MENU_COUNT;
#define CMD_COMMAND_LINE_HISTORY    "history"
#define CHAR_EXECUTE_HISTORY_NUM    '!'
#define COMMAND_HISTORY_CMD_LENGTH  100
#define COMMAND_HISTORY_DEPTH       200
#define COMMAND_TAB_KEYWORD_MAX_LEN 256
#define DEFAULT_ITEM_IDX           (-1)
enum {
  ESC = 27,
  DEL = 127,
} SPECIAL_ASCII_CODE;
#define PRINTABLE_ASCII_MIN         32   //space
#define PRINTABLE_ASCII_MAX         126  //~

static char last_keyword[COMMAND_TAB_KEYWORD_MAX_LEN];
static char last_item[COMMAND_TAB_KEYWORD_MAX_LEN];
static int  last_item_idx, keyed_after_tab, last_item_idx_generic;
static char *last_cache_pos;

static char cmd_history[COMMAND_HISTORY_DEPTH][COMMAND_HISTORY_CMD_LENGTH];
static unsigned int cmd_history_pos=0, cmd_history_counter=0, reach_history_top=0, reach_history_bottom=0;
void cle_clean_line(unsigned int scr_pos)
{
    unsigned int i=scr_pos;
    while (i-- >0 )
    {
        FLUSH_CHAR('\b')
        FLUSH_CHAR(' ')
        FLUSH_CHAR('\b')
    }
}
void cle_start_of_line(unsigned int *str_pos, unsigned int *scr_pos)
{
    unsigned int i=*scr_pos;
    while (i-- >0 )
        FLUSH_CHAR('\b')
    *str_pos=0;
    *scr_pos=0;
}
void cle_print_str(unsigned int str_len, char *cmd_str, unsigned int *str_pos, unsigned int *scr_pos, int go_last_char)
{
    unsigned int i;
    for (i=0;i<str_len;i++)
        FLUSH_CHAR(cmd_str[i])
    if (str_len && go_last_char)
    {
        FLUSH_CHAR('\b')
        *str_pos=str_len-1;
        *scr_pos=str_len-1;
    }
    else
    {
        *str_pos=str_len;
        *scr_pos=str_len;
    }
}
void cle_backspace(unsigned int *str_len, char *cmd_str, unsigned int *str_pos, unsigned int *scr_pos, unsigned int *cmd_modified)
{
    if ((*scr_pos==*str_len)&&(*str_len>0))
    {
        FLUSH_CHAR('\b')
        FLUSH_CHAR(' ')
        FLUSH_CHAR('\b')
        (*str_len)--;
        (*scr_pos)--;
        (*str_pos)--;
        cmd_str[*str_len]='\0';
        *cmd_modified=1;
        keyed_after_tab=1;
    }
    else if (*scr_pos>0)
    {
        unsigned int i=*str_len;
        FLUSH_CHAR('\b')
        (*scr_pos)--;
        (*str_pos)--;
        (*str_len)--;
        for (i=*scr_pos;i<*str_len;i++)
        {
            cmd_str[i]=cmd_str[i+1];
            FLUSH_CHAR(cmd_str[i])
        }
        FLUSH_CHAR(' ')
        for (i=(*str_len)+1; i>*scr_pos; i--)
        {
            FLUSH_CHAR('\b')
        }
        cmd_str[*str_len]='\0';
        *cmd_modified=1;
        keyed_after_tab=1;
    }
}
void cle_up_arrow(unsigned int *str_len, char *cmd_str, unsigned int *str_pos, unsigned int *scr_pos)
{
    unsigned int i;
    //FLUSH_STR("UP\n");
    if (cmd_history_pos>0)
    {
        reach_history_top=0;
        cmd_history_pos--;
        if (reach_history_bottom)
        {
            reach_history_bottom=0;
            cmd_history_pos=cmd_history_counter-1;
        }
    }
    else if (cmd_history_counter!=1)
    {
        reach_history_top=1;
        cle_clean_line(*scr_pos);
        *str_pos=0;
        *str_len=0;
        *scr_pos=0;
        return;
    }
    if ( strlen(cmd_history[cmd_history_pos])>0 )
    {
        cle_clean_line(*scr_pos);
        *str_len=strlen(cmd_history[cmd_history_pos]);
        strncpy(cmd_str, cmd_history[cmd_history_pos], COMMAND_HISTORY_CMD_LENGTH-1);
        for (i=0;i<*str_len;i++)
            FLUSH_CHAR(cmd_str[i])
        *str_pos=*str_len;
        *scr_pos=*str_len;
    }
}
void cle_down_arrow(unsigned int *str_len, char *cmd_str, unsigned int *str_pos, unsigned int *scr_pos)
{
    unsigned int i;
    //FLUSH_STR("DOWN\n");
    if (cmd_history_pos<cmd_history_counter-1)
    {
        reach_history_bottom=0;
        cmd_history_pos++;
        if (reach_history_top)
        {
            reach_history_top=0;
            cmd_history_pos=0;
        }
    }
    else
    {
        reach_history_bottom=1;
        cle_clean_line(*scr_pos);
        *str_pos=0;
        *str_len=0;
        *scr_pos=0;
        return;
    }
    if ( strlen(cmd_history[cmd_history_pos])>0 )
    {
        cle_clean_line(*scr_pos);
        *str_len=strlen(cmd_history[cmd_history_pos]);
        strncpy(cmd_str, cmd_history[cmd_history_pos], COMMAND_HISTORY_CMD_LENGTH-1);
        for (i=0;i<*str_len;i++)
            FLUSH_CHAR(cmd_str[i])
        *str_pos=*str_len;
        *scr_pos=*str_len;
    }
}
void cle_right_arrow(unsigned int str_len, char *cmd_str, unsigned int *str_pos, unsigned int *scr_pos)
{
    //FLUSH_STR("RIGHT\n");
    if (*scr_pos<str_len)
    {
        (*scr_pos)++;
        FLUSH_CHAR(cmd_str[(*str_pos)++])
    }
}
void cle_left_arrow(unsigned int *str_pos, unsigned int *scr_pos)
{
    //FLUSH_STR("LEFT\n");
    if (*scr_pos>0)
    {
        FLUSH_CHAR('\b')
        (*scr_pos)--;
        *str_pos=*scr_pos;
    }
}
char *cle_rm_spaces_around(char *tmp_str)
{
    int space_counter;
    //rm space before
    for (space_counter=0; tmp_str[space_counter]==' '; space_counter++);
    strcpy (tmp_str, &tmp_str[space_counter]);
    //rm space after
    for (space_counter=strlen(tmp_str)-1; tmp_str[space_counter]==' '; space_counter--);
    tmp_str[space_counter+1] = (char)NULL;
    return tmp_str;
}
#define CMD_SYSTEM_CMD    "shell_command"  //cmd_ShellCommand()
void cle_enter(unsigned int str_len, char *cmd_str, unsigned int *cmd_modified)
{
    char shell_cmd_str[32];
    char* orig_cmd_str;
    sprintf(shell_cmd_str,"%s ",CMD_SYSTEM_CMD);
    cmd_str[str_len]='\0';
    FLUSH_CHAR('\n')
    if (strstr(cmd_str,shell_cmd_str))
    {
        orig_cmd_str=cmd_str;
        cmd_str=cmd_str+strlen(shell_cmd_str);
        //printf("%s '%s'\n",__func__,cmd_str);
        if (!strcasecmp(cmd_str,"cat")) //avoid hangs on 'cat'
          printf("wrong parameter!\n");
        else
        {
          system(cmd_str);
          printf("\nShellCommand: \"%s\" done\n",cmd_str);
          cmd_str=orig_cmd_str;
        }
    }
    else
        cmd_str=cle_rm_spaces_around(cmd_str);
    if (strlen(cmd_str)==0)
    {
        //sprintf(cmd_str, "%c",'h'); // show help?
        return;
    }
    //FLUSH_STR2("strlen(cmd_str)=%d",strlen(cmd_str));
    if (cmd_str[0]=='!')         // no history for !xx cmd
        return;
    if (cmd_history_pos==(cmd_history_counter-1))
        reach_history_bottom=1;
    if (*cmd_modified)            // append modified command
    {
        if ((cmd_history_counter<COMMAND_HISTORY_DEPTH)&& *cmd_modified)
            cmd_history_counter++;
        cmd_history_pos=cmd_history_counter-1;
        strncpy(cmd_history[cmd_history_pos], cmd_str, COMMAND_HISTORY_CMD_LENGTH-1);
    }
    else
    {
        if (cmd_history_pos<cmd_history_counter-1)
            cmd_history_pos++;   // advanced to next history
    }
}
void cle_cmd_add_chr(unsigned int *str_len, char *cmd_str, unsigned int *str_pos, unsigned int *scr_pos, unsigned int *cmd_modified, unsigned char ch)
{
    FLUSH_CHAR(ch)
    cmd_str[*str_pos]=ch;
    *cmd_modified=1;
    keyed_after_tab=1;
    (*str_pos)++;
    (*scr_pos)++;
    if (*scr_pos>*str_len)
        *str_len=*scr_pos;
}
void cle_show_history(void)
{
    unsigned int i;
    for (i=0;i<cmd_history_counter;i++)
    {
        FLUSH_STR2("%3d. ",i)
        FLUSH_STR2("%s\n",cmd_history[i])
    }
}
int cle_exec_history(char *cmd_str)
{
    unsigned int tmp_history_pos=atoi(cmd_str+1);
    if (tmp_history_pos<cmd_history_counter)
    {
        cmd_history_pos=tmp_history_pos;
        if (cmd_history_pos==0)
            reach_history_top=1;
        if (cmd_history_pos==(cmd_history_counter-1))
            reach_history_bottom=1;
        strncpy(cmd_str, cmd_history[cmd_history_pos], COMMAND_HISTORY_CMD_LENGTH-1);
        if (!strcmp(cmd_str, "history"))
        {
            cle_show_history();
            return -1;
        }
    }
    return 0;
}
char *cle_get_last_word(char *cmd_str)
{
    char *cache_pos=NULL;
    int i;
    if (last_item_idx!=DEFAULT_ITEM_IDX)
    {
        //FLUSH_STR2("cle_get_last_word:last_keyword=%s\n", last_keyword)
        return last_keyword;
    }
    if (strlen(cmd_str)>=1)
    {
        for(i=strlen(cmd_str)-1; (cmd_str[i]!=' '&&i>0); i--)
           cache_pos=&cmd_str[i];

        if (cache_pos!=NULL)
           strncpy(last_keyword,cache_pos,sizeof(last_keyword));
        last_cache_pos=cache_pos;
    }
    return cache_pos;
}
void cle_tab_fetch_str(unsigned int *str_len, char *cmd_str, unsigned int *str_pos, unsigned int *scr_pos, unsigned int *cmd_modified)
{
    char *cache_pos, keyword[COMMAND_TAB_KEYWORD_MAX_LEN];
    int i;
    bzero(keyword, sizeof(keyword));
    if ((strchr(cmd_str, ' ')==NULL)||!keyed_after_tab) // no space entered->search cmd
    {
        keyed_after_tab=0;
        if (strlen(last_keyword))
        {
          if (strstr(cmd_str, last_keyword)!=NULL) //find original typed keyword
            strncpy(keyword, last_keyword, sizeof(keyword));
        }
        else
        {
            strncpy(keyword, cmd_str, sizeof(keyword));
            strncpy(last_keyword, keyword, sizeof(last_keyword));
        }
        for (i=0;i<total_text_items;i++)
           if (toupper(Text_Menu[i].cmd[0])==toupper(keyword[0]))
            if (strcasestr(Text_Menu[i].cmd,keyword)!=NULL) //found keyword in command set
             if (strcasecmp(Text_Menu[i].cmd,last_item)) //seek different item
             {
                if (i<last_item_idx)
                  continue;
                else
                  last_item_idx=i;
                strncpy(last_item, Text_Menu[i].cmd, sizeof(last_item));
                strncpy(cmd_str, last_item, CONSOLE_COMMAND_MAX_LENGTH);
                if ((strlen(Text_Menu[i].op))||(strlen(Text_Menu[i].desc)))
                  strncat(cmd_str, " ", CONSOLE_COMMAND_MAX_LENGTH); //append space if having parameter
                *str_len=strlen(cmd_str);
                cle_clean_line(*scr_pos);
                cle_print_str(*str_len, cmd_str, str_pos, scr_pos, 0);
                *cmd_modified=1;
                return;
             }
        if (last_item_idx!=DEFAULT_ITEM_IDX)
            last_item_idx=DEFAULT_ITEM_IDX-1; //force used orig last_keyword in cle_get_last_word()
    }
    else if ((cache_pos=cle_get_last_word(cmd_str))!=NULL)
    {
        strncpy(keyword, cache_pos, sizeof(keyword));
        for (i=0;i<total_text_items;i++)
          if (toupper(Text_Menu[i].cmd[0])==toupper(cmd_str[0])) //filter by cmd first
           if (toupper(Text_Menu[i].op[0])==toupper(keyword[0]))
            if (strcasestr(Text_Menu[i].op,keyword)!=NULL) //found keyword in op set
             if (strcasecmp(Text_Menu[i].op,last_item)) //seek different item
             {
                if (i<last_item_idx)
                  continue;
                else
                  last_item_idx=i;
                strncpy(last_item, Text_Menu[i].op,sizeof(last_item));
                strncpy(keyword, last_item,sizeof(keyword));
                *last_cache_pos='\0';
                strncat(cmd_str, keyword, CONSOLE_COMMAND_MAX_LENGTH);
                if (strlen(Text_Menu[i].desc))
                  strncat(cmd_str, " ", CONSOLE_COMMAND_MAX_LENGTH); //append space if having parameter
                *str_len=strlen(cmd_str);
                cle_clean_line(*scr_pos);
                cle_print_str(*str_len, cmd_str, str_pos, scr_pos, 0);
                *cmd_modified=1;
                return;
             }
        if ((last_item_idx<0)&&(last_item_idx_generic==DEFAULT_ITEM_IDX)) // not found in cmd's ops
            last_item_idx_generic=DEFAULT_ITEM_IDX-1; //force used orig last_keyword in cle_get_last_word()
        last_item_idx=DEFAULT_ITEM_IDX-1; //force used orig last_keyword in cle_get_last_word()
        //FLUSH_STR2("\nkeyword=%s\n", keyword)
        //FLUSH_STR2("last_item=%s\n", last_item)
        //FLUSH_STR2("last_item_idx=%d\n", last_item_idx)
        int generic_ops_count=sizeof(generic_ops)/sizeof(generic_ops[0]);
        for (i=0; i<generic_ops_count; i++)
           if (toupper(generic_ops[i].op[0])==toupper(keyword[0]))
            if (strcasestr(generic_ops[i].op,keyword)!=NULL) //found keyword in op set
             if (strcasecmp(generic_ops[i].op,last_item)) //seek different item
             {
                //FLUSH_STR2("generic_ops[i].op=%s\n", generic_ops[i].op)
                //FLUSH_STR2("i=%d\n", i)
                if (i<last_item_idx_generic)
                  continue;
                else
                  last_item_idx_generic=i;
                strncpy(last_item, generic_ops[i].op,sizeof(last_item));
                strncpy(keyword, last_item,sizeof(keyword));
                *last_cache_pos='\0';
                strncat(cmd_str, keyword, CONSOLE_COMMAND_MAX_LENGTH);
                *str_len=strlen(cmd_str);
                cle_clean_line(*scr_pos);
                cle_print_str(*str_len, cmd_str, str_pos, scr_pos, 0);
                *cmd_modified=1;
                return;
             }
        if (last_item_idx!=DEFAULT_ITEM_IDX)
            last_item_idx=DEFAULT_ITEM_IDX-1; //force used orig last_keyword in cle_get_last_word()
        if (last_item_idx_generic!=DEFAULT_ITEM_IDX)
            last_item_idx_generic=DEFAULT_ITEM_IDX-1; //force used orig last_keyword in cle_get_last_word()
    }
    return;
}
void cle_tab_fetch_reset(void)
{
    bzero(last_keyword, sizeof(last_keyword));
    bzero(last_item, sizeof(last_item));
    last_item_idx=DEFAULT_ITEM_IDX;
    last_item_idx_generic=DEFAULT_ITEM_IDX;
}
int command_line_edit(char *cmd_str)
{
	unsigned char ch, ch2;
    unsigned int str_len=0, scr_pos=0, str_pos=0, cmd_modified=0;
    cle_tab_fetch_reset();
    last_cache_pos=NULL;
    bzero(cmd_str, CONSOLE_COMMAND_MAX_LENGTH);
    keyed_after_tab=0;
    do
    {
        read(0, &ch, 1);
        if (ch==ESC)                     //ESC+char->control char
        {
            read(0, &ch, 1);
            if (ch == '[' || ch == 'O')
    		    read(0, &ch, 1);
            if (ch >= '1' && ch <= '9')
            {
    		    read(0, &ch2, 1);
            	if (ch2 != '~')
                    ch = 0;
            }
            switch (ch) 
            {
                case 'A':
                    cle_up_arrow(&str_len, cmd_str, &str_pos, &scr_pos);
                    break;
                case 'B':
                    cle_down_arrow(&str_len, cmd_str, &str_pos, &scr_pos);
                    break;
                case 'C':
                    cle_right_arrow(str_len, cmd_str, &str_pos, &scr_pos);
                    break;
                case 'D':
                    cle_left_arrow(&str_pos, &scr_pos);
                    break;
                default:
                    //FLUSH_STR2("ESC:'%c'\n", ch);
                    break;
            }
        }
        else if (ch==3)                  //ctrl+c
        	exit_program();
        else if (ch==1)                  //ctrl+a
            cle_start_of_line(&str_pos, &scr_pos);
        else if (ch==5)                  //ctrl+e
        {
            cle_start_of_line(&str_pos, &scr_pos);
            cle_print_str(str_len, cmd_str, &str_pos, &scr_pos, 1);
        }
        else if (ch==22)                 //PageDown
        {
            cle_start_of_line(&str_pos, &scr_pos);
            cle_print_str(str_len, cmd_str, &str_pos, &scr_pos, 1);
        }
        else if (ch=='\b' || ch==DEL)    //backspace
        {
            cle_backspace(&str_len, cmd_str, &str_pos, &scr_pos, &cmd_modified);
            last_item_idx=DEFAULT_ITEM_IDX;
            cle_tab_fetch_reset();
        }
        else if (ch=='\n' || ch=='\r')   //enter
        {
            cle_enter(str_len, cmd_str, &cmd_modified);
            last_item_idx=DEFAULT_ITEM_IDX;
            process_command(cmd_str);
            break;
        }
        //else if (ch=='\t')               //TAB
        //    cle_tab_fetch_str(&str_len, cmd_str, &str_pos, &scr_pos, &cmd_modified);
        //else if (ch>=PRINTABLE_ASCII_MIN && ch<=PRINTABLE_ASCII_MAX) //other printable char
        else if ((ch=='\t')||(ch>=PRINTABLE_ASCII_MIN && ch<=PRINTABLE_ASCII_MAX)) //other cmds for zigbee
        {
            cle_cmd_add_chr(&str_len, cmd_str, &str_pos, &scr_pos, &cmd_modified, ch);
            last_item_idx=DEFAULT_ITEM_IDX;
            cle_tab_fetch_reset();
        }
    } while (str_len< (CONSOLE_COMMAND_MAX_LENGTH-1));

    if (!strcmp(cmd_str, CMD_COMMAND_LINE_HISTORY))
    {
        cle_show_history();
        return -1;
    }
    else if (cmd_str[0]==CHAR_EXECUTE_HISTORY_NUM)
    {
        return cle_exec_history(cmd_str);
    }
    return 0;
}

