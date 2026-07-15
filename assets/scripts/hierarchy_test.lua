-- Test-only entity script exercising the entity -> scene -> game -> scene -> entity callScript
-- chain end-to-end. Assign via the Create Entity dialog's Script Path field to verify the
-- hierarchy in Play mode.
local pinged = false

function onPingAcknowledged(entity, message)
    print("[hierarchy_test] onPingAcknowledged on entity " .. tostring(entity) .. ": " .. tostring(message))
    return "entity-ack"
end

function onUpdate(entity, deltaTime)
    if pinged then
        return
    end
    pinged = true

    local result = callScript("scene", "onEntityPinged", entity)
    print("[hierarchy_test] scene acknowledged: " .. tostring(result))

    -- Forbidden path - entities may not target "game" directly. Should print a callScript
    -- rejection to stderr and return nil, not crash or silently succeed.
    local rejected = callScript("game", "onScoreReported", 999)
    print("[hierarchy_test] forbidden call returned: " .. tostring(rejected))
end
