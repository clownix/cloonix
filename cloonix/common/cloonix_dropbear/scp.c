/*
 * scp - secure remote copy.  This is basically patched BSD rcp which
 * uses ssh to do the data transfer (instead of using rcmd).
 *
 */

#include "includes.h"
/*RCSID("$OpenBSD: scp.c,v 1.130 2006/01/31 10:35:43 djm Exp $");*/

#include "atomicio.h"
#include "compat.h"
#include "scpmisc.h"
#include "progressmeter.h"


#define KOUT(format, a...)                                 \
 do {                                                      \
    printf("\n%s %s line:%d   " format,            \
    __FILE__,__FUNCTION__,__LINE__, ## a);                 \
    syslog(LOG_ERR, "%s %s line:%d   " format, \
    __FILE__,__FUNCTION__,__LINE__, ## a);                 \
    exit(-1);                                              \
    } while (0)

#define KERR(format, a...)                                 \
 do {                                                      \
    printf("\n%s %s line:%d   " format,            \
    __FILE__,__FUNCTION__,__LINE__, ## a);                 \
    syslog(LOG_ERR, "%s %s line:%d   " format, \
    __FILE__,__FUNCTION__,__LINE__, ## a);                 \
    } while (0)




void bwlimit(int);

/* Struct for addargs */
arglist args;

/* Bandwidth limit */
off_t limit_rate = 0;

/* Name of current file being transferred. */
char *curfile;

/* This is set to non-zero to enable verbose mode. */
int verbose_mode = 0;

/* This is set to zero if the progressmeter is not desired. */
int showprogress = 1;

char *ssh_program;

/* This is used to store the pid of ssh_program */
pid_t pid_cmd_do = -1;


static void
killchild(int signo)
{
	if (pid_cmd_do > 1) {
		kill(pid_cmd_do, signo ? signo : SIGTERM);
		waitpid(pid_cmd_do, NULL, 0);
	}

KOUT("%d", signo);
}

static int
do_local_cmd(arglist *a)
{
	int i;
	int status;
	pid_t pid;

	if (a->num == 0)
		fatal("do_local_cmd: no arguments");

	if (verbose_mode) {
		fprintf(stderr, "Executing:");
		for (i = 0; i < a->num; i++)
			fprintf(stderr, " %s", a->list[i]);
		fprintf(stderr, "\n");
	}
	pid = fork();
	if (pid == -1)
		fatal("do_local_cmd: fork: %s", strerror(errno));

	if (pid == 0) {
		execvp(a->list[0], a->list);
                KOUT("ERROR execvp");
		perror(a->list[0]);
KOUT(" ");
	}

	pid_cmd_do = pid;
	signal(SIGTERM, killchild);
	signal(SIGINT, killchild);
	signal(SIGHUP, killchild);

	while (waitpid(pid, &status, 0) == -1)
		if ((errno != EINTR) && (errno != EAGAIN))
			fatal("do_local_cmd: waitpid: %s", strerror(errno));

	pid_cmd_do = -1;

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return (-1);

	return (0);
}

/*
 * This function executes the given command as the specified user on the
 * given host.  This returns < 0 if execution fails, and >= 0 otherwise. This
 * assigns the input and output file descriptors on success.
 */

static void
arg_setup(char *host, char *cmd)
{
	replacearg(&args, 0, "%s", ssh_program);
	addargs(&args, "%s", host);
	addargs(&args, "%s", cmd);
}

static int
do_cmd(char *host, char *cmd, int *fdin, int *fdout)
{
	int pin[2], pout[2], reserved[2];

	if (verbose_mode)
		fprintf(stderr,
		    "Executing: program %s host %s, command %s\n",
		    ssh_program, host, cmd);

	/*
	 * Reserve two descriptors so that the real pipes won't get
	 * descriptors 0 and 1 because that will screw up dup2 below.
	 */
	pipe(reserved);

	/* Create a socket pair for communicating with ssh. */
	if (pipe(pin) < 0)
		fatal("pipe: %s", strerror(errno));
	if (pipe(pout) < 0)
		fatal("pipe: %s", strerror(errno));

	/* Free the reserved descriptors. */
	close(reserved[0]);
	close(reserved[1]);

	/* uClinux needs to build the args here before vforking,
	   otherwise we do it later on. */
	pid_cmd_do = fork();
	if (pid_cmd_do == 0) {
		/* Child. */
		close(pin[1]);
		close(pout[0]);
		dup2(pin[0], 0);
		dup2(pout[1], 1);
		close(pin[0]);
		close(pout[1]);
		arg_setup(host, cmd);
		execvp(ssh_program, args.list);
                KOUT("ERROR execvp");
		perror(ssh_program);
KOUT(" ");
	} else if (pid_cmd_do == -1) {
		fatal("fork: %s", strerror(errno));
	}

	/* Parent.  Close the other side, and return the local side. */
	close(pin[0]);
	*fdout = pin[1];
	close(pout[1]);
	*fdin = pout[0];
	signal(SIGTERM, killchild);
	signal(SIGINT, killchild);
	signal(SIGHUP, killchild);
	return 0;
}

typedef struct {
	size_t cnt;
	char *buf;
} BUF;

BUF *allocbuf(BUF *, int);
void lostconn(int);
void nospace(void);
void run_err(const char *,...);
void verifydir(char *);

uid_t userid;
int errs, remin, remout;
int pflag, iamremote, iamrecursive, targetshouldbedirectory;

#define	CMDNEEDS	64
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */

int response(void);
void rsource(char *, struct stat *);
void sink(int, char *[]);
void source(int, char *[]);
void tolocal(int, char *[]);
void toremote(char *, int, char *[]);
void usage(int line);

#define MAX_PATH_LEN 256
static char g_path_to_scp_ssh[MAX_PATH_LEN];

int
main(int argc, char **argv)
{
	int ch, fflag, tflag, status;
	double speed;
	char *targ, *endp, *ptr;
	extern char *optarg;
	extern int optind;

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
memset(g_path_to_scp_ssh, 0, MAX_PATH_LEN);
strncpy(g_path_to_scp_ssh, argv[0], MAX_PATH_LEN-1);
ptr = strstr(g_path_to_scp_ssh, "cloonix-dropbear-scp");
if (!ptr)
KOUT("%s", g_path_to_scp_ssh);
strcpy(ptr, "cloonix-dropbear-ssh");
ssh_program = g_path_to_scp_ssh;

	sanitise_stdfd();


	memset(&args, '\0', sizeof(args));
	args.list = NULL;
	addargs(&args, "%s", ssh_program);

        addargs(&args, "%s", argv[1]);
        argc--;
        argv++;

        addargs(&args, "%s", argv[1]);
        argc--;
        argv++;

        addargs(&args, "-Z");

	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, "dfl:prtvBCc:i:P:q1246S:o:F:")) != -1)
		switch (ch) {
		/* User-visible flags. */
		case '1':
		case '2':
		case '4':
		case '6':
		case 'C':
			addargs(&args, "-%c", ch);
			break;
		case 'o':
		case 'c':
		case 'i':
		case 'F':
			addargs(&args, "-%c%s", ch, optarg);
			break;
		case 'P':
			addargs(&args, "-p%s", optarg);
			break;
		case 'B':
			fprintf(stderr, "Note: -B option is disabled in this version of scp");
			break;
		case 'l':
			speed = strtod(optarg, &endp);
			if (speed <= 0 || *endp != '\0')
				usage(__LINE__);
			limit_rate = speed * 1024;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			iamrecursive = 1;
			break;
		case 'v':
			addargs(&args, "-v");
			verbose_mode = 1;
			break;
		case 'q':
			addargs(&args, "-q");
			showprogress = 0;
			break;

		/* Server options. */
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':	/* "from" */
			iamremote = 1;
			fflag = 1;
			break;
		case 't':	/* "to" */
			iamremote = 1;
			tflag = 1;
			break;
		default:
			usage(__LINE__);
		}
	argc -= optind;
	argv += optind;

	if (!isatty(STDERR_FILENO))
		showprogress = 0;

	remin = STDIN_FILENO;
	remout = STDOUT_FILENO;

	if (fflag) {
		/* Follow "protocol", send data. */
		(void) response();
		source(argc, argv);
		KOUT("%d", errs);
	}
	if (tflag) {
		/* Receive data. */
		sink(argc, argv);
		KOUT("%d", errs);
	}
	if (argc < 2)
		usage(__LINE__);
	if (argc > 2)
		targetshouldbedirectory = 1;

	remin = remout = -1;
	pid_cmd_do = -1;
	/* Command to be executed on remote system using "ssh". */
	(void) snprintf(cmd, sizeof cmd, "/cloonixmnt/cnf_fs/cloonix-scp%s%s%s%s",
	    verbose_mode ? " -v" : "",
	    iamrecursive ? " -r" : "", pflag ? " -p" : "",
	    targetshouldbedirectory ? " -d" : "");

	(void) signal(SIGPIPE, lostconn);

	if ((targ = colon(argv[argc - 1])))	/* Dest is remote host. */
		toremote(targ, argc, argv);
	else {
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
		tolocal(argc, argv);	/* Dest is local host. */
	}
	if (pid_cmd_do != -1 && errs == 0) {
		if (remin != -1)
		    (void) close(remin);
		if (remout != -1)
		    (void) close(remout);
		if (waitpid(pid_cmd_do, &status, 0) == -1)
			errs = 1;
		else {
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
				errs = 1;
		}
	}
return 0;
}

void
toremote(char *targ, int argc, char **argv)
{
	int i, len;
	char *bp, *host, *src, *thost, *arg;
	arglist alist;

	memset(&alist, '\0', sizeof(alist));
	alist.list = NULL;

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	arg = xstrdup(argv[argc - 1]);
	if ((thost = strrchr(arg, '@'))) {
		*thost++ = 0;
	} else {
		thost = arg;
	}


	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {	/* remote to remote */
			freeargs(&alist);
			addargs(&alist, "%s", ssh_program);
			if (verbose_mode)
				addargs(&alist, "-v");
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = strrchr(argv[i], '@');
			if (host) {
				*host++ = 0;
				host = cleanhostname(host);
			} else {
				host = cleanhostname(argv[i]);
 
			}
			addargs(&alist, "%s", host);
			addargs(&alist, "%s", cmd);
			addargs(&alist, "%s", src);
			addargs(&alist, "%s:%s", thost, targ);
			if (do_local_cmd(&alist) != 0)
				errs = 1;
		} else {	/* local to remote */
			if (remin == -1) {
				len = strlen(targ) + CMDNEEDS + 20;
				bp = xmalloc(len);
				(void) snprintf(bp, len, "%s -t %s", cmd, targ);
				host = cleanhostname(thost);
				if (do_cmd(host, bp, &remin,
				    &remout) < 0)
KOUT(" ");
				if (response() < 0)
KOUT(" ");
				(void) xfree(bp);
			}
			source(1, argv + i);
		}
	}
}

