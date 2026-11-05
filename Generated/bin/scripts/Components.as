shared class PlayerComponent : Component
{
    float m_gravity = 9.870;
    float m_speed = 5.0;
    Vec2 m_velocity = Vec2(0.0, 0.0);
    Vec2 m_currentDirection = Vec2(0.0, 1.0);
    float m_currentRotationRadian = 1.0;
    float m_forwardForce = -10.0;
    float m_upwardForce = 0.0;
    float m_drag = 1.0; // Scale from 0 - 1, full drag means that the velocity will be slowed down by friction to eventually become 0
    bool m_isFlipping = false;
    float m_flipTimer = 0.0f;
    float m_flipDuration = 1.0f;
    float m_flipStartRotation = 0.0f;
    float m_flipDirection = 1.0f;
}