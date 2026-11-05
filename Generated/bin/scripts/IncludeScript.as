
class Foo
{
    bool IsInside(Vec2 pos)
    {
        // Find the difference
        float dx = pos.x - m_pos.x;
        float dy = pos.y - m_pos.y;

        // Calculate the distance
        float distance = sqrt(dx*dx + dy*dy);
        if (distance <= m_radius)
        {
            return true;
        }
        return false;
    }

    void ShowPosition()
    {
        DrawCircle(m_pos, m_radius, ColorGrey50);
    }

    void MoveAround()
    {
        switch (m_counter)
        {
            case 0:
                m_pos = Vec2(200.25);
                break;
            case 1:
                m_pos = Vec2(200.25, -200.75);
                break;
            case 2:
                m_pos = Vec2(-200.75,-200.75);
                break;
            case 3:
                m_pos = Vec2(-200.75, 200.25);
                break;
            default:
                m_pos = Vec2(0.0);
        }
        m_counter = (m_counter + 1) % 4;
    }
    int m_counter = 0;
    float m_radius = 10.1;
    Vec2 m_pos = Vec2(200.25);
}

enum EPlayerWingMode
{
    eValue0,
    eValue2 = 2,
    eValue3,
    eValue200 = eValue2 * 100
}

float Lerp(float x, float y, float t)
{
    return x + t * (y - x);
}

float Dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
} 

float Clamp01(float val)
{
    if (val < 0.0)
    {
        val = 0.0;
    }
    else if (val > 1.0)
    {
        val = 1.0;
    }
    return val; 
}

class PlayerMovementCapability : Capability
{
    void Setup()
    {
        if (CreateCapabilityForEntity(GetOwningEntity(), "PlayerWingBeatCapability", "WingCapability"))
        {
            Print("Successfully created the Wing Capability");    
        }
        else
        {
            Print("Did not successfully create the Wing Capability");    
        }
    }

    void OnOwnerDestroyed()
    {
        RemoveCapabilityFromEntity(GetOwningEntity(), "PlayerWingBeatCapability");
    }

    bool ShouldActivate()
    {
        return m_bActiveCapability == true;
    }

    bool ShouldDeactivate()
    {
        return m_bActiveCapability == false;
    }

