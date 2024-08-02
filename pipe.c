#include <unistd.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include "pipe.h"

pid_t bidir_pipe_cmd(int p[2], const char *cmd)
{
	pid_t pid = -1;
	extern char **environ;
	int pout[2]={-1,-1}, pin[2]={-1,-1};
	posix_spawn_file_actions_t fa;
	int fa_live = 0;

	if (pipe2(pout, O_CLOEXEC) || pipe2(pin, O_CLOEXEC))
		goto fail;
	if (posix_spawn_file_actions_init(&fa))
		goto fail;
	fa_live = 1;

	if (posix_spawn_file_actions_addclose(&fa, pout[1]) ||
	    posix_spawn_file_actions_addclose(&fa, pin[0]) ||
	    posix_spawn_file_actions_adddup2(&fa, pout[0], 0) ||
	    posix_spawn_file_actions_adddup2(&fa, pin[1], 1) ||
	    posix_spawnp(&pid, "sh", &fa, 0, (char *[])
	                 { "sh", "-c", (char *)cmd, 0 },
	                 environ))
	{
		goto fail;
	}
	close(pout[0]); pout[0] = -1;
	close(pin[1]); pin[1] = -1;

	p[1] = pout[1];
	p[0] = pin[0];

	posix_spawn_file_actions_destroy(&fa);
	return pid;
fail:
	if (fa_live) posix_spawn_file_actions_destroy(&fa);
	if (pout[0]>=0) close(pout[0]);
	if (pout[1]>=0) close(pout[1]);
	if (pin[0]>=0) close(pin[0]);
	if (pin[1]>=0) close(pin[1]);
	int status;
	if (pid>=1) waitpid(pid, &status, 0);
	return -1;
}
