/* TSTMSG - the message numbers of OFTPD (#93, #200): one meaning per
** number, the partner name followed by a colon. It reads every .c and .h
** file under src/, because the messages live in MVS-only files too; it
** checks the literals, not what a running monitor writes. */
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <mbtcheck.h>

#define SRC "src"

static char letters[300][8];    /* classes seen per OFTPnnn */
static int  texts_ok[4];

#define HAS(p, lit) (strncmp((p), (lit), sizeof(lit) - 1) == 0)

static void
note(int num, char cls)
{
    size_t k;

    if (num < 200 || num > 299)
        return;
    k = strlen(letters[num]);
    if (strchr(letters[num], cls) == NULL && k + 1 < sizeof(letters[num])) {
        letters[num][k] = cls;
        letters[num][k + 1] = '\0';
    }
}

static void
scan_line(const char *line)
{
    const char *p = line;

    while ((p = strstr(p, "\"OFTP2")) != NULL) {
        int  num = 200 + (p[6] - '0') * 10 + (p[7] - '0');
        char cls = p[8];

        if (p[6] >= '0' && p[6] <= '9' && p[7] >= '0' && p[7] <= '9'
            && cls >= 'A' && cls <= 'Z') {
            note(num, cls);
            if (HAS(p, "\"OFTP201W %s: CALLED FROM ")
                || HAS(p, "\"OFTP202W %s: REFUSED, ")
                || HAS(p, "\"OFTP218P %s: BUSY, "))
                texts_ok[0]++;
            if (HAS(p, "\"OFTP249P %s: %.8s %s DELETED (DISP=DELETE)"))
                texts_ok[1]++;
            if (HAS(p, "\"OFTP250E %s: ABEND %s CLOSING THE FILES"))
                texts_ok[2]++;
            if (HAS(p, "\"OFTP201W %s ")
                || HAS(p, "\"OFTP202W %s ")
                || HAS(p, "\"OFTP218P %s "))
                texts_ok[3]++;
        }
        p += 6;
    }
}

static int files_read;
static int seen_mon, seen_spl;

static void
scan_file(const char *path)
{
    static char line[512];
    FILE *f = fopen(path, "r");

    if (f == NULL) {
        CHECK(0, "a source file can be read");
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL)
        scan_line(line);
    fclose(f);
    files_read++;
    if (strstr(path, "oftp#mon.c") != NULL)
        seen_mon = 1;
    if (strstr(path, "oftp#spl.c") != NULL)
        seen_spl = 1;
}

static void
walk(const char *dir)
{
    struct dirent *e;
    DIR *d = opendir(dir);

    if (d == NULL) {
        CHECK(0, "a source directory can be read");
        return;
    }
    while ((e = readdir(d)) != NULL) {
        char path[512];
        struct stat st;
        size_t n;

        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (stat(path, &st) != 0)
            continue;
        n = strlen(e->d_name);
        if (S_ISDIR(st.st_mode))
            walk(path);     /* src/ and src/mvs: two levels */
        else if (n > 2 && (strcmp(e->d_name + n - 2, ".c") == 0
                           || strcmp(e->d_name + n - 2, ".h") == 0))
            scan_file(path);
    }
    closedir(d);
}

int
main(void)
{
    walk(SRC);
    CHECK(files_read > 0 && seen_mon && seen_spl,
          "the monitor and spool sources were scanned");

    CHECK(texts_ok[0] == 3, "OFTP201W, 202W and 218P: a colon after the partner");
    CHECK(texts_ok[3] == 0, "no partner name without the colon left");
    CHECK(texts_ok[1] == 1, "OFTP249P: data set deleted (DISP=DELETE)");
    CHECK(texts_ok[2] == 1, "OFTP250E: abend closing the files");

    CHECK(strcmp(letters[215], "W") == 0, "OFTP215 is only the held transfer");
    CHECK(strcmp(letters[216], "W") == 0, "OFTP216 is only the data set not deleted");
    CHECK(strcmp(letters[249], "P") == 0, "OFTP249 has one class");
    CHECK(strcmp(letters[250], "E") == 0, "OFTP250 has one class");
    CHECK(strcmp(letters[219], "E") == 0, "OFTP219 is only the spool request abend");
    CHECK(strcmp(letters[251], "W") == 0, "OFTP251 is the spool thread without recovery");

    /* two letters on one number: only where it is one event (233), or
    ** the engine's own line with the class as the letter (290) */
    for (int n = 201; n <= 299; n++) {
        if (strlen(letters[n]) > 1 && n != 233 && n != 290) {
            printf("    OFTP%d has the classes %s\n", n, letters[n]);
            CHECK(0, "a number carries one meaning");
        }
    }
    return mbt_test_summary("TSTMSG");
}
