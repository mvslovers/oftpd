-- test/tso/ispf.lua -- the ISPF dialog: ISPF, More (M), OFTP (O), then
-- the menu, the monitor, the partners, a held send through the transfer
-- list (show, hold, delete), the partner pick list and the configure
-- edit. Needs OFTPD running and the dialog installed (panels in ISPPLIB,
-- OFTPI in ISPLLIB), the data set <user>.OFTPD.SAMPLE, and the user's
-- Additional applications panel with an O entry that starts OFTPI.
-- The titles are matched in part: ISPF's own differ between systems.
local function show(t, title)
  print("==== " .. title)
  print(t:text())
end

return function(t)
  t:logon()
  t:type("ISPF"):enter()
  t:expect("rimary", { timeout = 30 })
  t:type("M"):enter()
  t:expect("dditional")
  show(t, "more")
  t:type("O"):enter()
  t:expect("OFTPD - Odette File Transfer", { timeout = 30 })
  show(t, "menu")
  t:type("5"):enter()
  t:expect("OFTPD - Monitor")
  show(t, "monitor")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")
  t:type("2"):enter()
  t:expect("OFTPD - Transfers")
  show(t, "transfers")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")
  t:type("3"):enter()
  t:expect("OFTPD - Partners")
  show(t, "partners")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- send held: partner, data set, (virtual name), text Y, translate N, hold Y
  t:type("1"):enter()
  t:expect("OFTPD - Send a Data Set")
  t:type("MAC1"):tab()
  t:type(t.user .. ".OFTPD.SAMPLE"):tab()
  t:type("ISPF.TEST"):tab()
  t:type("Y"):tab()
  t:type("N"):tab()
  t:type("Y"):enter()
  t:expect("queued and held")
  show(t, "send")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- the list, filtered to HELD; D on the first row deletes it
  t:type("2"):enter()
  t:expect("OFTPD - Transfers")
  t:tab():tab():tab()
  t:type("HELD"):enter()
  t:expect("1 transfers")
  show(t, "held")
  t:tab():tab():tab():tab():tab()
  t:type("S"):enter()
  t:expect("OFTPD - Transfer")
  t:expect("queued and held")
  show(t, "show")
  t:pf(3)
  t:expect("OFTPD - Transfers")
  t:tab():tab():tab():tab():tab()
  t:type("H"):enter()
  t:expect("only a queued or waiting send can be held")
  t:tab():tab():tab():tab():tab()
  t:type("D"):enter()
  t:expect("deleted")
  show(t, "deleted")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- the partner pick list: ? on the send panel, S on the first partner
  t:type("1"):enter()
  t:expect("OFTPD - Send a Data Set")
  t:type("?       "):enter()
  t:expect("OFTPD - Partners")
  -- command, scroll, the New partner field, then the first row
  t:tab():tab():tab()
  t:type("S"):enter()
  t:expect("OFTPD - Send a Data Set")
  show(t, "picked")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- configure: edit a copy of the partner member, OFTPD reads its own
  -- again when the edit ends
  t:type("4"):enter()
  t:expect("OFTPD - Configure the Partners")
  local dsn = t.fixtures.PARMS .. "(OFTPPTN0)"
  t:type(dsn .. string.rep(" ", 46 - #dsn)):enter()
  t:expect("EDITME")
  show(t, "edit")
  t:pf(3)
  t:expect("partners reloaded")
  show(t, "reloaded")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")
  t:pf(3)
  t:pf(3)
  t:expect("rimary")
  t:type("X"):enter()
  t:logoff()
end
