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
-- WHAT: while the game says the player is reloading, tryGuardStart is skipped (the same hook praydog uses to allow the
-- guard only by gesture; REFramework chains both). The guard button press still goes through (praydog's pad code is not
-- touched), so if the press alone also cancels the reload, this script's log line will show the guard skipped and the
-- reload still ending early -- then the next lever is the pad press itself.
-- Log: [re8-reload-guard] one line per skipped guard, with how long the reload has been running.
-- Settings file: reframework/data/re_scope_reload_guard.txt, `on=1` (default) / `on=0` = do nothing. Read every second.

local TAG = "[re8-reload-guard] "
local function L(s) log.info(TAG .. tostring(s)) end
local CFG_FILE = "re_scope_reload_guard.txt"
local CFG_EVERY_S = 1.0

local cfg = { on = 1 }
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