void
tolocal(int argc, char **argv)
{
	int i, len;
	char *bp, *host, *src;
	arglist alist;

	memset(&alist, '\0', sizeof(alist));
	alist.list = NULL;

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {	/* Local to local. */
			freeargs(&alist);
			addargs(&alist, "%s", _PATH_CP);
			if (iamrecursive)
				addargs(&alist, "-r");
			if (pflag)
				addargs(&alist, "-p");
			addargs(&alist, "%s", argv[i]);
			addargs(&alist, "%s", argv[argc-1]);
			if (do_local_cmd(&alist))
				++errs;
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		if ((host = strrchr(argv[i], '@')) == NULL) {
			host = argv[i];
		} else {
			*host++ = 0;
		}
		host = cleanhostname(host);
		len = strlen(src) + CMDNEEDS + 20;
		bp = xmalloc(len);
		(void) snprintf(bp, len, "%s -f %s", cmd, src);
		if (do_cmd(host, bp, &remin, &remout) < 0) {
			(void) xfree(bp);
			++errs;
			continue;
		}
		xfree(bp);
		sink(1, argv + argc - 1);
		(void) close(remin);
		remin = remout = -1;
	}
}

void
source(int argc, char **argv)
{
	struct stat stb;
	static BUF buffer;
	BUF *bp;
	off_t i, amt, statbytes;
	size_t result;
	int fd = -1, haderr, indx;
	char *last, *name, buf[2048];
	int len;

	for (indx = 0; indx < argc; ++indx) {
		name = argv[indx];
		statbytes = 0;
		len = strlen(name);
		while (len > 1 && name[len-1] == '/')
			name[--len] = '\0';
		if (strchr(name, '\n') != NULL) {
			run_err("%s: skipping, filename contains a newline",
			    name);
			goto next;
		}
		if ((fd = open(name, O_RDONLY, 0)) < 0)
			goto syserr;
		if (fstat(fd, &stb) < 0) {
syserr:			run_err("%s: %s", name, strerror(errno));
			goto next;
		}
		switch (stb.st_mode & S_IFMT) {
		case S_IFREG:
			break;
		case S_IFDIR:
			if (iamrecursive) {
				rsource(name, &stb);
				goto next;
			}
			/* FALLTHROUGH */
		default:
			run_err("%s: not a regular file", name);
			goto next;
		}
		if ((last = strrchr(name, '/')) == NULL)
			last = name;
		else
			++last;
		curfile = last;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void) snprintf(buf, sizeof buf, "T%lu 0 %lu 0\n",
			    (u_long) stb.st_mtime,
			    (u_long) stb.st_atime);
			(void) atomicio(vwrite, remout, buf, strlen(buf));
			if (response() < 0)
				goto next;
		}
