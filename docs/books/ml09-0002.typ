#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "OFTPD for MVS 3.8j",
  subtitle: "Reference",
  short-title: "OFTPD Reference",
  product: "OFTPD",
  number: "ML09-0002-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to Version 1 Release 0 of OFTPD, the OFTP server
    for MVS 3.8j (oftpd 1.0.0), and to all subsequent releases and
    modifications until otherwise indicated in new editions.

    *Draft.* OFTPD 1.0 is still in development, and so is this book. It
    describes the state of the main branch of the repository.

    Comments on this book may be addressed to the issue tracker of the
    mvslovers/oftpd repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [OFTPD Reference])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book describes OFTPD (oftpd), the OFTP server for MVS 3.8j, in full:
the commands of the #cmd("OFTP") utility, the operator commands of the
monitor, the configuration parameters and the partner table, and the
messages. How to use them together is the subject of the companion volume, the _OFTPD User's Guide_.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“The OFTP Command”.],
  [Chapter 2], [“Operator Commands”.],
  [Chapter 3], [“Configuration and the Partner Table”.],
  [Appendix A], [“Messages”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML09-0001], [_OFTPD User's Guide_: installing OFTPD, running the
    monitor, setting up partners, and sending and receiving files.],
)

#mainmatter()
]
#set page(numbering: "1")

#part("ref-cmd.html", include "ref/ref-cmd.typ")
#part("ref-op.html", include "ref/ref-op.typ")
#part("ref-cfg.html", include "ref/ref-cfg.typ")

#show: appendices
#part("apx-messages.html", include "ref/apx-messages.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
