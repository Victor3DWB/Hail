
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
        DrawCircle(m_pos, m_radius);
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

enum EBestEnumEver
{
    eValue0,
    eValue2 = 2,
    eValue3,
    eValue200 = eValue2 * 100
}

class PlayerMovementCapability : Capability
{
    void Setup()
    {
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
        float movementSpeed = 5.0;
        Vec2 direction = GetDirectionInput(PlayerMoveJoystickL, 0);
        direction.y = direction.y * -1.0;
        direction = direction.GetNormalized();
        m_pos += direction * movementSpeed;

        if (GetButtonInput(eInputAction::PlayerAction1, 0) == 1)
        {
            m_bActiveCapability = false;
            if (m_bActive)
            {
                Print("Still Active <3");
                string printString = "Owning Entity ID: ";
                printString += GetOwningEntity();
                Print(printString);
            }
        }

        if (GetButtonInput(eInputAction::PlayerMoveLeft, 0) == 2)
        {
            m_pos.x = m_pos.x - 0.5 * movementSpeed;
        }
        if (GetButtonInput(eInputAction::PlayerMoveRight, 0) == 2)
        {
            m_pos.x = m_pos.x + 0.5 * movementSpeed;
        }

        if (GetButtonInput(eInputAction::PlayerMoveUp, 0) == 2)
        {
            m_pos.y = m_pos.y - 0.5 * movementSpeed;
        }
        if (GetButtonInput(eInputAction::PlayerMoveDown, 0) == 2)
        {
            m_pos.y = m_pos.y + 0.5 * movementSpeed;
        }

        Transform2D@ playerTransform = GetEntityTransform(GetOwningEntity());
        playerTransform.SetPosition(m_pos);
        
        DrawCircle(m_pos, m_radius);
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
    Vec2 m_pos =  Vec2(0.0);
    bool m_bActiveCapability = true;
}
