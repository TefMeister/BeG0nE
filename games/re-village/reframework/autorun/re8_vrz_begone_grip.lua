-- re8_vrz_begone_grip.lua -- Dr.BeGonE for Resident Evil Village: the stacked two-handed grip and the drawn left
-- hand. VR only. Split out of the scope mod's re8_vrz_scope_left_grip.lua on 2026-10-01 (Tefa: "the Dr.BeGonE part
-- extracted only for VR"); the code below is unchanged except: the 'grip sound' / 'grip anim' words moved with the
-- probes to the scope's re8_vrz_scope_sounds_menu.lua, and the state line no longer counts silenced clicks.
-- Still gated on the scope's rifle camera (st.clone_go), so today it only acts with the sniper rifle and the scope
-- mod installed. Making it stand alone for every long weapon is the next step (Dr.BeGonE design spec, section 4).
--
-- re8_vrz_scope_left_grip.lua -- ONE left-hand grip spot on the rifle (2026-09-27, Tefa in the headset).
--
-- WHY: Tefa: "when the left hand is gripping the rifle, it has 3 different holding animations - one puts the hand on the
-- rifle iron barrel when holding it at a certain angle, and then sometimes two hand positions seem to be fighting when
-- holding the hand under the rifle". praydog's RE8VR::update_hand_ik (REFramework src/mods/vr/games/RE8VR.cpp) does not use
-- a fixed grip spot: every frame it takes the game ANIMATION's left-hand joint relative to the right hand
-- (`original_left_pos_relative`) and puts the docked left hand there, so every hold animation the game plays (normal, aim,
-- blends) moves the hand [inferred-static 2026-09-27].
--
-- WHAT: after REFramework's last update_hand_ik of the frame (re8_vr.lua, pre-PrepareRendering -- this file is named re8_vrz_*
-- so it loads, and registers, after re8_vr.lua), while the left hand is docked on the weapon, the left IK target is put back
-- at ONE spot in the right hand's frame -- the one captured with `grip capture` -- and the IK is solved again.
--
-- Words (harness):
--   grip probe 1|0   log the game's current grip spot (right-hand frame) whenever it moves > GRIP_PROBE_STEP
--   grip capture     keep the spot the hand is in RIGHT NOW (hold the rifle the way that looks right first); saved
--   grip capture <s> the same after <s> seconds, so the wearer can settle into the grip first (the probe stays quiet meanwhile)
--   grip on|off      apply the captured spot / leave the game's own
--   grip             print the state
-- File: reframework/data/re_scope_left_grip.txt  ("on px py pz qw qx qy qz"). No file = off.
-- Only while the rifle camera exists (st.clone_go), i.e. only with the sniper rifle out.

local TAG = "[re8-grip] "
local function L(s) log.info(TAG .. tostring(s)) end
local function safe(f) local ok, r = pcall(f) if ok then return r end return nil end

local GRIP_FILE = "re_scope_left_grip.txt"
local GRIP_PROBE_STEP = 0.01     -- metres: a grip spot change smaller than this is not logged
local GRIP_PROBE_MIN_S = 0.1     -- seconds between probe lines

local g = { on = false, pos = nil, rot = nil, probe = false, last = nil, last_t = 0.0, applied = 0, fingers = {} }

-- 2026-09-27 (Tefa, second wear): the hand stayed put, but "is still changing finger animations, like it was holding on
-- to a different part" -- the finger curl comes from the game's animation too. So the captured grip also keeps the finger
-- joints' local rotations, written back every frame after the IK solve. Names read live 2026-09-27 from the player's joints.
local FINGER_JOINTS = { "L_Thumb1", "L_Thumb2", "L_Thumb3", "L_IndexF1", "L_IndexF2", "L_IndexF3", "L_MiddleF1", "L_MiddleF2",
    "L_MiddleF3", "L_RingF1", "L_RingF2", "L_RingF3", "L_PinkyF1", "L_PinkyF2", "L_PinkyF3" }
local joint_cache = {}   -- name -> via.Joint (refreshed when the player transform changes)
local joint_cache_tf = nil

