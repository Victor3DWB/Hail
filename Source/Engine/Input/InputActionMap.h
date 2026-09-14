#pragma once
#include "Types.h"
#include "Containers\StaticArray\StaticArray.h"
#include "Containers\GrowingArray\GrowingArray.h"
#include "String.hpp"
#include "InputMappings.h"
#include "InputActionList.h"
namespace Hail
{
	class InputHandler;

	struct InputKey
	{
		uint16 m_keyValue{};
	};
	// Rename this class and file
	class InputActionMap
	{
	public:

		//Imports an input map if it exists, otherwise creates a default one that gets exported
		void Init(InputHandler* pInputHandler);

		enum class eInputType : uint8
		{
			Button=0x1,
			GamepadButton = 0x2,
			GamepadTrigger = 0x4, //Used for joysticks, mouse wheel or directions
			Direction=0x10, //Used for joysticks, mouse wheel or directions
		};
		enum class eDirectionInputType : uint8
		{
			Mouse,
			MouseWheel,
			JoystickLeft,
			JoystickRight,
			Count
		};

		struct ActionCommon
		{
			eInputAction action{};
			uint8 type{};
		};

		struct ButtonAction
		{
			ActionCommon actionData;
			eInputState currentState;
			GrowingArray<InputKey> registeredKeys;
		};
		struct DirectionAction
		{
			DirectionAction();
			ActionCommon actionData;
			bool bRegisteredInputTypes[(uint32)eDirectionInputType::Count];
			StaticArray<glm::vec2, 4> currentDirections;	
		};
		struct GamepadButtonAction
		{
			ActionCommon actionData;
			StaticArray<eInputState, 4> currentStates;
			StaticArray<bool, (uint8)eGamepadInputMapping::Count> bRegisteredButtons;
		};
		struct GamepadTriggerAction
		{
			ActionCommon actionData;
			StaticArray<float, 4> currentStates;
			StaticArray<bool, 2> bRegisteredTriggers; // L trigger 0 | R trigger 1
		};

		//Send in the controller ID if checking for controllers, an ID of 0 is for keyboard or player 1
		glm::vec2 GetDirectionInput(eInputAction actionToGet, int gamepadToCheck = 0) const;
		eInputState GetButtonInput(eInputAction actionToGet, int gamepadToCheck = 0) const;
		float GetGamepadTriggerInput(eInputAction actionToGet, int gamepadToCheck) const;

		void AddButtonInput(eInputAction actionToAdd, InputKey characterToBind);
		void AddGamepadButtonInput(eInputAction actionToAdd, eGamepadInputMapping buttonToBind);
		//Trigger 0 is Left Trigger 1 is Right
		void AddGamepadTriggerInput(eInputAction actionToAdd, uint8 triggerToBind);
		void AddDirectionInput(eInputAction actionToAdd, eDirectionInputType inputToBind);

		//Not a thread safe operation
		void UpdateInputActions();

		void ResetInputMappingToDefault();

		InputMap GetRawInputMap() const;

	private:
		void CreateDefaultInputMapping();
		void SaveActionToInputMap() const;

		InputHandler* m_pInputHandler;
		GrowingArray<DirectionAction> m_directionActions;
		GrowingArray<ButtonAction> m_buttonActions;
		GrowingArray<GamepadButtonAction> m_gamepadButtonActions;
		GrowingArray<GamepadTriggerAction> m_gamepadTriggerActions;

	};


}