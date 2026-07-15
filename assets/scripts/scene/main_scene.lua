-- Scene script: exactly one per scene. Mediates all entity scripts within this scene.
-- Called into from entity scripts via callScript("scene", functionName, ...), and reports
-- upward to the Game script via callScript("game", functionName, ...).

function onEntityPinged(entityId)
    print("[SceneScript] onEntityPinged from entity " .. tostring(entityId))
    local ack = callScript("game", "onScoreReported", 42)
    print("[SceneScript] game acknowledged: " .. tostring(ack))

    local entityAck = callScript("entity", "onPingAcknowledged", entityId, "hello from scene")
    print("[SceneScript] entity acknowledged: " .. tostring(entityAck))

    return "scene-ack"
end
