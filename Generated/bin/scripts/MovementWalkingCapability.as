
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

class PlayerGroundMovementCapability : Capability
{

    void Setup()
    {
    }

    bool ShouldActivate()
    {
        PlayerComponent@ playerComponent = GetOrCreatePlayerComponent(GetOwningEntity());
        return playerComponent.m_bIsLanded == true;
    }

    bool ShouldDeactivate()
    {
        PlayerComponent@ playerComponent = GetOrCreatePlayerComponent(GetOwningEntity());
        return playerComponent.m_bIsLanded == false;
    }

    void TickActive(float deltaTime)
    {
        PlayerComponent@ playerComponent = GetOrCreatePlayerComponent(GetOwningEntity());
        
        Transform2D@ playerTransform = GetEntityTransform(GetOwningEntity());
        Vec2 playerPos = playerTransform.GetPosition();

        // TODO check for collisions underneath.

        if (GetButtonInput(eInputAction::PlayerMoveUp, 0) == 1)
        {
            m_bWingInMovement = true;
        }

        if (m_bWingInMovement)
        {
            m_beatTimer += deltaTime;
            float t = Clamp01(m_beatTimer / m_beatDuration);
            m_wingBeatPos = sin((t * 3.14159265 * 2.0) - 3.14159265);

            // string debugText = "T value: ";
            // debugText += m_wingBeatPos;
            // Print(debugText);
            if (t >= 1.0)
            {
                m_bWingInMovement = false;
                m_beatTimer = 0;
                m_wingBeatPos = 0.0;
            }
        }
        Vec2 modifiedWingTipPos = Vec2(m_wingTipPos.x, m_wingTipPos.y + m_wingBeatPos * 70.0);

        float liftStrength = 0.0;
        bool bIsBeatingWing = GetButtonInput(eInputAction::PlayerAction1, 0) == 1;
        if (bIsBeatingWing)
        {
            liftStrength = 15.0;
            playerComponent.m_bIsLanded = false;
        }            
        Array<PhysicsCollisionEvent>@ events = GetCollisionEventsForEntity(GetOwningEntity());
        if (events is null)
        {
            playerComponent.m_bIsLanded = false;
        }
        float currentVelocity = playerComponent.m_velocity.Length();
        float lift = liftStrength * currentVelocity * currentVelocity;
        
        float mass = 1.0;
        Vec2 directionForce = Vec2(0.0, -1.0) * lift;
        Vec2 totalForce = directionForce;
        Vec2 externalAcceleration = totalForce / mass;

        float speedHorizontal = 0;

        if (GetButtonInput(eInputAction::PlayerMoveLeft, 0) == 2)
        {
            speedHorizontal -= playerComponent.m_speed * deltaTime * 8.0;
        }
    
        if (GetButtonInput(eInputAction::PlayerMoveRight, 0) == 2)
        {
            speedHorizontal += playerComponent.m_speed * deltaTime * 8.0;
        }

        playerComponent.m_velocity = playerComponent.m_velocity + (externalAcceleration + Vec2(speedHorizontal, 0.0)) * deltaTime;

        PhysicsComponent@ physicsComponent = GetPhysicsComponent(GetOwningEntity());
        if (physicsComponent !is null)
        {
            physicsComponent.m_velocity = playerComponent.m_velocity * 10.0;
        }

        playerPos = playerPos + playerComponent.m_velocity;

        playerTransform.SetPosition(playerPos);

        DrawLine(playerPos, playerPos + modifiedWingTipPos, ColorCyan);
    }

    void TickInactive(float deltaTime)
    {

    }

    void OnActivated()
    {
    }

    void OnDeactivated()
    {
    }

    float m_wingBeatPos = 0.0; 
    bool m_bWingInMovement = false;
    float m_beatTimer = 0.0;
    float m_beatDuration = 1.0;
    Vec2 m_wingTipPos = Vec2(30.0, -50.0);
}