//
// Created by delaplai on 3/12/2024.
//


#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

char buffer[4096];

int main(int argc, char **argv)
{
    int delay = 1, n, m = 0;
    if (argc > 1)
        delay=atoi(argv[1]);
    fcntl(0, F_SETFL, fcntl(0,F_GETFL) | O_NONBLOCK); /* stdin */
    fcntl(1, F_SETFL, fcntl(1,F_GETFL) | O_NONBLOCK); /* stdout */
    while (1) {
        n = (int) read(0, buffer, 4096);
        if (n >= 0)
            m = (int) write(1, buffer, n);
        if ((n < 0 || m < 0) && (errno != EAGAIN))
            break;
        sleep((unsigned int) delay);
    }
    perror(n < 0 ? "stdin" : "stdout");
    exit(1);
}
