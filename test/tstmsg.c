/* TSTMSG - the message numbers of the monitor (#93): one meaning per
** number, the partner name followed by a colon. It reads the source of
** the monitor, because oftp_mon_msg() lives in an MVS-only file; it
** checks the literals, not what a running monitor writes. */
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#define SRC "src/mvs/oftp#mon.c"

static char letters[300][8];    /* classes seen per OFTPnnn */
static int  texts_ok[4];

#define HAS(p, lit) (strncmp((p), (lit), sizeof(lit) - 1) == 0)

static void
note(int num, char cls)
{
    char *l;

    if (num < 200 || num > 299)
        return;
    l = letters[num];

    if (strchr(l, cls) == NULL && strlen(l) < sizeof(letters[0]) - 1)
        l[strlen(l)] = cls;
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

int
main(void)
{
    static char line[512];
    FILE *f = fopen(SRC, "r");

    CHECK(f != NULL, SRC " can be read");
    if (f == NULL)
        return mbt_test_summary("TSTMSG");
    while (fgets(line, sizeof(line), f) != NULL)
        scan_line(line);
    fclose(f);

    CHECK(texts_ok[0] == 3, "OFTP201W, 202W and 218P: a colon after the partner");
    CHECK(texts_ok[3] == 0, "no partner name without the colon left");
    CHECK(texts_ok[1] == 1, "OFTP249P: data set deleted (DISP=DELETE)");
    CHECK(texts_ok[2] == 1, "OFTP250E: abend closing the files");

    CHECK(strcmp(letters[215], "W") == 0, "OFTP215 is only the held transfer");
    CHECK(strcmp(letters[216], "W") == 0, "OFTP216 is only the data set not deleted");
    CHECK(strcmp(letters[249], "P") == 0, "OFTP249 has one class");
    CHECK(strcmp(letters[250], "E") == 0, "OFTP250 has one class");

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