#define	FILEMODEMASK	(S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO)
		snprintf(buf, sizeof buf, "C%04o %lld %s\n",
		    (u_int) (stb.st_mode & FILEMODEMASK),
		    (long long)stb.st_size, last);
		if (verbose_mode) {
			fprintf(stderr, "Sending file modes: %s", buf);
		}
		(void) atomicio(vwrite, remout, buf, strlen(buf));
		if (response() < 0)
			goto next;
		if ((bp = allocbuf(&buffer,  2048)) == NULL) {
next:			if (fd != -1) {
				(void) close(fd);
				fd = -1;
			}
			continue;
		}
		if (showprogress)
			start_progress_meter(curfile, stb.st_size, &statbytes);
		/* Keep writing after an error so that we stay sync'd up. */
		for (haderr = i = 0; i < stb.st_size; i += bp->cnt) {
			amt = bp->cnt;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (!haderr) {
				result = atomicio(read, fd, bp->buf, amt);
				if (result != (size_t)amt)
					haderr = errno;
			}
			if (haderr)
				(void) atomicio(vwrite, remout, bp->buf, amt);
			else {
				result = atomicio(vwrite, remout, bp->buf, amt);
				if (result != (size_t)amt)
					haderr = errno;
				statbytes += result;
			}
			if (limit_rate)
				bwlimit(amt);
		}
		if (showprogress)
			stop_progress_meter();

		if (fd != -1) {
			if (close(fd) < 0 && !haderr)
				haderr = errno;
			fd = -1;
		}
		if (!haderr)
			(void) atomicio(vwrite, remout, "", 1);
		else
			run_err("%s: %s", name, strerror(haderr));
		(void) response();
	}
}

