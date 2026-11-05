
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

class PlayerWingBeatCapability : Capability
{

    void Setup()
    {
        Print("Setup Wing Capability");    
        m_bActive = true;
    }

    void TickActive(float deltaTime)
    {
        PlayerComponent@ playerComponent = GetOrCreatePlayerComponent(GetOwningEntity());
        
        Transform2D@ playerTransform = GetEntityTransform(GetOwningEntity());
        Vec2 playerPos = playerTransform.GetPosition();

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

        DrawLine(playerPos, playerPos + modifiedWingTipPos, ColorCyan);
        //DrawBox(playerPos, Vec2(100.0), ColorCyan);
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
    float m_beatTimer = 0.0f;
    float m_beatDuration = 1.0f;
    Vec2 m_wingTipPos = Vec2(30.0, -50.0);
}