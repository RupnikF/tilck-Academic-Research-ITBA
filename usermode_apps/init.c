/* SPDX-License-Identifier: BSD-2-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>

#include <tilck/common/basic_defs.h> /* for MIN() and ARRAY_SIZE() */

#define DEFAULT_SHELL "/bin/devshell"

static char *shell_args[16] = { DEFAULT_SHELL, [1 ... 15] = NULL };

/* -- command line options -- */

static bool opt_quiet;

/* -- end -- */

static void call_exit(int code)
{
   printf("[init] exit with code: %i\n", code);
   exit(code);
}

static void open_std_handles(void)
{
   int in = open("/dev/tty0", O_RDONLY);
   int out = open("/dev/tty0", O_WRONLY);
   int err = open("/dev/tty0", O_WRONLY);

   if (in != 0) {
      printf("[init] in: %i, expected: 0\n", in);
      call_exit(1);
   }

   if (out != 1) {
      printf("[init] out: %i, expected: 1\n", out);
      call_exit(1);
   }

   if (err != 2) {
      printf("[init] err: %i, expected: 2\n", err);
      call_exit(1);
   }
}

static void do_initial_setup(void)
{
   if (getenv("TILCK")) {

      open_std_handles();

      if (getpid() != 1) {
         printf("[init] ERROR: my pid is %i instead of 1\n", getpid());
         call_exit(1);
      }

   } else {

      if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) < 0)
         perror("prctl(PR_SET_CHILD_SUBREAPER) failed");
   }
}

static void report_shell_exit(int wstatus)
{
   const int status = WEXITSTATUS(wstatus);
   printf("[init] the devshell exited with status: %d\n", status);
}

static void report_process_exit(pid_t pid, int wstatus)
{
   if (opt_quiet)
      return;

   const int status = WEXITSTATUS(wstatus);
   printf("[init] process %d exited with status: %d\n", pid, status);
}

static void show_help_and_exit(void)
{
   printf("\nUsage:\n\n");
   printf("    init                Regular run. Shell: %s\n", DEFAULT_SHELL);
   printf("    init -h/--help      Show this help and exit\n");
   printf("    init -q             Quiet: don't report exit of orphan tasks\n");
   printf("    init -- <cmdline>   "
          "Run the specified cmdline instead of the default one.\n");
   printf("                        Example: init -- /bin/devshell -c waitpid2\n");

   exit(0);
}

static void parse_opts(int argc, char **argv)
{
begin:

   if (!argc)
      return;

   if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help"))
      show_help_and_exit();

   if (!strcmp(*argv, "-q")) {
      opt_quiet = true;
      argc--; argv++;
      goto begin;
   }

   if (!strcmp(*argv, "--")) {
      const int elems = MIN(ARRAY_SIZE(shell_args), argc - 1);
      memcpy(shell_args, argv + 1, elems * sizeof(char *));
      return;
   }

unknown_opt:
   printf("[init] Unknown option '%s'\n", *argv);
}

static void wait_for_children(pid_t shell_pid)
{
   int wstatus;
   pid_t pid;

   while ((pid = waitpid(-1, &wstatus, 0)) > 0) {

      if (pid == shell_pid)
         report_shell_exit(wstatus);
      else
         report_process_exit(pid, wstatus);
   }
}

int main(int argc, char **argv, char **env)
{
   int shell_pid;

   do_initial_setup();
   parse_opts(argc - 1, argv + 1);

   shell_pid = fork();

   if (shell_pid < 0) {
      perror("fork() failed");
      call_exit(1);
   }

   if (!shell_pid) {
      if (execve(shell_args[0], shell_args, NULL) < 0) {
         perror("execve failed");
         call_exit(1);
      }
   }

   wait_for_children(shell_pid);
   call_exit(0);
}

