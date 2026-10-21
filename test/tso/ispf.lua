-- test/tso/ispf.lua -- the ISPF dialog: ISPF, More (M), OFTP (O), then
-- the menu, the monitor, the partners, a held send through the transfer
-- list (show, hold, delete), the partner pick list, the maintenance of a
-- partner (new, modify, disable, enable, delete, and the two refusals of
-- a delete) and the configure edit. Needs OFTPD running with the partner
-- MAC1 in its partner member and ADMIN= set to the test user, the dialog
-- installed (panels in ISPPLIB, OFTPI in ISPLLIB), the data set
-- <user>.OFTPD.SAMPLE, and the user's Additional applications panel with
-- an O entry that starts OFTPI.
-- The titles are matched in part: ISPF's own differ between systems.
local function show(t, title)
  print("==== " .. title)
  print(t:text())
end

local TESTPTN = "DLGPTN"

-- the first screen row from 5 on that holds text, or nil
local function row_of(t, text)
  local s = t:screen()
  for i = 5, #s do
    if s[i]:find(text, 1, true) then return i end
  end
  return nil
end

local function find_row(t, text)
  return row_of(t, text) or error("no screen row with " .. text)
end

-- tab until the cursor stands at row, col
local function tabto(t, row, col)
  for _ = 1, 80 do
    local r, c = t:cursor()
    if r == row and c == col then return end
    t:tab()
  end
  error("cannot reach row " .. row .. ", column " .. col)
end

-- the line command field of the row that names something, and the command in it
local function line(t, name, cmd)
  tabto(t, find_row(t, name), 2)
  t:type(cmd)
end

-- the screen row naming something must also hold text
local function expect_row(t, name, text)
  local r = find_row(t, name)
  local l = t:screen()[r]
  if not l:find(text, 1, true) then
    error("row of " .. name .. " lacks " .. text .. ": " .. l)
  end
end

