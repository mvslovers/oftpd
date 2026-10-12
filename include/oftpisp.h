#ifndef OFTPISP_H
#define OFTPISP_H

/* oftpisp.h - ISPF dialog services for a C program (src/mvs/oftp#isp.c)
**
** ISPLINK is ISPF's, and mbt cannot resolve it on MVS after the link, so
** it is loaded at run time: the dialog runs as a command under ISPF from
** a library of the ISPLLIB concatenation, which holds ISPLINK as well.
** VDEFINE goes through ISPLINK (it takes a program address); everything
** else is a request string for the ISPEXEC command, which needs the
** program to have been started as a command (a CPPL).
*/

typedef struct oftp_isp {
    void       *isplink;                /* entry point, NULL if absent   */
} OFTP_ISP;

/* 0, or -1 when ISPLINK is not found (not under ISPF) */
extern int  oftp_isp_init(OFTP_ISP *isp)                asm("OFTPISIN");

/* Ties the dialog variable name to buf[len] (character, blank padded) */
extern int  oftp_isp_vdef(OFTP_ISP *isp, const char *name, char *buf,
                          int len)                      asm("OFTPISVD");

/* An ISPEXEC request, formatted; returns the service's return code */
extern int  oftp_isp_exec(const char *fmt, ...)         asm("OFTPISEX");

/* A C string into a blank-padded field, and back with blanks trimmed */
extern void oftp_isp_put(char *field, int len, const char *s)
                                                        asm("OFTPISPT");
extern void oftp_isp_get(const char *field, int len, char *s)
                                                        asm("OFTPISGT");

#endif /* OFTPISP_H */
