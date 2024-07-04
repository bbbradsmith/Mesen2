#pragma once
#include "pch.h"
#include "Shared/BaseControlDevice.h"
#include "Utilities/Serializer.h"

class KeyboardMouseHost : public BaseControlDevice
{
private:
	uint32_t d3, d4;
	uint8_t wheel_timeout;
	bool relative;
	uint8_t kbold[17];
	EmuSettings* _settings = nullptr;
	static const int FRAMES_PER_WHEEL = 15; // simulated mouse wheel roll speed

protected:
	string GetKeyNames() override
	{
		return "LRMudRP.ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890EeBTS-=[]\\#;'`,./C123456789012PSpIHUDEDRLDUN/*-+E1234567890.|CRKYHM,HhKHZPS<>UDMCSAWcsaw";
	}

	enum Buttons
	{
		MouseLeft, MouseRight, MouseMiddle, MouseWheelUp, MouseWheelDown,
		Rollover, PostFail, Undefined,
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,
		Enter, Esc, Backspace, Tab, Space, Minus, Equal, LeftBracket, RightBracket,
		Backslash, HashTilde, SemiColon, Apostrophe, Grave, Comma, Dot, Slash, CapsLock,
		F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		PrintScreen, ScrollLock, Pause,
		Insert, Home, PageUp, Delete, End, PageDown,
		Right, Left, Down, Up,
		NumLock, NumpadDivide, NumpadMultiply, NumpadMinus, NumpadPlus, NumpadEnter,
		Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9, Numpad0,
		NumpadDot,
		Key102, Compose,
		RO, Kana, Yen, Henkan, Muhenkan, KpJpComma, Hangeul, Hanja, Katakana, Hiragana, ZenkakuHankaku,
		PlayPause, Stop, Previous, Next, VolumeUp, VolumeDown, Mute,
		LeftCtrl, LeftShift, LeftAlt, LeftWin,
		RightCtrl, RightShift, RightAlt, RightWin,
	};

	bool HasCoordinates() override { return true; }

	void Serialize(Serializer& s) override
	{
		BaseControlDevice::Serialize(s);
		SV(d3); SV(d4); SV(wheel_timeout); SV(relative); SVArray(kbold,17);
	}

	void InternalSetStateFromInput() override
	{
		bool mouse_on = _settings->GetNesConfig().KeyboardMouseHostMouseOn;
		if (mouse_on) {
			relative = _settings->GetNesConfig().KeyboardMouseHostRelative;
			if (relative) {
				SetMovement(KeyManager::GetMouseMovement(_emu, _settings->GetInputConfig().MouseSensitivity));
			} else {
				SetCoordinates(KeyManager::GetMousePosition());
			}
		}

		for (KeyMapping& keyMapping : _keyMappings) {
			for (int i = 0; i <= Buttons::RightWin; i++) {
				SetPressedState(i, keyMapping.CustomKeys[i]);
			}
		}
	}

public:
	KeyboardMouseHost(Emulator* emu, KeyMappingSet keyMappings) : BaseControlDevice(emu, ControllerType::KeyboardMouseHost, BaseControlDevice::ExpDevicePort, keyMappings)
	{
		d3 = 0;
		d4 = 0;
		wheel_timeout = 0;
		relative = false;
		for (int i = 0; i < 17; i++) kbold[i] = 0;
		_settings = _emu->GetSettings();
	}

	uint8_t ReadRam(uint16_t addr) override
	{
		const uint16_t port = _settings->GetNesConfig().KeyboardMouseHostPort ? 0x4017 : 0x4016;
		uint8_t output = 0;
		if(addr == port) {
			output |= (d3 & 0x80000000) >> (31-3);
			output |= (d4 & 0x80000000) >> (31-4);
			d3 <<= 1;
			d4 <<= 1;
		}
		return output;
	}

	void WriteRam(uint16_t addr, uint8_t value) override
	{
		bool prevStrobe = _strobe;
		_strobe = (value & 0x01) == 0x01;
		// Latch state on rising strobe
		if(!prevStrobe && _strobe) {
			RefreshStateBuffer();
		}
	}

	void RefreshStateBuffer() override
	{
		bool mouse_on = _settings->GetNesConfig().KeyboardMouseHostMouseOn;
		bool key_on = _settings->GetNesConfig().KeyboardMouseHostKeyOn;

		uint8_t status = 0x06 | // device ID
			(relative ? 0x08 : 0x00) |
			(key_on   ? 0x10 : 0x00) |
			(mouse_on ? 0x20 : 0x00) |
			(IsPressed(Buttons::MouseRight) ? 0x40 : 0x00) |
			(IsPressed(Buttons::MouseLeft)  ? 0x80 : 0x00);

		uint8_t mx = 0;
		uint8_t my = 0;
		uint8_t ms = 0;
		if (mouse_on) {
			if (relative) {
				MouseMovement mov = GetMovement();
				int8_t rx = std::clamp(int(mov.dx),-128,127);
				int8_t ry = std::clamp(int(mov.dy),-128,127);
				mx = uint8_t(rx); // reinterpret signed difference as unsigned
				my = uint8_t(ry);
			} else {
				MousePosition pos = GetCoordinates();
				mx = std::clamp(int(pos.X),0,255);
				my = std::clamp(int(pos.Y),0,255);
			}
			ms = IsPressed(Buttons::MouseMiddle) ? 0x80 : 00;
			// scroll wheel simulated with a rate-limiting timeout on a button/key hold
			if (wheel_timeout > 0) {
				wheel_timeout--;
			} else {
				bool wu = IsPressed(Buttons::MouseWheelUp);
				bool wd = IsPressed(Buttons::MouseWheelDown);
				if (wu || wd) {
					uint8_t w = 0;
					if (wu) w--;
					if (wd) w++;
					wheel_timeout = FRAMES_PER_WHEEL;
					ms |= (w & 0x80) >> 1; // sign bit
					ms |= (w & 0x0F) << 3; // value
				}
			}
		}

		uint8_t khit[4] = { 0 };
		if (key_on) {
			// Transfer changed keys into make/break scancode buffer
			uint8_t kbnew[17] = { 0 };
			int hits = 0;
			for (int i = Buttons::Rollover; i <= Buttons::RightWin; i++) {
				int ibyte = i / 8;
				uint8_t ibit = 1 << (i % 8);
				if (IsPressed(i)) kbnew[ibyte] |= ibit; // make/break high bit
				if ((kbnew[ibyte] ^ kbold[ibyte]) & ibit) {
					if (hits < 4) {
						khit[hits] = (i+1-Buttons::Rollover) | ((kbnew[ibyte] & ibit) ? 0x00 : 0x80); // scancode
						hits++;
					} else {
						kbnew[ibyte] ^= ibit; // too many keys this frame, pass to next frame
					}
				}
			}
			for (int i = 0; i < 17; i++) kbold[i] = kbnew[i];
		}

		d3 = (khit[0] << 24) | (khit[1] << 16) | (khit[2] << 8) | (khit[3] << 0);
		d4 = (status << 24) | (mx << 16) | (my << 8) | (ms << 0);
	}
};