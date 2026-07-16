-- Game script: exactly one per project, persists for the whole application lifetime.
-- Owns settings, high scores, save points, the main menu, and deciding the active scene.
-- Called into from Scene scripts via callScript("game", functionName, ...).

-- Threshold deliberately does NOT match the 42 that main_scene.lua's onEntityPinged test call
-- sends - switchScene destroys the current scene immediately (see Known Gotcha #17), which would
-- otherwise short-circuit hierarchy_test.lua's own entity-ack verification leg on every run. To
-- exercise switchScene specifically, temporarily raise the score main_scene.lua sends past 100.
function onScoreReported(score)
    print("[GameScript] onScoreReported: " .. tostring(score))

    if score >= 100 then
        local ok = switchScene("assets/scene2.json", "assets/scripts/scene/scene2_scene.lua")
        print("[GameScript] switchScene result: " .. tostring(ok))
    end

    return "acknowledged"
end
