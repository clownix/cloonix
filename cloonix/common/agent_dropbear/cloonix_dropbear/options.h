#ifndef _OPTIONS_H_
#define _OPTIONS_H_
#define LOCAL_IDENT "SSH-2.0-dropbear_2015.67.cloonix"
#define PROGNAME "dropbear"
#define UNIX_DROPBEAR_SOCK "/tmp/unix_cloonix_dropbear_sock"
#define PATH_DROPBEAR_PID "/var/run/dropbear.pid"

#define MAX_DROPBEAR_PATH_LEN 300
#define MAX_UNAUTH_PER_IP 5
#define MAX_UNAUTH_CLIENTS 30
#define MAX_AUTH_TRIES 10
#define XAUTH_COMMAND "/usr/bin/xauth -q"

#define TRANS_MAX_PAYLOAD_LEN 500000
#define DEFAULT_RECV_WINDOW  4000000
#define RECV_MAX_PAYLOAD_LEN 4500000
#define RECV_MAX_PACKET_LEN  4510000 
#define RECV_WINDOWEXTEND     200000

#define DEFAULT_PATH "/sbin:/usr/sbin:/bin:/usr/bin"

#define MAX_BANNER_SIZE 2000 /* this is 25*80 chars, any more is foolish */
#define MAX_BANNER_LINES 20 /* How many lines the client will display */
#define MAX_CMD_LEN 4096 /* max length of a command */
#define MAX_TERM_LEN 200 /* max length of TERM name */
#define MAX_HOST_LEN 254 /* max hostname len for tcp fwding */
#define MAX_IP_LEN 15 /* strlen("255.255.255.255") == 15 */
#define _PATH_TTY "/dev/tty"
#define _PATH_CP "/bin/cp"
#define DROPBEAR_ESCAPE_CHAR '~'
#define DROPBEAR_SUCCESS 0
#define DROPBEAR_FAILURE -1
#define MIN_PACKET_LEN 16
#define MAX_CHANNELS 50
#define MAX_STRING_LEN 5000
#define DROPBEAR_MAX_SOCKS 2 
#define IS_DROPBEAR_SERVER (ses.isserver == 1)
#define IS_DROPBEAR_CLIENT (ses.isserver == 0)

#endif /* _OPTIONS_H_ */
