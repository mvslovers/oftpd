/* TSTLOD - background load for the TSTLAT measurement (MVS)
**
** Keeps the system busy for a number of seconds so that the request
** times of TSTLAT can be taken on a loaded system. PARM='CPU,secs' spins
** the processor; PARM='IO,secs,dsn' writes and reads a data set over and
** over (and removes it at the end). Prints how many rounds it got
** through, so a load that did nothing shows as such. It measures
** nothing and checks nothing.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int
main(int argc, char **argv)
{
    char          parm[100];
    char          line[81];
    const char   *mode;
    const char   *dsn = NULL;
    char         *p;
    char         *q;
    long          secs = 0;
    unsigned long rounds = 0;
    unsigned long x = 12345;
    time_t        t0;

    if (argc < 2) {
        printf("TSTLOD: PARM='CPU,secs' or PARM='IO,secs,dsn'\n");
        return 0;
    }
    snprintf(parm, sizeof(parm), "%s", argv[1]);
    mode = parm;
    p    = strchr(parm, ',');
    if (p != NULL) {
        *p++ = '\0';
        q = strchr(p, ',');
        if (q != NULL) {
            *q++ = '\0';
            if (*q != '\0')
                dsn = q;
        }
        secs = atol(p);
    }
    if (mode[0] == '\0' || secs < 1 || (strcmp(mode, "IO") == 0 && dsn == NULL)) {
        printf("TSTLOD: PARM='CPU,secs' or PARM='IO,secs,dsn'\n");
        return 8;
    }
    t0 = time(NULL);
    if (strcmp(mode, "CPU") == 0) {
        while ((long)(time(NULL) - t0) < secs) {
            for (int i = 0; i < 20000; i++)
                x = x * 1103515245UL + 12345UL;
            rounds++;
        }
        printf("TSTLOD CPU: %lu rounds in %ld s (%lu)\n", rounds, secs, x);
        return 0;
    }
    memset(line, 'X', 80);
    line[80] = '\0';
    while ((long)(time(NULL) - t0) < secs) {
        FILE *fp = fopen(dsn, "w");

        if (fp == NULL) {
            printf("TSTLOD IO: cannot create %s\n", dsn);
            return 12;
        }
        for (int i = 0; i < 300; i++)
            fprintf(fp, "%s\n", line);
        fclose(fp);
        if ((fp = fopen(dsn, "r")) == NULL) {
            printf("TSTLOD IO: cannot read %s\n", dsn);
            return 12;
        }
        while (fgets(line, sizeof(line), fp) != NULL)
            ;
        fclose(fp);
        memset(line, 'X', 80);
        line[80] = '\0';
        rounds++;
    }
    remove(dsn);
    printf("TSTLOD IO: %lu rounds in %ld s\n", rounds, secs);
    return 0;
}