void
rsource(char *name, struct stat *statp)
{
	DIR *dirp;
	struct dirent *dp;
	char *last, *vect[1], path[1100];

	if (!(dirp = opendir(name))) {
		run_err("%s: %s", name, strerror(errno));
		return;
	}
	last = strrchr(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void) snprintf(path, sizeof(path), "T%lu 0 %lu 0\n",
		    (u_long) statp->st_mtime,
		    (u_long) statp->st_atime);
		(void) atomicio(vwrite, remout, path, strlen(path));
		if (response() < 0) {
			closedir(dirp);
			return;
		}
	}
	(void) snprintf(path, sizeof path, "D%04o %d %.1024s\n",
	    (u_int) (statp->st_mode & FILEMODEMASK), 0, last);
	if (verbose_mode)
		fprintf(stderr, "Entering directory: %s", path);
	(void) atomicio(vwrite, remout, path, strlen(path));
	if (response() < 0) {
		closedir(dirp);
		return;
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= sizeof(path) - 1) {
			run_err("%s/%s: name too long", name, dp->d_name);
			continue;
		}
		(void) snprintf(path, sizeof path, "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	(void) closedir(dirp);
	(void) atomicio(vwrite, remout, "E\n", 2);
	(void) response();
}

void
bwlimit(int amount)
{
	static struct timeval bwstart, bwend;
	static int lamt, thresh = 16384;
	uint64_t waitlen;
	struct timespec ts, rm;

	if (!timerisset(&bwstart)) {
		gettimeofday(&bwstart, NULL);
		return;
	}

	lamt += amount;
	if (lamt < thresh)
		return;

	gettimeofday(&bwend, NULL);
	timersub(&bwend, &bwstart, &bwend);
	if (!timerisset(&bwend))
		return;

	lamt *= 8;
	waitlen = (double)1000000L * lamt / limit_rate;

	bwstart.tv_sec = waitlen / 1000000L;
	bwstart.tv_usec = waitlen % 1000000L;

	if (timercmp(&bwstart, &bwend, >)) {
		timersub(&bwstart, &bwend, &bwend);

		/* Adjust the wait time */
		if (bwend.tv_sec) {
			thresh /= 2;
			if (thresh < 2048)
				thresh = 2048;
		} else if (bwend.tv_usec < 100) {
			thresh *= 2;
			if (thresh > 32768)
				thresh = 32768;
		}

		TIMEVAL_TO_TIMESPEC(&bwend, &ts);
		while (nanosleep(&ts, &rm) == -1) {
		        if ((errno != EINTR) && (errno != EAGAIN))
				break;
			ts = rm;
		}
	}

	lamt = 0;
	gettimeofday(&bwstart, NULL);
}

