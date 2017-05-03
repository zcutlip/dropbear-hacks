/*
 * Dropbear - a SSH2 server
 * SSH client implementation
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * Copyright (c) 2004 by Mihnea Stoenescu
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "dbutil.h"
#include "runopts.h"
#include "session.h"
#include "dbrandom.h"
#include "crypto_desc.h"
#include "netio.h"

static void cli_dropbear_exit(int exitcode, const char* format, va_list param) ATTRIB_NORETURN;
static void cli_dropbear_log(int priority, const char* format, va_list param);

#ifdef CLI_REVERSE_CONNECT
static int cli_accept_remote(const char *local_port,char **error);
#endif

#ifdef ENABLE_CLI_PROXYCMD
static void cli_proxy_cmd(int *sock_in, int *sock_out, pid_t *pid_out);
static void kill_proxy_sighandler(int signo);
#endif

#if defined(DBMULTI_dbclient) || !defined(DROPBEAR_MULTI)
#if defined(DBMULTI_dbclient) && defined(DROPBEAR_MULTI)
int cli_main(int argc, char ** argv) {
#else
int main(int argc, char ** argv) {
#endif

	int dbsock;
	int sock_in, sock_out;
	struct dropbear_progress_connection *progress = NULL;
	char *error;
	
	_dropbear_exit = cli_dropbear_exit;
	_dropbear_log = cli_dropbear_log;

	disallow_core();

	seedrandom();
	crypto_init();

	cli_getopts(argc, argv);

#ifndef DISABLE_SYSLOG
	if (opts.usingsyslog) {
		startsyslog("dbclient");
	}
#endif

	TRACE(("user='%s' host='%s' port='%s'", cli_opts.username,
				cli_opts.remotehost, cli_opts.remoteport))

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		dropbear_exit("signal() error");
	}

	pid_t proxy_cmd_pid = 0;
#ifdef ENABLE_CLI_PROXYCMD
	if (cli_opts.proxycmd) {
		cli_proxy_cmd(&sock_in, &sock_out, &proxy_cmd_pid);
		m_free(cli_opts.proxycmd);
		if (signal(SIGINT, kill_proxy_sighandler) == SIG_ERR ||
			signal(SIGTERM, kill_proxy_sighandler) == SIG_ERR ||
			signal(SIGHUP, kill_proxy_sighandler) == SIG_ERR) {
			dropbear_exit("signal() error");
		}
	} else
#endif
	{
		progress = connect_remote(cli_opts.remotehost, cli_opts.remoteport, cli_connected, &ses);
		sock_in = sock_out = -1;
	}

#ifdef CLI_REVERSE_CONNECT
	TRACE(("REVERSE_CONNECT defined so doing accept."));
	dbsock = cli_accept_remote(cli_opts.local_port,&error);
	if(dbsock < 0)
	{
		dropbear_exit("%s",error);
	}
	db_progress_set_sock(progress,dbsock);
#endif /* CLI_REVERSE_CONNECT */

	cli_session(sock_in, sock_out, progress, proxy_cmd_pid);

	/* not reached */
	return -1;
}
#endif /* DBMULTI stuff */

static void cli_dropbear_exit(int exitcode, const char* format, va_list param) {
	char exitmsg[150];
	char fullmsg[300];

	/* Note that exit message must be rendered before session cleanup */

	/* Render the formatted exit message */
	vsnprintf(exitmsg, sizeof(exitmsg), format, param);

	/* Add the prefix depending on session/auth state */
	if (!sessinitdone) {
		snprintf(fullmsg, sizeof(fullmsg), "Exited: %s", exitmsg);
	} else {
		snprintf(fullmsg, sizeof(fullmsg), 
				"Connection to %s@%s:%s exited: %s", 
				cli_opts.username, cli_opts.remotehost, 
				cli_opts.remoteport, exitmsg);
	}

	/* Do the cleanup first, since then the terminal will be reset */
	session_cleanup();
	/* Avoid printing onwards from terminal cruft */
	fprintf(stderr, "\n");

	dropbear_log(LOG_INFO, "%s", fullmsg);
	exit(exitcode);
}

static void cli_dropbear_log(int priority,
		const char* format, va_list param) {

	char printbuf[1024];

	vsnprintf(printbuf, sizeof(printbuf), format, param);

#ifndef DISABLE_SYSLOG
	if (opts.usingsyslog) {
		syslog(priority, "%s", printbuf);
	}
#endif

	fprintf(stderr, "%s: %s\n", cli_opts.progname, printbuf);
	fflush(stderr);
}

