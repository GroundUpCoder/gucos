/* ash_main.c -- the standalone entry point for the ash build (/bin/ash).
 * In a multicall busybox, appletlib's main() dispatches on argv[0]; here
 * ash IS the binary, exactly like port/main.c for hush. */
#define PV_NO_INTERCEPT 1
#include "libbb.h"

int ash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;

int main(int argc, char **argv)
{
    return ash_main(argc, argv);
}
