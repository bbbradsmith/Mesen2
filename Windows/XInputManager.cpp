#include "Common.h"
#include "XInputManager.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/EmuSettings.h"

XInputManager::XInputManager(Emulator* emu)
{
	_emu = emu;
	for(int i = 0; i < XUSER_MAX_COUNT; i++) {
		_gamePadStates.push_back(unique_ptr<XINPUT_STATE>(new XINPUT_STATE()));
		_gamePadConnected.push_back(true);
	}
}

void XInputManager::RefreshState()
{
	XINPUT_STATE state;
	for(DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
		if(_gamePadConnected[i]) {
			if(XInputGetState(i, &state) != ERROR_SUCCESS) {
				//XInputGetState is incredibly slow when no controller is plugged in
				ZeroMemory(_gamePadStates[i].get(), sizeof(XINPUT_STATE));
				_gamePadConnected[i] = false;
			} else {
				*_gamePadStates[i] = state;
			}
		}
	}
}

bool XInputManager::NeedToUpdate()
{
	for(int i = 0; i < XUSER_MAX_COUNT; i++) {
		if(!_gamePadConnected[i]) {
			XINPUT_STATE state;
			if(XInputGetState(i, &state) == ERROR_SUCCESS) {
				return true;
			}
		}
	}
	return false;
}

void XInputManager::UpdateDeviceList()
{
	//Periodically detect if a controller has been plugged in to allow controllers to be plugged in after the emu is started
	for(int i = 0; i < XUSER_MAX_COUNT; i++) {
		_gamePadConnected[i] = true;
	}
}

bool XInputManager::IsPressed(uint8_t gamepadPort, uint8_t button)
{
	if(_gamePadConnected[gamepadPort]) {
		XINPUT_GAMEPAD &gamepad = _gamePadStates[gamepadPort]->Gamepad;
		if(button <= 16) {
			WORD xinputButton = 1 << (button - 1);
			return (_gamePadStates[gamepadPort]->Gamepad.wButtons & xinputButton) != 0;
		} else {
			double ratio = _emu->GetSettings()->GetControllerDeadzoneRatio() * 2;

			switch(button) {
				case 17: return gamepad.bLeftTrigger > (XINPUT_GAMEPAD_TRIGGER_THRESHOLD * ratio);
				case 18: return gamepad.bRightTrigger > (XINPUT_GAMEPAD_TRIGGER_THRESHOLD * ratio);
				case 19: return gamepad.sThumbRY > (XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE * ratio);
				case 20: return gamepad.sThumbRY < -(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE * ratio);
				case 21: return gamepad.sThumbRX < -(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE * ratio);
				case 22: return gamepad.sThumbRX > (XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE * ratio);
				case 23: return gamepad.sThumbLY > (XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * ratio);
				case 24: return gamepad.sThumbLY < -(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * ratio);
				case 25: return gamepad.sThumbLX < -(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * ratio);
				case 26: return gamepad.sThumbLX > (XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * ratio);
			}
		}
	}
	return false;
}

optional<int16_t> XInputManager::GetAxisPosition(uint8_t port, int axis)
{
	if(_gamePadConnected[port]) {
		XINPUT_GAMEPAD& gamepad = _gamePadStates[port]->Gamepad;
		switch(axis - 27) {
			case 0: return gamepad.sThumbLY;
			case 1: return gamepad.sThumbLX;
			case 2: return gamepad.sThumbRY;
			case 3: return gamepad.sThumbRX;
		}
	}

	return std::nullopt;
}