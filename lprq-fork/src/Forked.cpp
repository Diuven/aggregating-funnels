#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <system_error>

static void throwSystemError() {
    int error_code = errno;
    errno = 0;
    throw std::system_error(std::make_error_code(static_cast<std::errc>(error_code)));
}

bool execute_in_fork_and_wait() {
    pid_t p = fork();
    if (p == 0)
        return true;
    if (p < 0) {
        throwSystemError();
    }
    pid_t r = waitpid(p, nullptr, 0);
    if (r < 0) {
        throwSystemError();
    }
    return false;
}
