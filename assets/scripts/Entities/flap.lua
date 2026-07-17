-- Flappy-Bird-style vertical control, meant for the player entity.
-- Manages gravity/flap fully in script via getVelocity/setVelocity (m/s), rather than
-- PhysicsComponent.gravityScale - the editor has no UI to set gravityScale non-zero yet,
-- so this script owns vertical motion entirely instead.
local GRAVITY = 25.0        -- m/s^2
local FLAP_VELOCITY = -6.0  -- m/s, negative = up
local MAX_FALL_SPEED = 10.0 -- m/s
local WINDOW_HEIGHT = 600

local wasSpacePressed = {}  -- per-entity, in case this script ever ends up shared

function onUpdate(entity, deltaTime)
    local vx, vy = getVelocity(entity)
    local spacePressed = isKeyPressed("Space")

    if spacePressed and not wasSpacePressed[entity] then
        vy = FLAP_VELOCITY
        playSound("jump")
    else
        vy = math.min(vy + GRAVITY * deltaTime, MAX_FALL_SPEED)
    end
    wasSpacePressed[entity] = spacePressed

    setVelocity(entity, vx, vy)

    -- No world boundaries in the engine yet (Known Gotcha #7) - clamp here in script.
    local x, y = getPosition(entity)
    if y < 0 then
        setPosition(entity, x, 0)
        setVelocity(entity, vx, 0)
    elseif y > WINDOW_HEIGHT then
        setPosition(entity, x, WINDOW_HEIGHT)
        setVelocity(entity, vx, 0)
    end
end

