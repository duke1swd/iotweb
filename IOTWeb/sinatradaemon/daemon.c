/*
 * This program runs a Ruby/Sinatra web server as a daemon
 * It is called from /etc/init.d/myweb
 *
 * This program is called with user:group set to 0:0 and with
 * the option -c configfile.
 *
 * It reads the config file, validates, then forks the daemon into
 * the background.  The parent creates the PIDFILE and exits.
 * The child sets uid:gid to specified uid:gid before running the web service
 * The path is also set up so that rbenv stuff works.

  The configuration file is a bunch of lines containing variable
  = value.  Comment, blank lines, etc are allowed.

  Some variables are known to this program.  They are listed below.
  Any variable not known to this program is passed to the environment
  of the daemon we spawn.

  Known variables:
	name	default		notes
	====	=======		==========
	uid	1000		switch to this uid before runing the daemon.  Numeric.
	gid	1000		same, but for gid
	rpath	/home/pi/.rbenv/bin:/home/pi/.rbenv/shims
				prepend this to the path so ruby stuff works
	chdir	/home/iotweb	cd here. App is found here
	pidfile	/var/run/iotweb.pid
	app	iotweb.rb	the command line will be "ruby iotweb.rb"
	interp	ruby		the interpreter to run your command.  See also rerun
	logfile	<none>		if none, goes to syslog.
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include "cfgets.h"

char * ds_copy(char *s);

char *myname;
char *conf_file;
FILE *conf;
int verbose;
int debug;

static void
set_defaults()
{
	conf_file = "/etc/iotweb/iotweb.conf";
	verbose = 0;
	debug = 0;
}

static void
usage(){
	set_defaults();

	fprintf(stderr, "Usage: %s <options>\n", myname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-v (verbose)\n");
	fprintf(stderr, "\t-d (debug)\n");
	fprintf(stderr, "\t-c <config file (%s)>\n", conf_file);
	exit(1);
}

static void
grok_args(int argc, char **argv)
{
	int c, errors;
	int nargs;

	myname = *argv;
	errors = 0;
	set_defaults();

	while ((c = getopt(argc, argv, "c:vdh")) != EOF)
	switch (c) {
	    case 'c':
	    	conf_file = optarg;
		break;

	    case 'd':
	    	debug++;
		break;

	    case 'v':
	    	verbose++;
		break;

	    case 'h':
	    case '?':
	    default:
	    	usage();
	}
	
	if (debug)
		fprintf(stderr, "%s: debug = %d\n",
			myname,
			debug);

	conf = fopen(conf_file, "r");
	if (conf == NULL) {
		fprintf(stderr, "%s: cannot open configuration file %s for reading\n",
			myname,
			conf_file);
		errors++;
	}

	nargs = argc - optind;
	if (nargs) {
		fprintf(stderr, "%s: No positional arguments accepted\n",
			myname);
		errors++;
	}

	if (errors)
		usage();
}

static char **local_env;
static int env_size;

static void
load_env_line(char *line)
{
	local_env = (char **)realloc(local_env, (env_size +2) * sizeof (char *));
	if (local_env == NULL) {
		fprintf(stderr, "%s: failed to allocate space for local environment\n",
			myname);
		exit(1);
	}
	local_env[env_size++] = line;
	local_env[env_size] = (char *)0;
}

static void
load_init_environ(char **env)
{
	env_size = 0;
	local_env = (char **)malloc(sizeof (char *));
	if (local_env == NULL) {
		fprintf(stderr, "%s: failed to allocate space for local environment\n",
			myname);
		exit(1);
	}

	while (*env)
		load_env_line(*env++);
}

/*
 * Read and process the configuration file.
 */
struct kv_s {
	char *name;
	char *dflt;
	char *value;
};

