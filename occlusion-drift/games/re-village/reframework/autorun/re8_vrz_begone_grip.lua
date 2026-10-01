-- re8_vrz_begone_grip.lua -- Dr.BeGonE for Resident Evil Village: the stacked two-handed grip and the drawn left
-- hand. VR only. Split out of the scope mod's re8_vrz_scope_left_grip.lua on 2026-10-01 (Tefa: "the Dr.BeGonE part
-- extracted only for VR"); the code below is unchanged except: the 'grip sound' / 'grip anim' words moved with the
-- probes to the scope's re8_vrz_scope_sounds_menu.lua, and the state line no longer counts silenced clicks.
-- Still gated on the scope's rifle camera (st.clone_go), so today it only acts with the sniper rifle and the scope
-- mod installed. Making it stand alone for every long weapon is the next step (Dr.BeGonE design spec, section 4).
--
-- 2026-10-01 (evening, home PC): THE TWO FIXES FROM THE PATCHED LOADER NOW LIVE HERE -- see "THE TWO FIXES" below.
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
--   grip button 1|0  the left grip button is the only way to take the grip (1, default) / praydog's 10 cm dock too (0)
--   grip freeze 1|0  steer against the socket frozen at the take (1, default) / the live animation socket (0)
--   grip check       print the pose check (our recomputed right hand against REFramework's own)
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

-- ---- THE TWO FIXES (2026-10-01, Tefa: "make the gun not jump left when shooting two handed and also make the gun only
-- grippable by LG" -- LG is the left grip button).
--
-- praydog's RE8VR::update_hand_ik (stock REFramework a24c3459, the loader players install) decides the two-handed grip
-- every frame:  docked = the left hand within 10 cm of the ANIMATION's grip socket, OR (was docked AND the left grip
-- button is held).  While docked it turns the rifle so the socket line matches the real hand line -- measured against
-- the socket the animation gives it THAT frame. Two faults, both worn and fixed in our patched loader on 2026-09-21/22
-- (re-village-scope-vr dossier 9cg and 9ci, grip-no-throw.patch), lost again when the loader went back to stock:
--   1. THE JUMP. The first trigger pull after a take plays the firing pose, the animation's socket moves, and the gun
--      follows it: 4.6-5.1 deg on every first shot after a take, 0.6-0.8 deg on shots with the hand kept on
--      [verified-live 2026-09-21, n=10 shots]. Fix: the socket the steering measures against is FROZEN at the take.
--      The animation may move its own hand; the gun does not follow it.
--   2. DOCKING WITHOUT THE BUTTON. Anything within 10 cm of the socket docked, while the hand was still travelling up to
--      the rifle, and the rest of the travel became steering. Fix: no left grip button, no grip. The button is the switch.
-- The loader is stock now, so both are done HERE, right after update_hand_ik has run (three times a frame, where re8_vr.lua
-- calls it): the controller poses are computed again the way the C++ does (same inputs, same offsets), the grip decision
-- is remade, and both hands' IK targets are set once more. The recompute PROVES ITSELF: the right hand's position from
-- the C++ is compared with ours on every pass (and its rotation too, whenever the C++ is not steering), the log says when
-- 300 passes agreed, and shouts if they ever do not. `grip check` prints the figures.
-- Steering is the shortest turn from the frozen socket line to the real hand line (no roll invented, the right wrist
-- keeps its roll) -- the maths the loader's corrected build used from 2026-09-21 19:00 on. [hypothesis] that it feels the
-- same as that build; the first wear says.
local GRIP_NEEDS_BUTTON  = true    -- fix 2: the left grip button is the only way to dock
local GRIP_FREEZE_SOCKET = true    -- fix 1: steer against the socket frozen at the take
local POSE_CHECK_WARN_M   = 0.003  -- metres: our recomputed right hand further than this from REFramework's = wrong
local POSE_CHECK_WARN_DEG = 0.5    -- degrees: same for its rotation (checked only when the C++ is not steering)
local POSE_CHECK_EVERY_S  = 5.0    -- seconds between warnings
local POSE_CHECK_PASSES   = 300    -- agreeing passes before the recompute is called right
local DRIFT_LOG_STEP_DEG  = 1.0    -- log the animation moving the socket each time it grows by this much

local S = { take = nil, takes = 0, undone = 0, undone_logged = false, steer_deg = 0, drift_deg = 0, drift_max = 0 }
local chk = { ok = 0, bad = 0, worst_m = 0, worst_deg = 0, last_t = 0, verified = false }

local function v3(a) return Vector3f.new(a.x, a.y, a.z) end
local function unit(a)
    local l = math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z)
    if l < 1e-6 then return nil end
    return Vector3f.new(a.x / l, a.y / l, a.z / l)
