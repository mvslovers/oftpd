#import "../bookmaster/bookmaster.typ": *

// One message of the appendix.  The destination follows from the class
// letter (P: log only, otherwise console and log) unless dest says otherwise.
#let msg(id, text, explanation, action, dest: auto) = {
  let where = if dest != auto { dest } else if id.last() == "P" { "log only" } else { "console and log" }
  idx(id)
  block(above: 1.4em, below: 0.4em, breakable: false)[#cmd(id) #cmd(text)]
  block(above: 0.4em, below: 0.4em, inset: (left: 0.25in))[
    *Destination:* #where

    *Explanation:* #explanation

    *Action:* #action
  ]
}