void
sink(int argc, char **argv)
{
	static BUF buffer;
	struct stat stb;
	enum {
		YES, NO, DISPLAYED
	} wrerr;
	BUF *bp;
	off_t i;
	size_t j, count;
	int amt, exists, first, mode, ofd, omode;
	off_t size, statbytes;
	int setimes, targisdir, wrerrno = 0;
	char ch, *cp, *np, *targ, *why, *vect[1], buf[2048];
	struct timeval tv[2];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

	setimes = targisdir = 0;
	if (argc != 1) {
		run_err("ambiguous target");
KOUT(" ");
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);

	(void) atomicio(vwrite, remout, "", 1);
	if (stat(targ, &stb) == 0 && S_ISDIR(stb.st_mode))
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;
		if (atomicio(read, remin, cp, 1) != 1)
			return;
		if (*cp++ == '\n')
                  {
                  exit(0);
                  }
		do {
			if (atomicio(read, remin, &ch, sizeof(ch)) != sizeof(ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[sizeof(buf) - 1] && ch != '\n');
		*cp = 0;
		if (verbose_mode)
			fprintf(stderr, "Sink: %s", buf);

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void) atomicio(vwrite, STDERR_FILENO,
				    buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
KOUT(" ");
			++errs;
			continue;
		}
		if (buf[0] == 'E') {
			(void) atomicio(vwrite, remout, "", 1);
			return;
		}
		if (ch == '\n')
			*--cp = 0;

		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			mtime.tv_sec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			mtime.tv_usec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			atime.tv_sec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			atime.tv_usec = strtol(cp, &cp, 10);
			if (!cp || *cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			(void) atomicio(vwrite, remout, "", 1);
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				run_err("%s", cp);
KOUT(" ");
			}
KERR("%s", "TOOCHECK: expected control record");
exit(0);
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");

		for (size = 0; isdigit(*cp);)
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if ((strchr(cp, '/') != NULL) || (strcmp(cp, "..") == 0)) {
			run_err("error: unexpected filename: %s", cp);
KOUT(" ");
		}
		if (targisdir) {
			static char *namebuf;
			static size_t cursize;
			size_t need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (namebuf)
					xfree(namebuf);
				namebuf = xmalloc(need);
				cursize = need;
			}
			(void) snprintf(namebuf, need, "%s%s%s", targ,
			    strcmp(targ, "/") ? "/" : "", cp);
			np = namebuf;
		} else
			np = targ;
		curfile = cp;
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			int mod_flag = pflag;
			if (!iamrecursive)
				SCREWUP("received directory without -r");
			if (exists) {
				if (!S_ISDIR(stb.st_mode)) {
					errno = ENOTDIR;
					goto bad;
				}
				if (pflag)
					(void) chmod(np, mode);
			} else {
				/* Handle copying from a read-only
				   directory */
				mod_flag = 1;
				if (mkdir(np, mode | S_IRWXU) < 0)
					goto bad;
			}
			vect[0] = xstrdup(np);
			sink(1, vect);
			if (setimes) {
				setimes = 0;
				if (utimes(vect[0], tv) < 0)
					run_err("%s: set times: %s",
					    vect[0], strerror(errno));
			}
			if (mod_flag)
				(void) chmod(vect[0], mode);
			if (vect[0])
				xfree(vect[0]);
			continue;
		}
		omode = mode;
		mode |= S_IWRITE;
		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:			run_err("%s: %s", np, strerror(errno));
			continue;
		}
		(void) atomicio(vwrite, remout, "", 1);
		if ((bp = allocbuf(&buffer,  4096)) == NULL) {
			(void) close(ofd);
			continue;
		}
		cp = bp->buf;
		wrerr = NO;

		statbytes = 0;
		if (showprogress)
			start_progress_meter(curfile, size, &statbytes);
		for (count = i = 0; i < size; i += 4096) {
			amt = 4096;
			if (i + amt > size)
				amt = size - i;
			count += amt;
			do {
				j = atomicio(read, remin, cp, amt);
				if (j == 0) {
					run_err("%s", j ? strerror(errno) :
					    "dropped connection");
KOUT(" ");
				}
				amt -= j;
				cp += j;
				statbytes += j;
			} while (amt > 0);

			if (limit_rate)
				bwlimit(4096);

			if (count == bp->cnt) {
				/* Keep reading so we stay sync'd up. */
				if (wrerr == NO) {
					if (atomicio(vwrite, ofd, 
                                            bp->buf, count) != (ssize_t)count) {
						wrerr = YES;
						wrerrno = errno;
					}
				}
				count = 0;
				cp = bp->buf;
			}
		}
		if (showprogress)
			stop_progress_meter();
		if (count != 0 && wrerr == NO &&
		    atomicio(vwrite, ofd, bp->buf, count) != (ssize_t)count) {
			wrerr = YES;
			wrerrno = errno;
		}
		if (wrerr == NO && ftruncate(ofd, size) != 0) {
			run_err("%s: truncate: %s", np, strerror(errno));
			wrerr = DISPLAYED;
		}
		if (pflag) {
			if (exists || omode != mode)
				if (chmod(np, omode)) {
					run_err("%s: set mode: %s",
					    np, strerror(errno));
					wrerr = DISPLAYED;
				}
		} else {
			if (!exists && omode != mode)
				if (chmod(np, omode)) {
					run_err("%s: set mode: %s",
					    np, strerror(errno));
					wrerr = DISPLAYED;
				}
		}
		if (close(ofd) == -1) {
			wrerr = YES;
			wrerrno = errno;
		}
		(void) response();
		if (setimes && wrerr == NO) {
			setimes = 0;
			if (utimes(np, tv) < 0) {
				run_err("%s: set times: %s",
				    np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
		switch (wrerr) {
		case YES:
			run_err("%s: %s", np, strerror(wrerrno));
			break;
		case NO:
			(void) atomicio(vwrite, remout, "", 1);
			break;
		case DISPLAYED:
			break;
		}
	}
screwup:
	run_err("protocol error: %s", why);
KOUT(" ");
}

int
response(void)
{
	char ch, *cp, resp, rbuf[2048];

	if (atomicio(read, remin, &resp, sizeof(resp)) != sizeof(resp))
		lostconn(0);

	cp = rbuf;
	switch (resp) {
	case 0:		/* ok */
		return (0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:		/* error, followed by error msg */
	case 2:		/* fatal error, "" */
		do {
			if (atomicio(read, remin, &ch, sizeof(ch)) != sizeof(ch))
				lostconn(0);
			*cp++ = ch;
		} while (cp < &rbuf[sizeof(rbuf) - 1] && ch != '\n');

		if (!iamremote)
			(void) atomicio(vwrite, STDERR_FILENO, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return (-1);
KOUT(" ");
	}
	/* NOTREACHED */
}

void
usage(int line)
{
	(void) fprintf(stderr,"\n\n"
	    "usage: cloonix_scp <network_name> [-r] <source> <destination> \n\n"
	    "source and destination are either\n"
	    " - a host file (or dir) path\n"
	    " - <cloonix_name>:<path> \n\n");
KOUT("%d", line);
}

void
run_err(const char *fmt,...)
{
	static FILE *fp;
	va_list ap;

	++errs;
	if (fp == NULL && !(fp = fdopen(remout, "w")))
		return;
	(void) fprintf(fp, "%c", 0x01);
	(void) fprintf(fp, "scp: ");
	va_start(ap, fmt);
	(void) vfprintf(fp, fmt, ap);
	va_end(ap);
	(void) fprintf(fp, "\n");
	(void) fflush(fp);

	if (!iamremote) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fprintf(stderr, "\n");
	}
}

void
verifydir(char *cp)
{
	struct stat stb;

	if (!stat(cp, &stb)) {
		if (S_ISDIR(stb.st_mode))
			return;
		errno = ENOTDIR;
	}
	run_err("%s: %s", cp, strerror(errno));
	killchild(0);
}


BUF *
allocbuf(BUF *bp, int blksize)
{
	size_t size;
	size = blksize;
	if (bp->cnt >= size)
		return (bp);
	if (bp->buf == NULL)
		bp->buf = xmalloc(size);
	else
		bp->buf = xrealloc(bp->buf, size);
	memset(bp->buf, 0, size);
	bp->cnt = size;
	return (bp);
}

void
lostconn(int signo)
{
	if (!iamremote)
		write(STDERR_FILENO, "lost connection\n", 16);
KOUT("%d", signo);
}