static void exec_proxy_cmd(void *user_data_cmd) {
	const char *cmd = user_data_cmd;
	char *usershell;

	usershell = m_strdup(get_user_shell());
	run_shell_command(cmd, ses.maxfd, usershell);
	dropbear_exit("Failed to run '%s'\n", cmd);
}

#ifdef ENABLE_CLI_PROXYCMD
static void cli_proxy_cmd(int *sock_in, int *sock_out, pid_t *pid_out) {
	char * ex_cmd = NULL;
	size_t ex_cmdlen;
	int ret;

	fill_passwd(cli_opts.own_user);

	ex_cmdlen = strlen(cli_opts.proxycmd) + 6; /* "exec " + command + '\0' */
	ex_cmd = m_malloc(ex_cmdlen);
	snprintf(ex_cmd, ex_cmdlen, "exec %s", cli_opts.proxycmd);

	ret = spawn_command(exec_proxy_cmd, ex_cmd,
			sock_out, sock_in, NULL, pid_out);
	m_free(ex_cmd);
	if (ret == DROPBEAR_FAILURE) {
		dropbear_exit("Failed running proxy command");
		*sock_in = *sock_out = -1;
	}
}

static void kill_proxy_sighandler(int UNUSED(signo)) {
	kill_proxy_command();
	_exit(1);
}
#endif /* ENABLE_CLI_PROXYCMD */

#ifdef CLI_REVERSE_CONNECT
#define IN_ADDR(sa) \
	((sa)->sa_family!=AF_INET) ? \
		NULL : \
		&(((struct sockaddr_in *)sa)->sin_addr)

static int
cli_accept_remote(const char *local_port,char **error)
{
	TRACE(("cli_accept_remote"));
	int errornum;
	int server_sockfd;
	int connection_sockfd;
	socklen_t sin_size;
	struct addrinfo hints;
	struct addrinfo *srvinfo;
	struct addrinfo *p;
	struct sockaddr_storage their_addr;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if(NULL == local_port)
	{
		if(error != NULL)
		{
			*error = strdup("Invalid parameter: local_port string was NULL.\n");
		}
		return -1;
	}
	

	memset(&hints,0,sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; //use my ip
	
	if((rv = getaddrinfo(NULL,local_port,&hints,&srvinfo)) != 0)
	{
		TRACE(("getaddrinfo: %s\n",gai_strerror(rv)));
		return -1;
	}

	for(p=srvinfo; p != NULL; p=p->ai_next)
	{
		if((server_sockfd = socket(p->ai_family,p->ai_socktype,
						p->ai_protocol)) == -1)
		{
			TRACE(("server: socket %s",strerror(errno)));
			continue;
		}
		if(setsockopt(server_sockfd, SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
		{
			TRACE(("setsockopt: %s",strerror(errno)));
			*error=strdup(strerror(errno));
			return -1;
		}
		TRACE(("bind()"));
		if(bind(server_sockfd,p->ai_addr, p->ai_addrlen) == -1)
		{
			TRACE(("server: bind %s",strerror(errno)));
			close(server_sockfd);
			continue;
		}
		break;
	}

	if(NULL == p)
	{
		TRACE(("server: failed to bind."));
		*error=strdup("Failed to bind.");
		return -1;
	}

	freeaddrinfo(srvinfo);
	TRACE(("listen()"));
	if(listen(server_sockfd,1) == -1)
	{
		TRACE(("listen=: %s",strerror(errno)));
		*error=strdup(strerror(errno));
		return -1;
	}

	while(1)
	{
		sin_size=sizeof(their_addr);
		TRACE(("accept()"));
		connection_sockfd = accept(server_sockfd,(struct sockaddr *)&their_addr,&sin_size);
		if(connection_sockfd == -1)
		{
			TRACE(("accept: %s",strerror(errno)));
			continue;
		}
		inet_ntop(their_addr.ss_family,
				IN_ADDR((struct sockaddr *)&their_addr),
				s,sizeof(s));
		TRACE(("Connection from %s",s));
		
		close(server_sockfd); //done with listener
		return connection_sockfd;
	}

}
#endif /* CLI_REVERSE_CONNECT */