    void TickActive(float deltaTime)
    {
        Vec2 rightVector = Vec2(1.0, 0.0);
        Vec2 upVector = Vec2(0.0, 1.0);
        PlayerComponent@ playerComponent = GetOrCreatePlayerComponent(GetOwningEntity());

        Vec2 gravityForce = Vec2(0.0, playerComponent.m_gravity * 0.2);
        float drag = playerComponent.m_drag * 0.0;
        float airDensity = 01.00;
        float mass = 1.0;

        // Flip input
        bool bIsReversing = GetButtonInput(eInputAction::PlayerMoveDown, 0) == 2;
        if (GetButtonInput(eInputAction::PlayerMoveDown, 0) == 1)
        {
            // if (!playerComponent.m_isFlipping)
            // {
            //     playerComponent.m_isFlipping = true;
            //     playerComponent.m_flipTimer = 0.0f;
        
            //     playerComponent.m_flipStartRotation = playerComponent.m_currentRotationRadian;
        
            //     playerComponent.m_flipDirection = 1.0f;
            // }
        }
        // Rotation
        float currentFrameRotation = playerComponent.m_currentRotationRadian;
        
        if (playerComponent.m_isFlipping)
        {
            float previousRotation = currentFrameRotation;

            // Calculate new rotation
            playerComponent.m_flipTimer += deltaTime;
            
            float t = Clamp01(playerComponent.m_flipTimer / playerComponent.m_flipDuration);
            
            float smoothT = t * t * (3.0 - 2.0 * t);
            
            currentFrameRotation = playerComponent.m_flipStartRotation + playerComponent.m_flipDirection * 3.14159265 * smoothT;
            
            // Calculate how much we rotated this frame
            float rotationDelta = currentFrameRotation - previousRotation;
            
            // Rotate velocity by the exact same amount
            float cosAngle = cos(rotationDelta);
            float sinAngle = sin(rotationDelta);
            
            Vec2 velocity = playerComponent.m_velocity;
            
            playerComponent.m_velocity = Vec2( velocity.x * cosAngle - velocity.y * sinAngle, velocity.x * sinAngle + velocity.y * cosAngle );
            
            if (t >= 1.0)
            {
                playerComponent.m_isFlipping = false;
            }
        }
        else
        {
            float rotationChange = 4.21f;
        
            if (GetButtonInput(eInputAction::PlayerMoveLeft, 0) == 2)
            {
                currentFrameRotation -= rotationChange * deltaTime;
            }
        
            if (GetButtonInput(eInputAction::PlayerMoveRight, 0) == 2)
            {
                currentFrameRotation += rotationChange * deltaTime;
            }
        }
        
        playerComponent.m_currentRotationRadian = currentFrameRotation;
        playerComponent.m_currentDirection = Vec2(cos(currentFrameRotation), sin(currentFrameRotation));

        float currentVelocity = playerComponent.m_velocity.Length();

        // Directional forces
        bool bIsMovingForward = GetButtonInput(eInputAction::PlayerMoveUp, 0) == 2;
        float currentForwardForce = 0.0;
        bool bIsBeatingWing = GetButtonInput(eInputAction::PlayerAction1, 0) == 1;
        float liftStrength = 0.0;

        Vec2 hoverForce(0.0f, 0.0f);
        float liftFromSpeed = currentVelocity / playerComponent.m_gravity;

        // Cancel vertical gravity and damp vertical velocity.
        hoverForce.y = -gravityForce.y * liftFromSpeed;
        if (bIsBeatingWing)
        {
            bool bOnlyMoveUp = bIsMovingForward == bIsReversing;
            if (!bOnlyMoveUp && bIsMovingForward || bIsReversing)
            {
                float forceToAdd = bIsMovingForward ? 50.0 : -50.0;
                currentForwardForce = (bOnlyMoveUp == false ? forceToAdd : 0.0) * (1.0 - Clamp01(liftFromSpeed));
            }
            liftStrength = 15.0;
        }
        // bool bIsHovering = GetButtonInput(eInputAction::PlayerAction2, 0) == 2;
        // Cancel vertical gravity and damp vertical velocity.
        // hoverForce.y = -gravityForce.y;
        // Higher number on the dampening and the hovering will begin faster and more abruptly
        float hoverDamping = 1.0;
        hoverForce.y = hoverForce.y - playerComponent.m_velocity.y * hoverDamping * mass;
  
        Vec2 dragDirection = playerComponent.m_velocity.GetNormalized() * -1.0;

        Vec2 currentUpwardsDirection = Vec2(-playerComponent.m_currentDirection.y, playerComponent.m_currentDirection.x);
        if (currentUpwardsDirection.y > 0.0)
        {
            currentUpwardsDirection = currentUpwardsDirection * -1.0;
        }
            
        float alignment = Dot(dragDirection * -1.0, playerComponent.m_currentDirection);

        float lift = liftStrength * currentVelocity * currentVelocity;

        Vec2 directionForce = (playerComponent.m_currentDirection * currentForwardForce) + (Vec2(0.0, -1.0) * lift);

        Vec2 totalForce = directionForce + gravityForce + hoverForce;
        Vec2 externalAcceleration = totalForce / mass;

        float dragMagnitude = currentVelocity * currentVelocity * drag * airDensity;
        Vec2 dragAcceleration = (dragDirection) * dragMagnitude / mass;
        playerComponent.m_velocity = playerComponent.m_velocity + (externalAcceleration + dragAcceleration) * deltaTime;


        // Reset everything
        if (GetButtonInput(eInputAction::DebugAction1, 0) == 1)
        {
            m_pos = Vec2(0.0);
            playerComponent.m_velocity = Vec2(0.0);
            playerComponent.m_forwardForce = 0.0;
            playerComponent.m_upwardForce = 0.0;
        }
        
        m_pos = m_pos + playerComponent.m_velocity;

        Transform2D@ playerTransform = GetEntityTransform(GetOwningEntity());
        playerTransform.SetPosition(m_pos);
        
        DrawCircle(m_pos, m_radius, ColorCyan);
        DrawLine(m_pos, m_pos + playerComponent.m_velocity, ColorRed);
        //DrawLine(m_pos, m_pos + directionForce * 1.0, ColorOrange);
        
        DrawLine(m_pos, m_pos + (playerComponent.m_currentDirection * playerComponent.m_forwardForce) * 1.0, ColorOrange);
        DrawLine(m_pos, m_pos + (currentUpwardsDirection) * 50.0, ColorRed);
        DrawLine(m_pos, m_pos + (playerComponent.m_currentDirection) * 50.0, ColorCyan);

    }

    void TickInactive(float deltaTime)
    {
        if (GetButtonInput(eInputAction::PlayerAction2, 0) == 1)
        {
            m_bActiveCapability = true;
        }
    }

    void OnActivated()
    {
        Print("Activated player capability from C++");
    }

    void OnDeactivated()
    {
        Print("Deactivated player capability from C++");
    }

    float m_radius = 10.1;
    Vec2 m_pos = Vec2(0.0);
    bool m_bActiveCapability = true;
    float m_forceToRemovePerFrame = 10.25;

}
