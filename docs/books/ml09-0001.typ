#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "OFTPD for MVS 3.8j",
  subtitle: "User's Guide",
  short-title: "OFTPD User's Guide",
  product: "OFTPD",
  number: "ML09-0001-0",
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

#part("index.html", title: [OFTPD User's Guide])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book shows how to use OFTPD (oftpd), the OFTP server for MVS 3.8j:
how it is installed, how the monitor runs as a started task, how partners
are set up, and how files are sent and received. Every command, parameter
and message is described in full in the companion volume, the _OFTPD
Reference_.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“Introducing OFTPD”.],
  [Chapter 2], [“Installing OFTPD”.],
  [Chapter 3], [“The Monitor”.],
  [Chapter 4], [“Partners and Configuration”.],
  [Chapter 5], [“Sending and Receiving Files”.],
  [Chapter 6], [“Operating and Troubleshooting”.],
  [Appendix A], [“Glossary”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML09-0002], [_OFTPD Reference_: the commands of #cmd("OFTP"), the
    operator commands, the configuration parameters and the partner table,
    and the messages.],
)

#mainmatter()
]
#set page(numbering: "1")

#part("ug-intro.html", include "guide/ug-intro.typ")
#part("ug-install.html", include "guide/ug-install.typ")
#part("ug-monitor.html", include "guide/ug-monitor.typ")
#part("ug-partners.html", include "guide/ug-partners.typ")
#part("ug-transfer.html", include "guide/ug-transfer.typ")
#part("ug-ops.html", include "guide/ug-ops.typ")

#show: appendices
#part("ug-glossary.html", include "guide/ug-glossary.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
