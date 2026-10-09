-- re8_vrz_reload_guard.lua -- a reload is not cancelled by the hands-up block gesture (2026-10-10, Tefa in the headset).
--
-- WHY: Tefa, with the left hand gripping the rifle (LG held): "tilting it up and down and moving the right controller ...
-- breaks that reload animation and it doesn't finish, the hand teleports back on the gun" -- "it's when i raised my right
-- controller it stops reloading". Without LG the reload is fine.
-- The grip log showed our grip never let go during those reloads; praydog's is_reloading simply ended after 0.3-1.5 s,
-- so the GAME cancelled them. praydog's RE8 VR (RE8VR.cpp, update_block_gesture) sets wants_block when BOTH controllers
-- are in front of the face (dot >= 0.8 with the gaze) and turned (controller X axis >= 0.5 along the gaze); re8_vr.lua then
-- presses the guard button for the game, and app.PlayerBase.tryGuardStart starts a guard -- which cancels the reload.
-- With LG held the left hand sits beside the right on the rifle, so raising the rifle to the face is exactly that gesture;
-- with the left hand hanging down it never is. `[inferred-static 2026-10-10]` until the first wear.
--
-- WHAT (first try, kept): while the game says the player is reloading, tryGuardStart is skipped (the same hook praydog uses to allow the
-- guard only by gesture; REFramework chains both). The guard button press still goes through (praydog's pad code is not
-- touched), so if the press alone also cancels the reload, this script's log line will show the guard skipped and the
-- reload still ending early -- then the next lever is the pad press itself.
-- Log: [re8-reload-guard] one line per skipped guard, with how long the reload has been running.
-- Settings file: reframework/data/re_scope_reload_guard.txt, `on=1` (default) / `on=0` = do nothing. Read every second.

local TAG = "[re8-reload-guard] "
local function L(s) log.info(TAG .. tostring(s)) end
local CFG_FILE = "re_scope_reload_guard.txt"
local CFG_EVERY_S = 1.0

-- two_hands=1 (2026-10-10, Tefa: "block the guard if there are two hands on the weapon for all guns, so if LG held and
-- hand docked"): with the left grip button held AND the left hand docked on the weapon (praydog's was_gripping_weapon),
-- the block gesture is ignored on every gun, reloading or not. two_hands=0 = only during reloads.
local cfg = { on = 1, two_hands = 1 }
local cfg_at = -10
local function read_cfg()
    if os.clock() - cfg_at < CFG_EVERY_S then return end
    cfg_at = os.clock()
    local f = io.open("reframework/data/" .. CFG_FILE, "r")
    if f == nil then return end
    for line in f:lines() do
        local k, v = line:match("^%s*([%w_]+)%s*=%s*([-%d.]+)")
        if k ~= nil and cfg[k] ~= nil and tonumber(v) ~= nil then cfg[k] = tonumber(v) end
    end
    f:close()
end

local reload_t0 = nil      -- os.clock() when the current reload was first seen
local skipped = 0

local function reloading_now()
    local v = rawget(_G, "re8vr")
    if v == nil then return false end
    local ok, r = pcall(function() return v.is_reloading end)
    return ok and r == true
end

re.on_frame(function()
    read_cfg()
    if reloading_now() then
        if reload_t0 == nil then reload_t0 = os.clock() end
    else
        reload_t0 = nil
    end
end)

-- 2026-10-10 00:35, the probe (re8_vrz_reload_probe.lua): a full reload is 2.65 s; every reload that broke ended with
-- praydog's wants_block TRUE, every good one with it false -- and tryGuardStart was never called. So the cancel comes from
-- the guard BUTTON praydog presses for the game (re8_vr.lua: wants_block -> LTrigTop in the pad update), which the reload
-- honours on its own. The gesture is computed in praydog's UpdateBehavior callback and read by the pad hook in the NEXT
-- frame's UpdateHID, so clearing wants_block after UpdateBehavior, while reloading, stops the press before it is made.
local function two_hands_now(v)
    if cfg.two_hands == 0 then return false end
    local ok, r = pcall(function() return v.is_holding_left_grip == true and v.was_gripping_weapon == true end)
    return ok and r == true
end

local withheld_why = nil   -- nil = the block is free; otherwise why it is being withheld (logged on change)
re.on_application_entry("UpdateBehavior", function()
    if cfg.on == 0 then return end
    local v = rawget(_G, "re8vr")
    if v == nil then return end
    local why = nil
    if reloading_now() then why = "reloading"
    elseif two_hands_now(v) then why = "two hands on the weapon (LG held, left hand docked)" end
    local wb = false
    pcall(function() wb = v.wants_block == true end)
    if why ~= nil and wb then
        pcall(function() v.wants_block = false end)
        if withheld_why ~= why then
            withheld_why = why
            L(string.format("block gesture ignored: %s%s", why,
                why == "reloading" and string.format(" (%.2f s into it)", reload_t0 and (os.clock() - reload_t0) or 0) or ""))
        end
    elseif why == nil and withheld_why ~= nil then
        withheld_why = nil
        L("block gesture free again")
    end
end)

local function on_pre_try_guard_start(args)
    if cfg.on == 0 then return end
    if not reloading_now() then return end
    skipped = skipped + 1
    L(string.format("guard #%d skipped: the player is reloading (%.2f s into it) -- the hands-up block gesture would have cancelled the reload",
        skipped, reload_t0 and (os.clock() - reload_t0) or 0))
    return sdk.PreHookResult.SKIP_ORIGINAL
end

local function on_post_try_guard_start(retval)
    return retval
end

local td = sdk.find_type_definition("app.PlayerBase")
local m = td and td:get_method("tryGuardStart")
if m == nil then
    L("app.PlayerBase.tryGuardStart not found -- doing nothing")
else
    sdk.hook(m, on_pre_try_guard_start, on_post_try_guard_start)
    L("hooked app.PlayerBase.tryGuardStart: a guard is skipped while reloading")
end