static struct kv_s known_variables[] = {
	{ "uid", "1000", (char *)0, }, 
	{ "gid", "1000", (char *)0, }, 
	{ "rpath", "/home/pi/.rbenv/bin:/home/pi/.rbenv/shims", (char *)0 },
	{ "chdir", "/home/iotweb", (char *)0, },
	{ "pidfile", "/var/run/iotweb.pid", (char *)0, },
	{ "app", "iotweb", (char *)0, },
	{ "interp" "ruby", (char *)0, },
	{ "logfile", (char *)0, (char *)0, },
	{ (char *)0, (char *)0, (char *)0, },
};

static void
init_kvs()
{
	struct kv_s *kp;

	for (kp = known_variables; kp->name; kp++)
		kp->value = kp->dflt;
}

static void
process_conf_var(char *name, char *value)
{
	struct kv_s *kp;
	char *p, *q;
	char buf[256];

	// buf is name in lower case
	for (p = buf, q=name; *q; p++, q++)
		if(isupper(*q))
			*p = tolower(*q);
		else
			*p = *q;
	*p = '\0';

	for (kp = known_variables; kp->name; kp++)
		if (strcmp(kp->name, buf) == 0) {
			if (kp->value && kp->value != kp->dflt)
				free(kp->value);
			kp->value = ds_copy(value);
			if (verbose > 1)
				printf("setting internal variable %s to %s\n",
					kp->name, kp->value);
			return;
		}

	// process loading a variable into the environment
	if (strlen(name) + strlen(value) + 1 >= sizeof buf) {
		fprintf(stderr, "%s: env variable %s name + value to big\n",
			myname, name);
		exit(1);
	}
	sprintf(buf, "%s=%s", name, value);
	if (verbose > 1)
		printf("adding %s to env\n", buf);

	load_env_line(ds_copy(buf));
}

static void
configure()
{
	char *var_p, *val_p;
	char *p;
	int lineno;
	char lbuf[256];

	lineno = 0;
	init_kvs();
	while (cfgets(lbuf, sizeof lbuf, conf) == lbuf) {
		lineno++;
		// skip initial whitespace
		for (var_p = lbuf; *var_p && isspace(*var_p); var_p++) ;
		if (!*var_p)
			continue;	// blank line is OK

		// look for end of variable name
		for (p = var_p; *p && !isspace(*p) && *p != '='; p++) ;
		if (!*p)
			goto err;

		// Look for '='
		if (*p == '=') {
			*p++ = '\0';
		} else {
			for (*p++ = '\0' ; *p && isspace(*p); p++) ;
			if (!*p || *p != '=')
				goto err;
			p++;
		}

		// Look for start of value
		for (val_p = p; *val_p && isspace(*val_p); val_p++) ;

		if (!*val_p)
			goto err;

		// Look for end of value, and discard the rest.
		for (p = val_p; *p && !isspace(*p); p++) ;
		*p = '\0';

		process_conf_var(var_p, val_p);
	}
	return;

    err:
    	fprintf(stderr, "%s: badly formed conf file %s line %d\n",
		myname,
		conf_file,
		lineno);
	exit(1);
}

/*
 * Verbose / debugging routine
 */
static void
print_known_variables()
{
	struct kv_s *kp;

	for (kp = known_variables; kp->name; kp++) {
		printf("%10s %20s ",
			kp->name, kp->value);
		if (kp->dflt == kp->value)
			printf("DFLT\n");
		else
			printf("(%s)\n", kp->dflt);
	}
}

/*
 * Process the known variables
 */
static char *
kv_search(char *key)
{
	struct kv_s *kp;

	for (kp = known_variables; kp->name; kp++)
		if (strcmp(kp->name, key) == 0)
			return kp->value;
	fprintf(stderr, "%s: INTERNAL ERROR KV_S(%s)\n",
		myname,
		key);
	exit(1);
}

#define	PATHNAME	"PATH="	// environment holding the path.
#define	PATHLEN		5	// length of the above

static int uid;
static int gid;
static FILE *pidfile;
static char *app;
static int logfile_fd;

