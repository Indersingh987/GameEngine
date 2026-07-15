-- Game script: exactly one per project, persists for the whole application lifetime.
-- Owns settings, high scores, save points, the main menu, and deciding the active scene.
-- Called into from Scene scripts via callScript("game", functionName, ...).

function onScoreReported(score)
    print("[GameScript] onScoreReported: " .. tostring(score))
    return "acknowledged"
end