local function finger_joint(name)
    local v = rawget(_G, "re8vr")
    local tf = v ~= nil and v.transform or nil
    if tf == nil then return nil end
    if tf ~= joint_cache_tf then joint_cache, joint_cache_tf = {}, tf end
    local j = joint_cache[name]
    if j == nil then
        j = safe(function() return tf:call("getJointByName", name) end)
        joint_cache[name] = j
    end
    return j
end

local function st() return _G.re8_scope_st end
local function vr() return rawget(_G, "re8vr") end

local function save()
    if g.pos == nil or g.rot == nil then return end
    local s = string.format("%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n", g.on and 1 or 0, g.pos.x, g.pos.y, g.pos.z,
        g.rot.w, g.rot.x, g.rot.y, g.rot.z)
    for _, n in ipairs(FINGER_JOINTS) do
        local q = g.fingers[n]
        if q ~= nil then s = s .. string.format("f %s %.6f %.6f %.6f %.6f\n", n, q.w, q.x, q.y, q.z) end
    end
    pcall(fs.write, GRIP_FILE, s)
end

local function load()
    local s = safe(function() return fs.read(GRIP_FILE) end)
    if s == nil or s == "" then return end
    local first = s:match("^[^\n]*") or ""
    for n, w, x, y, z in s:gmatch("f%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)%s+(%S+)") do
        g.fingers[n] = Quaternion.new(tonumber(w), tonumber(x), tonumber(y), tonumber(z)):normalized()
    end
    local v = {}
    for w in first:gmatch("%S+") do v[#v + 1] = tonumber(w) end
    if #v < 8 then return end
    g.on = v[1] == 1
    g.pos = Vector3f.new(v[2], v[3], v[4])
    g.rot = Quaternion.new(v[5], v[6], v[7], v[8]):normalized()
    L(string.format("loaded: %s, spot (%.3f, %.3f, %.3f)", g.on and "ON" or "off", g.pos.x, g.pos.y, g.pos.z))
end

-- the current hand targets, as REFramework set them this frame
local function hands()
    local v = vr()
    if v == nil then return nil end
    local lt, rt = v.left_hand_ik_transform, v.right_hand_ik_transform
    if lt == nil or rt == nil then return nil end
    local lp = safe(function() return lt:call("get_Position") end)
    local lr = safe(function() return lt:call("get_Rotation") end)
    local rp = safe(function() return rt:call("get_Position") end)
    local rr = safe(function() return rt:call("get_Rotation") end)
    if lp == nil or lr == nil or rp == nil or rr == nil then return nil end
    return lp, lr, rp, rr
end

local function relative(lp, lr, rp, rr)
    local inv = rr:inverse()
    return inv * (lp - rp), (inv * lr):normalized()
end

local function capture_now()
    local lp, lr, rp, rr = hands()
    if lp == nil then L("capture: no hand targets yet") return end
    g.pos, g.rot = relative(lp, lr, rp, rr)
    g.on = true
    local nf = 0
    for _, n in ipairs(FINGER_JOINTS) do
        local j = finger_joint(n)
        local q = j ~= nil and safe(function() return j:call("get_LocalRotation") end) or nil
        if q ~= nil then g.fingers[n] = q nf = nf + 1 end
    end
    save()
    L(string.format("capture: spot (%.3f, %.3f, %.3f) and %d finger joint(s) kept, ON; docked=%s", g.pos.x, g.pos.y, g.pos.z,
        nf, tostring(vr() and vr().was_gripping_weapon)))
end

local function fingers_apply()
    for n, q in pairs(g.fingers) do
        local j = finger_joint(n)
        if j ~= nil then safe(function() j:call("set_LocalRotation", q) end) end
    end
end

local function grip(a, b)
    if a == "capture" and tonumber(b) then
        g.capture_at = os.clock() + tonumber(b)
        g.on = false   -- the game's own spot while the wearer settles, so the capture reads it, not ours
        L("capture in " .. b .. " s -- hold the grip you want")
    elseif a == "probe" then
        g.probe = not g.probe
        L("probe " .. (g.probe and "ON" or "off"))
    elseif a == "capture" then
        capture_now()
    elseif a == "on" then
        if g.pos == nil then L("on: nothing captured yet -- grip capture first") return end
        g.on = true save() L("ON")
    elseif a == "off" then
        g.on = false save() L("off: the game's own grip spot")
    else
        L(string.format("state: %s, spot %s, probe %s, applied %d frames", g.on and "ON" or "off",
            g.pos and string.format("(%.3f, %.3f, %.3f)", g.pos.x, g.pos.y, g.pos.z) or "none", tostring(g.probe), g.applied))
    end
end

local function probe_tick(lp, lr, rp, rr)
    local p = relative(lp, lr, rp, rr)
    local now = os.clock()
    if now - g.last_t < GRIP_PROBE_MIN_S then return end
    if g.last ~= nil and (p - g.last):length() < GRIP_PROBE_STEP then return end
    g.last, g.last_t = p, now
    local v = vr()
    L(string.format("probe: game grip spot (%.3f, %.3f, %.3f) docked=%s holding=%s reloading=%s", p.x, p.y, p.z,
        tostring(v.was_gripping_weapon), tostring(v.is_holding_left_grip), tostring(v.is_reloading)))
end

-- ---- THE STACKED GRIP, in our own file (2026-09-27). Tefa's rule: players install praydog's REFramework and our mod only
-- adds files. The stacked grip lived in our patched loader (grip-no-throw.patch, RE8VR::update_hand_ik, worn 2026-09-22:
-- "the occlusion drift is no more"): with the LEFT GRIP BUTTON held and the left controller 3-35 cm above the right one
-- (within 12 cm sideways to enter, 18 cm to leave), the left hand counts as holding the rifle -- drawn on the forestock --
-- while the rifle is aimed by the right hand only. On stock REFramework a hand held there is NOT docked (it is far from the
-- animation's grip socket), so the rifle already follows the right hand alone; all that is missing is the drawn hand, which
-- the grip spot below provides. Measured on the controllers' own tracked positions (tracking space, metres, y up).
local STACK_UP_MIN_M, STACK_UP_MAX_M = 0.03, 0.35
local STACK_SIDE_ENTER_M, STACK_SIDE_LEAVE_M = 0.12, 0.18
g.stacked = false
local function stacked_zone(v)
    if not v.is_holding_left_grip or v.is_reloading then return false end
    local ok, l, r = pcall(function()
        local c = vrmod:get_controllers()
        return vrmod:get_transform(c[1])[3], vrmod:get_transform(c[2])[3]
    end)
    if not ok or l == nil or r == nil then return false end
    local up = l.y - r.y
    local side = math.sqrt((l.x - r.x) ^ 2 + (l.z - r.z) ^ 2)
    local limit = g.stacked and STACK_SIDE_LEAVE_M or STACK_SIDE_ENTER_M
    local now = up >= STACK_UP_MIN_M and up <= STACK_UP_MAX_M and side <= limit
    if now ~= g.stacked then
        L(string.format("stacked grip %s (%.0f cm above, %.0f cm to the side)", now and "TAKEN" or "released", up * 100, side * 100))
        g.stacked = now
    end
    return now
end

re.on_pre_application_entry("PrepareRendering", function()
    local s = st()
    if s == nil or s.clone_go == nil then return end
    local v = vr()
    if v == nil or v.is_reloading then return end
    local stacked = stacked_zone(v)
    if not v.was_gripping_weapon and not stacked then return end
    local lp, lr, rp, rr = hands()
    if lp == nil then return end
    if g.probe then probe_tick(lp, lr, rp, rr) end
    if g.capture_at ~= nil and os.clock() >= g.capture_at then g.capture_at = nil capture_now() return end
    if not g.on or g.pos == nil then return end
    local ik, lt = v.left_hand_ik, v.left_hand_ik_transform
    local pos = rp + rr * g.pos
    local rot = (rr * g.rot):normalized()
    safe(function() v:set_hand_joints_to_tpose(ik) end)
    safe(function() lt:call("set_Position", pos) end)
    safe(function() lt:call("set_Rotation", rot) end)
    safe(function() ik:set_field("Transition", 1.0) end)
    safe(function() ik:call("calc") end)
    fingers_apply()
    g.applied = g.applied + 1
end)

load()
_G.re8_scope_left_grip = grip
L("loaded -- words: grip probe | grip capture | grip on | grip off | grip")