static void
process_configuration()
{
	char **ep;
	char *prepend;
	char *chdirname;
	char *pidfilename;
	char *logfilename;
	int r;
	char pathbuf[512];

	uid = atoi(kv_search("uid"));
	gid = atoi(kv_search("gid"));

	/*
	 * This section edits the search path
	 */
	for (ep = local_env; *ep; ep++)
		if (strncmp(*ep, PATHNAME, PATHLEN) == 0) 
			goto found;
	fprintf(stderr, "%s: NO PATH \n", myname);
	exit(1);

    found:
	prepend = kv_search("rpath");
    	if (strlen(prepend) + strlen(*ep) > sizeof pathbuf) {
		fprintf(stderr, "%s total path longer than %d\n",
			myname,
			sizeof pathbuf);
		exit(1);
	}
	sprintf(pathbuf, "%s%s:%s",
		PATHNAME,
		prepend,
		*ep + PATHLEN);
	if (verbose > 1)
		printf("Setting path:\n%s\n", pathbuf);
	*ep = ds_copy(pathbuf);

	/*
	 * This section moves to the right directory
	 */
	chdirname = kv_search("chdir");
	if (chdirname[0] != '/') {
		fprintf(stderr, "%s: path name of working directory (%s) "
				"does not begin with \"/\"\n",
			myname,
			chdirname);
		exit(1);
	}
	r = chdir(chdirname);
	if (r != 0) {
		fprintf(stderr, "%s: cannot chdir(%s)\n",
			myname,
			chdirname);
		perror("chdir");
		exit(1);
	}

	/*
	 * This section creates the PIDFILE and opens it for writing.
	 */
	pidfilename = kv_search("pidfile");
	pidfile = fopen(pidfilename, "w");
	if (pidfile == NULL) {
		fprintf(stderr, "%s: cannot open pidfile %s for writing\n",
			myname,
			pidfilename);
		exit(1);
	}

	/*
	 * This section makes sure that logfile_fd points to something sane.
	 * If not supplied, we used 2.
	 */
	logfile_fd = 2;
	logfilename = kv_search("logfile");
	if (logfilename) {
		logfile_fd = open(logfilename, O_WRONLY | O_CREAT, 0644);
		if (logfile_fd < 0) {
			fprintf(stderr, "%s: cannot open log file %s for writing\n",
				myname,
				logfilename);
			exit(1);
		}
		// seek to end.
		lseek(logfile_fd, (off_t)0, SEEK_END);
		r = write(logfile_fd, "IOTWEB Log File Open\n", 21);
	}
	
	/*
	 * find the application
	 */
	app = kv_search("app");
}


/*
 * fork the child process.
 * after fork, get the right uid/gid.
 */
#define	APP_ARGS	3
static void
doit()
{
	int i;
	int pid;
	char *app_argv[APP_ARGS];
	extern char **environ;

	if (debug)
		pid = 0;
	else
		pid = fork();

	if (pid == -1) {
		// there was an error
		fprintf(stderr, "%s: cannot fork child\n",
			myname);
		perror("fork");
		exit(1);
	} else if (pid == 0) {
		/*
		 * We are the child.
		 *
		 * Mostly ignore errors from this point on.
 		 * we don't really have a way to deal with them.
		 */
		setgid(gid);
		setuid(uid);
		environ = local_env;
		app_argv[0] = "ruby";
		app_argv[1] = app;
		app_argv[2] = (char *)0;

		// no input
		close(0);

		// output goes to the log file
		if (logfile_fd > 2) {
			dup2(logfile_fd, 1);
			dup2(logfile_fd, 2);
		}

		// paranoia strikes deep (Tom Truscott)
		for (i = 3; i <= 20; i++)
			close(i);

		execvp("ruby", app_argv);
		fprintf(stderr, "%s: execvp returned\n",
			myname);
		perror("execvp");
		_exit(1);
	} else {
		/* we are the parent */
		fprintf(pidfile, "%d\n", pid);
		fclose(pidfile);
		close(logfile_fd);
	}
}

int
main(int argc, char **argv, char **env)
{
	grok_args(argc, argv);
	load_init_environ(env);
	configure();
	if (verbose)
		print_known_variables();
	process_configuration();
	doit();
	return 0;
}
