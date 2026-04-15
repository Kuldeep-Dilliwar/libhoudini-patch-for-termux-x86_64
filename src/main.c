#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>                                                                                                                                                                        
#include <sys/wait.h>
#include <sys/user.h>
#include <signal.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <arm64_binary>\n", argv[0]);
        return 1;
    }

    // 1. Spawn a child process
    pid_t child = fork();

    if (child == 0) {
        // --- CHILD PROCESS ---
        // Tell the OS kernel: "Allow my parent to trace and modify me"
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);

        // Replace this child process with the target ARM64 binary
        execvp(argv[1], &argv[1]);
        perror("execvp failed");
        exit(1);
    }

    // --- PARENT PROCESS (The Tracer) ---                                                                                                                                                     
    int status;
    printf("[AutoPatcher] Attached to %s (PID: %d)\n", argv[1], child);

    while (1) {
        // Wait for the child to change state (e.g., hit a segfault)
        waitpid(child, &status, 0);

        // If the program finished normally, exit our loop
        if (WIFEXITED(status)) {
            printf("[AutoPatcher] Program exited normally with code %d\n", WEXITSTATUS(status));
            break;
        }
        if (WIFSIGNALED(status)) {
            printf("[AutoPatcher] Program terminated by signal %d\n", WTERMSIG(status));
            break;
        }

        // If the program was stopped by a signal
        if (WIFSTOPPED(status)) {
            int sig = WSTOPSIG(status);
            int signal_to_inject = 0;

            if (sig == SIGSEGV) {
                // We caught a Segmentation Fault!
                // Read the x86_64 hardware registers from the child process
                struct user_regs_struct regs;
                ptrace(PTRACE_GETREGS, child, NULL, &regs);

                // Check if the RDX register has an ARM TBI tag (top byte is not 00)
                // A canonical x86_64 user-space pointer should be less than 0x00007FFFFFFFFFFF
                if (regs.rdx > 0x00FFFFFFFFFFFFFF) {
                    printf("[AutoPatcher] Caught SIGSEGV! Bad Pointer in RDX: 0x%lx\n", regs.rdx);

                    // Apply the bitmask to strip the Top Byte Ignore (TBI) tag
                    regs.rdx = regs.rdx & 0x00FFFFFFFFFFFFFF;

                    printf("[AutoPatcher] Stripped Tag. New RDX: 0x%lx. Resuming...\n", regs.rdx);

                    // Write the fixed registers back into the CPU
                    ptrace(PTRACE_SETREGS, child, NULL, &regs);

                    // We fixed the issue, so we DO NOT pass the SEGV signal back to the child
                    signal_to_inject = 0;
                } else {
                    // It was a legitimate crash caused by something else.
                    // Let the child die naturally.
                    signal_to_inject = SIGSEGV;
                }
            }
            else if (sig == SIGTRAP) {
                // Ignore standard ptrace traps
                signal_to_inject = 0;
            }
            else {
                // Pass any other signals (like Ctrl+C / SIGINT) down to the program
                signal_to_inject = sig;
            }

            // Tell the CPU to resume executing the child program
            ptrace(PTRACE_CONT, child, NULL, signal_to_inject);
        }
    }

    return 0;
}
