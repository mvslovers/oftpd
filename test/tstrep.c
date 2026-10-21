/* TSTREP - an answer of OFTPD as the client reads it, in pieces */
#include <stdio.h>
#include <string.h>

#include <mbtcheck.h>

#include "oftpcli.h"

static int  rows;
static char last[80];

static void
row(void *ctx, const char *text)
{
    (void)ctx;
    rows++;
    snprintf(last, sizeof(last), "%s", text);
}

static int
piece(const char *buf, char *final)
{
    final[0] = '\0';
    return oftp_cli_piece(buf, (unsigned)strlen(buf), row, NULL, final, 80, NULL);
}

int
main(void)
{
    char final[80];

    printf("=== TSTREP answers ===\n");

    /* the return codes the utility documents */
    CHECK_EQ(oftp_cli_rc("200 end"), 0, "200 is done");
    CHECK_EQ(oftp_cli_rc("210 queued, empty"), 4, "210 is done, with a warning");
    CHECK_EQ(oftp_cli_rc("450 active"), 4, "4xx is refused for now");
    CHECK_EQ(oftp_cli_rc("504 no such transfer"), 4, "504 is refused for now");
    CHECK_EQ(oftp_cli_rc("501 unknown partner"), 12, "501 is severe");
    CHECK_EQ(oftp_cli_rc("506 not your transfer"), 8, "506 is an error");
    CHECK_EQ(oftp_cli_rc("507 needs ADMIN="), 8, "507 is an error");

    /* a whole answer in one piece */
    rows = 0;
    CHECK_EQ(piece("100 a\n100 b\n200 2 transfers\n", final), 0,
             "a complete answer gives its rc");
    CHECK_EQ(rows, 2, "each row once");
    CHECK(strcmp(last, "b") == 0, "rows without their code");
    CHECK(strcmp(final, "2 transfers") == 0, "the final line without its code");

    /* a piece that stops after a row: more to come, nothing final */
    rows = 0;
    CHECK_EQ(piece("100 a\n100 b\n", final), -1, "a piece of rows only: more");
    CHECK_EQ(rows, 2, "its rows delivered");
    CHECK_EQ(final[0], '\0', "and no final text");
    CHECK_EQ(piece("100 c\n200 3 transfers\n", final), 0,
             "the last piece ends the answer");
    CHECK_EQ(rows, 3, "the rows of both pieces add up");

    /* an empty piece is a piece with nothing in it yet */
    rows = 0;
    CHECK_EQ(piece("", final), -1, "an empty piece: more");
    CHECK_EQ(rows, 0, "no rows");

    /* the final line alone, and a refusal */
    CHECK_EQ(piece("504 no such transfer\n", final), 4, "a refusal alone");
    CHECK(strcmp(final, "no such transfer") == 0, "with its text");

    /* not an answer: a line cut short, a line after the final one */
    CHECK_EQ(piece("100 a\n100 b", final), -2, "a row without its newline");
    CHECK_EQ(piece("200 end", final), -2, "a final line without its newline");
    CHECK_EQ(piece("200 end\n100 late\n", final), -2,
             "a line after the final one");

    /* a 1xx line with no text, and no callback at all */
    rows = 0;
    CHECK_EQ(piece("100\n200 ok\n", final), 0, "a row with no text");
    CHECK(strcmp(last, "") == 0, "comes as an empty string");
    CHECK_EQ(oftp_cli_piece("100 x\n200 y\n", 12, NULL, NULL, final, 80, NULL), 0,
             "no callback: rows are skipped");

    /* the code of the final line, for a caller that must tell 210 from 4xx */
    {
        int code = -1;

        CHECK_EQ(oftp_cli_piece("100 w\n210 done\n", 15, NULL, NULL, final,
                                80, &code), 4, "210 is RC 4");
        CHECK_EQ(code, 210, "and its code is handed out");
        CHECK_EQ(oftp_cli_piece("450 busy\n", 9, NULL, NULL, final, 80,
                                &code), 4, "450 is RC 4 too");
        CHECK_EQ(code, 450, "but with another code");
        CHECK_EQ(oftp_cli_piece("200 ok\n", 7, NULL, NULL, final, 80, &code),
                 0, "200 is RC 0");
        CHECK_EQ(code, 200, "with its code");
    }

    /* an answer that does not end as one carries no code: a caller that
    ** decides on code / 100 == 2 must not take it for success (#136)
    */
    {
        int code = -1;

        CHECK_EQ(oftp_cli_piece("200 ok\n100 late\n", 17, NULL, NULL, final,
                                80, &code), -2, "a final line with a tail");
        CHECK_EQ(oftp_cli_broken(final, 80, &code), 12, "is RC 12");
        CHECK_EQ(code, 0, "with no code");
        CHECK(strcmp(final, "incomplete answer from OFTPD") == 0,
              "and its text");
        CHECK_EQ(oftp_cli_broken(final, 80, NULL), 12, "no code wanted: RC 12");
    }

    return mbt_test_summary("TSTREP");
}
