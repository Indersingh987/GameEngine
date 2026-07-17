-- Scene script for scene2.json - only needs to exist and load cleanly for the
-- switchScene() test chain (see main_game.lua's onScoreReported); no entity logic yet.

function onEntityPinged(entityId)
    print("[Scene2Script] onEntityPinged from entity " .. tostring(entityId))
    return "scene2-ack"
end