-- the partner list's New partner field
local function new_partner(t, name)
  tabto(t, find_row(t, "New partner"), 21)
  t:type(name)
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
  -- a held transfer of an earlier, failed run would confuse the counts below
  t:tab():tab():tab()
  t:type("HELD"):enter()
  t:expect("Status ===> HELD")
  while row_of(t, "ISPF.TEST") or row_of(t, "DLG.HELD") do
    line(t, row_of(t, "ISPF.TEST") and "ISPF.TEST" or "DLG.HELD", "D"); t:enter()
    t:expect("deleted")
  end
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")
  t:type("3"):enter()
  t:expect("OFTPD - Partners")
  show(t, "partners")
  t:expect("MAC1")
  if row_of(t, TESTPTN) then  -- left by an earlier, failed run
    line(t, TESTPTN, "D"); t:enter()
    t:expect("Delete a Partner")
    t:enter()
    t:expect("deleted")
  end
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

  -- the list, filtered to HELD; S shows the transfer, H refuses, D deletes
  t:type("2"):enter()
  t:expect("OFTPD - Transfers")
  t:tab():tab():tab()
  t:type("HELD"):enter()
  t:expect("1 transfers")
  show(t, "held")
  local key = t:screen()[find_row(t, "ISPF.TEST")]:match("T%d+")
  line(t, "ISPF.TEST", "S"); t:enter()
  t:expect("OFTPD - Transfer")
  t:expect(key .. " SEND HELD, partner MAC1")
  t:expect("virtual file ISPF.TEST")
  t:expect("data set " .. t.user .. ".OFTPD.SAMPLE")
  t:expect("queued and held")
  show(t, "show")
  t:pf(3)
  t:expect("OFTPD - Transfers")
  line(t, "ISPF.TEST", "H"); t:enter()
  t:expect("only a queued or waiting send can be held")
  line(t, "ISPF.TEST", "D"); t:enter()
  t:expect(key .. " deleted")
  show(t, "deleted")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- the partner pick list: ? on the send panel, S on MAC1
  t:type("1"):enter()
  t:expect("OFTPD - Send a Data Set")
  t:type("?       "):enter()
  t:expect("OFTPD - Partners")
  line(t, "MAC1", "S"); t:enter()
  t:expect("OFTPD - Send a Data Set")
  show(t, "picked")
  t:expect("Partner   ===> MAC1")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- partner maintenance: new, modify, disable, enable, delete and the
  -- two refusals of a delete
  t:type("3"):enter()
  t:expect("OFTPD - Partners")
  new_partner(t, TESTPTN); t:enter()
  t:expect("Function")
  t:enter()
  t:expect("VALUE IS BLANK")
  show(t, "new, Odette ID blank")
  t:type("O0000" .. TESTPTN):enter()
  t:expect("OFTPD - Partners")
  t:expect("partner " .. TESTPTN .. " added")
  expect_row(t, TESTPTN, "ENABLED")
  show(t, "new, added")

  new_partner(t, TESTPTN); t:enter()
  t:expect("The partner is there: M changes it")

  new_partner(t, "DLGPT2"); t:enter()
  t:expect("Function")
  t:type("O0000" .. TESTPTN):enter()
  t:expect("is partner " .. TESTPTN .. "'s already")
  show(t, "new, Odette ID taken")
  t:pf(3)
  t:expect("OFTPD - Partners")
  if row_of(t, "DLGPT2") then error("DLGPT2 was added after END") end

  line(t, TESTPTN, "M"); t:enter()
  t:expect("Function")
  t:enter()
  t:expect("Nothing was changed")
  t:pf(3)
  t:expect("OFTPD - Partners")
  line(t, TESTPTN, "M"); t:enter()
  t:expect("Function")
  tabto(t, find_row(t, "Sessions"), 23)
  t:type("2"):enter()
  t:expect("OFTPD - Partners")
  t:expect("partner " .. TESTPTN .. " changed")

  line(t, TESTPTN, "X"); t:enter()
  t:expect("DISABLED")
  expect_row(t, TESTPTN, "DISABLED")
  line(t, TESTPTN, "E"); t:enter()
  expect_row(t, TESTPTN, "ENABLED")
  line(t, TESTPTN, "Z"); t:enter()
  t:expect("Line commands: C call, M modify, E enable, X disable, D delete")

  -- a partner of OFTPPTN cannot be deleted
  line(t, "MAC1", "D"); t:enter()
  t:expect("Delete a Partner")
  t:enter()
  t:expect("the partner is in OFTPPTN: remove it there, then REFRESH")
  show(t, "delete, in OFTPPTN")
  expect_row(t, "MAC1", "ENABLED")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- a transfer in the spool names the partner: D disables it and refuses
  t:type("1"):enter()
  t:expect("OFTPD - Send a Data Set")
  t:type(TESTPTN):tab()
  t:type(t.user .. ".OFTPD.SAMPLE"):tab()
  t:type("DLG.HELD"):tab()
  t:type("Y"):tab()
  t:type("N"):tab()
  t:type("Y"):enter()
  t:expect("queued and held")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")
  t:type("3"):enter()
  t:expect("OFTPD - Partners")
  line(t, TESTPTN, "D"); t:enter()
  t:expect("Delete a Partner")
  t:enter()
  t:expect("transfers name partner " .. TESTPTN)
  show(t, "delete, transfer in the spool")
  expect_row(t, TESTPTN, "DISABLED")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")

  -- the transfer goes, then the partner can
  t:type("2"):enter()
  t:expect("OFTPD - Transfers")
  line(t, "DLG.HELD", "D"); t:enter()
  t:expect("deleted")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")
  t:type("3"):enter()
  t:expect("OFTPD - Partners")
  line(t, TESTPTN, "D"); t:enter()
  t:expect("Delete a Partner")
  t:enter()
  t:expect("partner " .. TESTPTN .. " deleted")
  if row_of(t, TESTPTN) then error(TESTPTN .. " is still listed") end
  show(t, "delete, done")
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
  -- retype the HOST line with the other port (same length, so nothing is left over)
  local was = t:screen()[find_row(t, "HOST=")]:match("PORT=(%d+)")
  local now = was == "3307" and "3306" or "3307"
  tabto(t, find_row(t, "HOST="), 9)
  t:type("  HOST=127.0.0.1 PORT=" .. now .. " LEVEL=5")
  t:pf(3)
  t:expect("partners reloaded")
  show(t, "reloaded")
  -- the edit was kept: the member shows the new port when opened again
  t:enter()
  t:expect("PORT=" .. now)
  t:pf(3)
  t:expect("partners reloaded")
  t:pf(3)
  t:expect("OFTPD - Odette File Transfer")
  t:pf(3)
  t:pf(3)
  t:expect("rimary")
  t:type("X"):enter()
  t:logoff()
end
