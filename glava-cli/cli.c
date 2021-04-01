#include <glava.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

static glava_handle handle;

static void handle_term  (int _) {
    printf("Interrupt received, closing...\n");
    glava_terminate(&handle);
}
static void handle_reload(int _) {
    printf("User signal received, reloading...\n");
    glava_reload(&handle);
}

int main(int argc, char** argv) {
    const struct sigaction term_action   = { .sa_handler = handle_term   };
    const struct sigaction reload_action = { .sa_handler = handle_reload };
    sigaction(SIGTERM, &term_action,   NULL);
    sigaction(SIGINT,  &term_action,   NULL);
    sigaction(SIGUSR1, &reload_action, NULL);
        
    glava_entry(argc, argv, &handle);
    return EXIT_SUCCESS;
}
