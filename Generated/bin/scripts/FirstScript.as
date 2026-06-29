#include "IncludeScript.as"

Foo TestClass;

float gameTime = 0.0;
Array<Vec2> savedPositions;
Vec2 playerPosition = Vec2(0.5, 0.5);
float movementSpeed = 5.0;
PlayerMovementCapability playerCapability;

void Init()
{
    playerCapability.Setup();
}

void main()
{
    gameTime += 0.4;
    Vec2 direction = GetDirectionInput(PlayerMoveJoystickL, 0);
    direction.y = direction.y * -1.0;
    direction = direction.GetNormalized();
    playerPosition += direction * movementSpeed;

    DrawCircle(playerPosition, 5.015);
    playerCapability.TickActive(0.1);
    if (GetButtonInput(eInputAction::PlayerAction1, 0) == 1)
    {
        savedPositions.Add(playerPosition);

        string inputOutputText = "Player position = x : ";
        inputOutputText += playerPosition.x;
        inputOutputText += " y : ";
        inputOutputText += playerPosition.y;
        Print(inputOutputText);
    }

    for (uint i = 0; i < savedPositions.Size(); i++)
    {
        DrawCircle(savedPositions[i], 10.005);
    }
    
    if (TestClass.IsInside(playerPosition))
    {
        TestClass.MoveAround();
    }
    TestClass.ShowPosition();
}