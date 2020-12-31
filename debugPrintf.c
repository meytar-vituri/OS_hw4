//paste all of the below code in pfind.c
//make sure you have a global variable num_of_running_threads
// use the function debugPrintf instead of printf to debug your code, printed messages can be formatted like in printf
//credits to sid feiner who wrote this awesome code!

#include <stdarg.h>
#include <stdint.h>

long getNanoTs(void) {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return (int64_t) (spec.tv_sec) * (int64_t) 1000000000 + (int64_t) (spec.tv_nsec);
}

char *debugFormat = "[%02x] : %lu : %d : ";
char *debugLevel = "***";

void debugPrintf(char *fmt, ...) {
    pthread_mutex_t printLock;
    if (pthread_mutex_init(&printLock, NULL)) {
        printf("ERROR creating printLock\n");
    }
    va_list args;
    va_start(args, fmt);
    char *placeholder = malloc(strlen(debugFormat) + strlen(debugLevel) + 1);
    char *newFmt = malloc(strlen(debugLevel) + strlen(fmt) + 1);
    snprintf(placeholder, strlen(debugFormat) + strlen(debugLevel) + 1, "%s%s", debugLevel, debugFormat);
    snprintf(newFmt, strlen(debugLevel) + strlen(fmt) + 1, "%s%s", debugLevel, fmt);
    pthread_mutex_lock(&printLock);
    printf(placeholder, pthread_self(), getNanoTs(), num_of_running_threads);
    vprintf(newFmt, args);
    pthread_mutex_unlock(&printLock);
    fflush(stdout);
    free(newFmt);
    free(placeholder);
    va_end(args);
    pthread_mutex_destroy(&printLock);
}
//++++++++++++++++=

