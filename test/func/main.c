#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

#define MAX_PATH_LEN 1024

/* Directory containing test files */
static char testdir[MAX_PATH_LEN];

/* Directory containing result files */
static char resultdir[MAX_PATH_LEN];

/* Path of toysqld executable */
static char serverpath[MAX_PATH_LEN];

/* File for out and err logs from the toysqld server */
static char errlogpath[MAX_PATH_LEN];

/* Template name for the temp directory */
static const char tmpdir_template[] = "/tmp/toysql-func-test-XXXXXX";

/* Temporary directory for output files */
static char tmpdir[sizeof(tmpdir_template)];

/* Pid of running toysqld server */
static pid_t serverpid;

static void self_exe_path(char *buf, size_t buflen)
{
	memset(buf, 0, buflen);

#ifdef __APPLE__
	if (proc_pidpath(getpid(), buf, buflen) <= 0) {
		perror("proc_pidpath");
		exit(EXIT_FAILURE);
	}
#elif __unix__
	if (readlink("/proc/self/exe", buf, buflen - 1) < buflen) {
		perror("readlink");
		exit(EXIT_FAILURE);
	}
#else
#error "No support for this platform"
#endif
}

static void init(void)
{
	char basedir[MAX_PATH_LEN];

	self_exe_path(basedir, sizeof(basedir));
	strncpy(basedir, dirname(dirname(basedir)), sizeof(basedir));

	strcpy(testdir, basedir);
	strcat(testdir, "/test/func/test/");

	strcpy(resultdir, basedir);
	strcat(resultdir, "/test/func/result/");

	strcpy(serverpath, basedir);
	strcat(serverpath, "/bin/toysqld");

	strcpy(tmpdir, tmpdir_template);
	mkdtemp(tmpdir);

	strcpy(errlogpath, tmpdir);
	strcat(errlogpath, "/error.log");
}

static void start_server(void)
{
	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		int fd = open(errlogpath, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
		if (fd <= 0) {
			perror(errlogpath);
			exit(EXIT_FAILURE);
		}
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);

		execl(serverpath, (char *)NULL);
		perror("toysqld");
		exit(EXIT_FAILURE);
	}
	sleep(1); /* wait for server start */
	serverpid = pid;
}

static void stop_server(void)
{
	kill(serverpid, SIGTERM);
}

static int check_server_alive(void)
{
	int status;

	if (waitpid(serverpid, &status, WNOHANG)) {
		if (WIFEXITED(status))
			printf("server exited\n");
		if (WIFSIGNALED(status))
			printf("server terminated with signal %d\n",
			       WTERMSIG(status));
		return 0;
	}
	return 1;
}

static size_t collect_test_names(const char *dirname, char **names,
				 size_t names_len)
{
	DIR	      *dir;
	struct dirent *entry;
	size_t	       n = 0;
	const char    *ext;

	dir = opendir(dirname);
	if (dir == NULL) {
		perror(dirname);
		return 0;
	}
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;
		ext = strchr(entry->d_name, '.');
		if (ext == NULL)
			continue;
		if (strcmp(ext, ".sql") != 0)
			continue;
		names[n]		      = strdup(entry->d_name);
		names[n][ext - entry->d_name] = '\0';
		++n;
	}
	closedir(dir);
	return n;
}

static void exec_sql_client(const char *infile, const char *outfile)
{
	char cwd[1024];
	int  fd;

	/* Redirect stdin */
	fd = open(infile, O_RDONLY);
	if (fd <= 0) {
		perror(infile);
		return;
	}
	close(STDIN_FILENO);
	dup2(fd, STDIN_FILENO);

	/* Redirect stdout and stderr */
	fd = open(outfile, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
	if (fd <= 0) {
		perror(outfile);
		return;
	}
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);

	/* Exec psql */
	getcwd(cwd, sizeof(cwd));
	execlp("psql", "psql", "-h", cwd, "-a", (char *)NULL);
	perror("psql");
}

static void print_file(const char *path)
{
	char  c;
	FILE *f = fopen(path, "r");

	while ((c = fgetc(f)) != EOF)
		putc(c, stdout);
	fclose(f);
}

static int diff(const char *f1, const char *f2, const char *fout)
{
	pid_t pid;
	int   fd;
	int   status;
	int   exit_status;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		fd = open(fout, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
		if (fd <= 0) {
			perror(fout);
			exit(EXIT_FAILURE);
		}
		close(STDOUT_FILENO);
		dup2(fd, STDOUT_FILENO);

		execlp("diff", "diff", f1, f2, (char *)NULL);
		perror("diff");
		exit(EXIT_FAILURE);
	}
	waitpid(pid, &status, 0);
	assert(WIFEXITED(status));
	exit_status = WEXITSTATUS(status);
	if (exit_status == 1) {
		printf("\033[31m");
		printf("diff %s %s\n", f1, f2);
		print_file(fout);
		printf("\033[0m");
	}
	return exit_status;
}

static int run_test(const char *testname)
{
	char  testpath[1024];
	char  resultpath[1024];
	char  outpath[1024];
	char  diffpath[1024];
	pid_t pid;
	int   status;

	strcpy(testpath, testdir);
	strcat(testpath, testname);
	strcat(testpath, ".sql");

	strcpy(resultpath, resultdir);
	strcat(resultpath, testname);
	strcat(resultpath, ".out");

	strcpy(outpath, tmpdir);
	strcat(outpath, "/");
	strcat(outpath, testname);
	strcat(outpath, ".out");

	strcpy(diffpath, tmpdir);
	strcat(diffpath, "/");
	strcat(diffpath, testname);
	strcat(diffpath, ".diff");

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		/* Will not return if successful */
		exec_sql_client(testpath, outpath);
		exit(EXIT_FAILURE);
	}
	waitpid(pid, &status, 0);

	if (!check_server_alive())
		return 1;

	return diff(resultpath, outpath, diffpath);
}

int main(int argc, char **argv)
{
	char  *testnames[1024];
	size_t ntests;

	init();

	start_server();

	printf("Searching for tests in %s\n", testdir);
	ntests = collect_test_names(testdir, testnames, 1024);
	printf("Collected %lu tests\n", ntests);

	for (int i = 0; i < ntests; ++i) {
		int rc;

		printf("%s...\n", testnames[i]);
		rc = run_test(testnames[i]);
		printf("%s...%s\n", testnames[i], rc ? "FAIL" : "OK");
	}

	stop_server();

	return 0;
}