end
local function quat_deg(q)   -- the turn a unit quaternion carries, in degrees
    return math.deg(2 * math.acos(math.min(1, math.abs(q.w))))
end
local function angle_between_deg(a, b)   -- two unit vectors
    local d = a.x * b.x + a.y * b.y + a.z * b.z
    return math.deg(math.acos(math.max(-1, math.min(1, d))))
end
-- the shortest turn taking unit vector a onto unit vector b; no roll about the line
local function shortest_arc(a, b)
    local d = a.x * b.x + a.y * b.y + a.z * b.z
    if d < -0.9999 then return Quaternion.new(1, 0, 0, 0) end   -- opposite: refuse rather than flip the gun
    local cx = a.y * b.z - a.z * b.y
    local cy = a.z * b.x - a.x * b.z
    local cz = a.x * b.y - a.y * b.x
    return Quaternion.new(1 + d, cx, cy, cz):normalized()
end
_G.re8_begone_grip_maths = { shortest_arc = shortest_arc, quat_deg = quat_deg, angle_between_deg = angle_between_deg }

-- the controller poses the way RE8VR::update_hand_ik computes them: world position and rotation of each hand target
-- BEFORE any grip steering. Same inputs (camera world matrix + HMD transform, the controllers' tracking-space transforms,
-- re8_vr.lua's hand offsets), same order of operations.
local function controller_poses(v)
    local c = vrmod:get_controllers()
    if c == nil or #c < 2 then return nil end
    local lct, rct, hmd = vrmod:get_transform(c[1]), vrmod:get_transform(c[2]), vrmod:get_transform(0)
    local cam = sdk.get_primary_camera()
    if cam == nil or lct == nil or rct == nil or hmd == nil then return nil end
    local m = cam:call("get_WorldMatrix")
    if m == nil then return nil end
    local cam_rot, cam_pos = m:to_quat(), m[3]
    vrmod:apply_hmd_transform(cam_rot, cam_pos)
    cam_rot = (cam_rot * hmd:to_quat():inverse()):normalized()
    local cp = v3(cam_pos)
    local h3 = hmd[3]
    local function hand(ct, rot_off, pos_off)
        local crot = ct:to_quat()
        local c3 = ct[3]
        local off = Vector3f.new(c3.x - h3.x, c3.y - h3.y, c3.z - h3.z)
        local rot = (cam_rot * crot * rot_off):normalized()
        local pos = cp + cam_rot * off + (cam_rot * crot):normalized() * v3(pos_off)
        return pos, rot
    end
    local lp, lr = hand(lct, v.left_hand_rotation_offset, v.left_hand_position_offset)
    local rp, rr = hand(rct, v.right_hand_rotation_offset, v.right_hand_position_offset)
    return lp, lr, rp, rr
end

local function pose_check(rp, rr, rp_ik, rr_ik, cpp_steering)
    local dm = (rp - v3(rp_ik)):length()
    chk.worst_m = math.max(chk.worst_m, dm)
    local ddeg = nil
    if not cpp_steering then
        ddeg = quat_deg((rr:inverse() * rr_ik):normalized())
        chk.worst_deg = math.max(chk.worst_deg, ddeg)
    end
    local bad = dm > POSE_CHECK_WARN_M or (ddeg ~= nil and ddeg > POSE_CHECK_WARN_DEG)
    if bad then chk.bad = chk.bad + 1 else chk.ok = chk.ok + 1 end
    local now = os.clock()
    if bad and now - chk.last_t >= POSE_CHECK_EVERY_S then
        chk.last_t = now
        L(string.format("POSE CHECK FAILED: our right hand is %.1f mm%s from REFramework's own -- the recompute is WRONG and the two fixes are steering on bad numbers (grip button 0 and grip freeze 0 put praydog's behaviour back)",
            dm * 1000, ddeg ~= nil and string.format(" / %.2f deg", ddeg) or ""))
    elseif not bad and not chk.verified and chk.ok >= POSE_CHECK_PASSES then
        chk.verified = true
        L(string.format("pose check: %d passes within %.1f mm / %.2f deg of REFramework's own right hand -- the recompute is right",
            chk.ok, chk.worst_m * 1000, chk.worst_deg))
    end
end

local function set_ik(v, ik, tf, pos, rot)
    safe(function() v:set_hand_joints_to_tpose(ik) end)
    safe(function() tf:call("set_Position", pos) end)
    safe(function() tf:call("set_Rotation", rot) end)
    safe(function() ik:set_field("Transition", 1.0) end)
    safe(function() ik:call("calc") end)
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
    elseif a == "button" then
        GRIP_NEEDS_BUTTON = b ~= "0" and b ~= "off"
        L("button: " .. (GRIP_NEEDS_BUTTON and "the left grip button is the only way to dock" or "praydog's 10 cm dock allowed too"))
    elseif a == "freeze" then
        GRIP_FREEZE_SOCKET = b ~= "0" and b ~= "off"
        L("freeze: " .. (GRIP_FREEZE_SOCKET and "steering against the socket frozen at the take" or "steering against the live animation socket (praydog's)"))
    elseif a == "check" then
        L(string.format("pose check: %d passes agree, %d disagree, worst %.1f mm / %.2f deg, %s", chk.ok, chk.bad,
            chk.worst_m * 1000, chk.worst_deg, chk.verified and "VERIFIED" or "not yet verified"))
    else
        L(string.format("state: %s, spot %s, probe %s, applied %d frames | button-only %s, frozen socket %s | takes %d, docks undone %d, steering %.2f deg, socket drift %.2f deg (max %.2f) | pose check %s",
            g.on and "ON" or "off", g.pos and string.format("(%.3f, %.3f, %.3f)", g.pos.x, g.pos.y, g.pos.z) or "none",
            tostring(g.probe), g.applied, tostring(GRIP_NEEDS_BUTTON), tostring(GRIP_FREEZE_SOCKET), S.takes, S.undone,
            S.steer_deg, S.drift_deg, S.drift_max, chk.verified and "ok" or (chk.bad > 0 and "FAILING" or "pending")))
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

-- One pass, after REFramework's update_hand_ik. `final` = the PrepareRendering pass, the last of the frame, where the
-- once-a-frame things (probe, timed capture) run.
local function grip_pass(final)
    local s = st()
    if s == nil or s.clone_go == nil then return end
    local v = vr()
    if v == nil then return end
    if not safe(function() return vrmod:is_hmd_active() and vrmod:is_using_controllers() end) then return end
    if v.is_reloading then S.take = nil return end   -- the C++ parks both hands on the socket; leave it
    local stacked = stacked_zone(v)
    local cpp_grip = v.was_gripping_weapon
    local holding = v.is_holding_left_grip
    if not cpp_grip and not stacked and chk.verified then S.take = nil return end
    local lp_ik, lr_ik, rp_ik, rr_ik = hands()
    if lp_ik == nil then return end
    local lp, lr, rp, rr = safe(function() return controller_poses(v) end)
    if lp == nil then return end
    pose_check(rp, rr, rp_ik, rr_ik, cpp_grip)
    if not cpp_grip and not stacked then S.take = nil return end
    if final and g.probe then probe_tick(lp_ik, lr_ik, rp_ik, rr_ik) end
    if final and g.capture_at ~= nil and os.clock() >= g.capture_at then g.capture_at = nil capture_now() return end

    -- FIX 2: the C++ docked the hand (it came within 10 cm of the socket) but the left grip button is not held: undo it.
    -- Both hands go back to where the controllers are, the rifle keeps the right hand's own aim.
    if cpp_grip and GRIP_NEEDS_BUTTON and not holding then
        set_ik(v, v.right_hand_ik, v.right_hand_ik_transform, rp, rr)
        set_ik(v, v.left_hand_ik, v.left_hand_ik_transform, lp, lr)
        safe(function() v.was_gripping_weapon = false end)
        S.take = nil
        S.undone = S.undone + 1
        if not S.undone_logged then
            S.undone_logged = true
            L("the game docked the left hand without the left grip button -- undone; hold the button to take the grip")
        end
        return
    end
    S.undone_logged = false

    if cpp_grip then
        -- the animation's socket this frame, in the right hand's frame, read back from what the C++ just set
        local inv = rr_ik:inverse()
        local socket_live = inv * (v3(lp_ik) - v3(rp_ik))
        local lrot_live = (inv * lr_ik):normalized()
        if S.take == nil then
            S.takes = S.takes + 1
            S.take = { socket = socket_live, lrot = lrot_live, drift_max = 0 }
            S.drift_max = 0
            L(string.format("grip taken #%d with the button: socket frozen at (%.3f, %.3f, %.3f) in the right hand's frame%s",
                S.takes, socket_live.x, socket_live.y, socket_live.z, stacked and " (stacked: the right hand aims alone)" or ""))
        end
        -- FIX 1: the socket the steering measures against is the one frozen at the take
        local socket_ref = GRIP_FREEZE_SOCKET and S.take.socket or socket_live
        local a, b = unit(S.take.socket), unit(socket_live)
        if a ~= nil and b ~= nil then
            S.drift_deg = angle_between_deg(a, b)
            if S.drift_deg > S.take.drift_max + DRIFT_LOG_STEP_DEG then
                S.take.drift_max = S.drift_deg
                S.drift_max = math.max(S.drift_max, S.drift_deg)
                L(string.format("grip #%d: the ANIMATION has moved the socket %.2f deg under the held grip -- %s", S.takes, S.drift_deg,
                    GRIP_FREEZE_SOCKET and "IGNORED, the gun does not move" or "FOLLOWED (freeze off)"))
            end
        end
        local steer = Quaternion.new(1, 0, 0, 0)
        if not stacked then
            local frozen_dir = unit(rr * socket_ref)
            local hand_dir = unit(lp - rp)
            if frozen_dir ~= nil and hand_dir ~= nil then steer = shortest_arc(frozen_dir, hand_dir) end
        end
        S.steer_deg = quat_deg(steer)
        local rr_new = (steer * rr):normalized()
        set_ik(v, v.right_hand_ik, v.right_hand_ik_transform, rp, rr_new)
        local pos, rot
        if g.on and g.pos ~= nil then
            pos, rot = rp + rr_new * g.pos, (rr_new * g.rot):normalized()
        else
            pos, rot = rp + rr_new * socket_ref, (rr_new * S.take.lrot):normalized()
        end
        set_ik(v, v.left_hand_ik, v.left_hand_ik_transform, pos, rot)
        if g.on and g.pos ~= nil then fingers_apply() end
        g.applied = g.applied + 1
        return
    end

    -- stacked only, the C++ did not dock (the hand is far from the socket): the rifle already follows the right hand
    -- alone; draw the left hand at the captured spot, as before the fixes
    S.take = nil
    if not g.on or g.pos == nil then return end
    set_ik(v, v.left_hand_ik, v.left_hand_ik_transform, rp_ik + rr_ik * g.pos, (rr_ik * g.rot):normalized())
    fingers_apply()
    g.applied = g.applied + 1
end

-- re8_vr.lua calls update_hand_ik after UpdateMotion, after LateUpdateBehavior and before PrepareRendering; this file
-- registers after it (the re8_vrz_ name), so each of these runs right after that call.
re.on_application_entry("UpdateMotion", function() pcall(grip_pass, false) end)
re.on_application_entry("LateUpdateBehavior", function() pcall(grip_pass, false) end)
re.on_pre_application_entry("PrepareRendering", function() pcall(grip_pass, true) end)

load()
_G.re8_scope_left_grip = grip
L("loaded -- words: grip probe | grip capture | grip on | grip off | grip button | grip freeze | grip check | grip")
