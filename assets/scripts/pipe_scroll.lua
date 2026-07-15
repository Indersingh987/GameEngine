-- Shared by every pipe entity. Scrolls left each frame; once off-screen, destroys
-- itself and spawns a fresh pipe at the right edge with a randomized height/color -
-- this is what validates spawnEntity/destroyEntity/setColor end-to-end, not just
-- getPosition/setPosition.
-- Note: the newly spawned pipe has no ScriptComponent (spawnEntity doesn't accept a
-- script path in the current final binding surface), so it won't keep scrolling on
-- its own - a deliberate limitation, not a bug. Re-attach pipe_scroll.lua to it via
-- the editor if continuous recycling is needed.
local SCROLL_SPEED = 150   -- pixels/s
local WINDOW_WIDTH = 800
local WINDOW_HEIGHT = 600
local PIPE_WIDTH = 80

-- Manual overlap check (temporary stand-in for real Box2D onCollision - see Feature #4
-- step 8's reordering rationale in CLAUDE.md; deferred until it's actually needed).
-- getPosition only returns x,y, not size, so PLAYER_WIDTH_ESTIMATE is a rough guess, and
-- this only checks horizontal (x-axis) proximity, not a real AABB against the pipe gap.
local PLAYER_WIDTH_ESTIMATE = 50
local HIT_MARGIN = 10
local hitState = {} -- per-entity, so one pipe being hit doesn't freeze the others

function onUpdate(entity, deltaTime)
    if hitState[entity] then
        return -- frozen after being hit
    end

    local x, y = getPosition(entity)
    local newX = x - SCROLL_SPEED * deltaTime
    setPosition(entity, newX, y)

    local player = getEntityByRole("player")
    if player ~= 0 then
        local px, _ = getPosition(player)
        local overlapX = newX < px + PLAYER_WIDTH_ESTIMATE + HIT_MARGIN and
                          newX + PIPE_WIDTH + HIT_MARGIN > px
        if overlapX then
            hitState[entity] = true
            setColor(entity, 220, 40, 40, 255)
            return
        end
    end

    if newX < -100 then
        destroyEntity(entity)

        local newHeight = 150 + math.random(0, 200)
        local newY = math.random(0, WINDOW_HEIGHT - newHeight)
        -- Spawn fully on-screen at the right edge (TransformComponent is top-left, so
        -- x = WINDOW_WIDTH would place it entirely offscreen at 800-880 and never be seen).
        local newPipe = spawnEntity(WINDOW_WIDTH - PIPE_WIDTH, newY, PIPE_WIDTH, newHeight, "Kinematic")
        setColor(newPipe, 60, 180, 80, 255)
    end
end
