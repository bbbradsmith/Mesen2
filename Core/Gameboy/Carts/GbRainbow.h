#pragma once
#include "pch.h"
#include "Gameboy/Carts/GbCart.h"
#include "Gameboy/GbMemoryManager.h"
#include "NES/Mappers/Homebrew/RainbowESP.h"
#include "Utilities/Serializer.h"

// mapper 0xFA - Rainbow board by Broke Studio
//
// documentation available here: https://github.com/BrokeStudio/rainbow-net

#define MAPPER_PLATFORM_PCB 0
#define MAPPER_PLATFORM_EMU 1
#define MAPPER_PLATFORM_WEB 2
#define MAPPER_VERSION_PROTOTYPE 0
#define MAPPER_VERSION		(MAPPER_PLATFORM_EMU << 5) | MAPPER_VERSION_PROTOTYPE

#define ROM_MODE_0		0b000	// 32K
#define ROM_MODE_1		0b001	// 16K + 16K
#define ROM_MODE_2		0b010	// 16K + 8K + 8K
#define ROM_MODE_3		0b011	// 8K + 8K + 8K + 8K
#define ROM_MODE_4		0b100	// 4K + 4K + 4K + 4K + 4K + 4K + 4K + 4K

#define RAM_MODE_0		0b0	// 8K
#define RAM_MODE_1		0b1	// 4K

class GbRainbow : public GbCart
{
private:
	static constexpr int FpgaRamSize = 0x2000;
	static constexpr int FpgaFlashSize = 0x3000;

	uint8_t* _fpgaRam = nullptr;
	//uint8_t* _fpgaFlash = nullptr;

	bool _bootRomMode;
	bool _mbc5CompatibilityMode;
	bool _ramEnabled;

	uint8_t _romMode;
	uint8_t _ramMode;

	uint32_t _romBanks[8];
	uint32_t _ramBanks[2];

	// FPGA RAM auto R/W
	uint16_t _fpgaRamAutoRwAddress;
	uint8_t _fpgaRamAutoRwIncrement;

	// ESP/WIFI
	BrokeStudioFirmware* _esp = NULL;
	bool _espEnable;
	bool _espIrqEnable;
	bool _espHasReceivedMessage;
	bool _espMessageSent;
	uint8_t _espRxAddress, _espTxAddress;

public:
	void InitCart() override
	{
		// Power/Reset values
		_bootRomMode = false;
		_mbc5CompatibilityMode = true;
		_ramEnabled = false;
		_romBanks[0] = 0x00000;
		_romBanks[4] = 0x00001;
		_ramBanks[0] = 0x10000;
		_ramBanks[1] = 0x10001;
		_romMode = 0;
		_ramMode = 0;

		// ESP / WiFi
		_esp = new BrokeStudioFirmware;
		_espEnable = false;
		EspClearMessageReceived();
		_espMessageSent = true;
		_espRxAddress = 0;
		_espTxAddress = 0;

		_memoryManager->MapRegisters(0x0000, 0x5FFF, RegisterAccess::Write);
		_fpgaRam = _cartRam + 0x8000;
	}

	void RefreshMappings() override
	{
		uint32_t ramRealSize = _gameboy->GetHeader().GetCartRamSize() - FpgaRamSize;
		if(_memoryManager->IsBootRomDisabled()) _memoryManager->MapRegisters(0x0000, 0x00FF, RegisterAccess::ReadWrite);

		bool mbc5CompatibilityMode = _bootRomMode ? false : _mbc5CompatibilityMode;
		uint8_t romMode = _bootRomMode ? ROM_MODE_3 : _mbc5CompatibilityMode ? ROM_MODE_1 : _romMode;
		uint16_t bankSize;
		uint16_t mask;

		if(mbc5CompatibilityMode) {

			constexpr int prgBankSize = 0x4000;
			constexpr int ramBankSize = 0x2000;

			Map(0x0000, 0x3FFF, GbMemoryType::PrgRom, 0, true);
			Map(0x4000, 0x7FFF, GbMemoryType::PrgRom, (_romBanks[4] & 0xFFFF) * prgBankSize, true);

			if(_ramEnabled) {
				Map(0xA000, 0xBFFF, GbMemoryType::CartRam, (_ramBanks[0] & 0xFFFF) * ramBankSize, false);
				_memoryManager->MapRegisters(0xA000, 0xBFFF, RegisterAccess::None);
			} else {
				Unmap(0xA000, 0xBFFF);
				_memoryManager->MapRegisters(0xA000, 0xBFFF, RegisterAccess::Read);
			}

		} else {

			_memoryManager->MapRegisters(0xA000, 0xBFFF, RegisterAccess::None);

			// ROM banking
			switch(romMode) {

				case ROM_MODE_0:

					// 32K
					bankSize = 0x8000;
					switch((_romBanks[0] >> 16) & 0x01) {
						case 0x00:						// ROM
							Map(0x0000, 0x7FFF, GbMemoryType::PrgRom, (_romBanks[0] & 0xFFFF) * bankSize, true);
							break;
						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0x0000, 0x7FFF, GbMemoryType::CartRam, (_romBanks[0] & mask) * bankSize, true);
							break;
					}
					break;

				case ROM_MODE_1:

					// 16K
					bankSize = 0x4000;
					switch((_romBanks[0] >> 16) & 0x01) {
						case 0x00:						// ROM
							Map(0x0000, 0x3FFF, GbMemoryType::PrgRom, (_romBanks[0] & 0xFFFF) * bankSize, true);
							break;
						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0x0000, 0x3FFF, GbMemoryType::CartRam, (_romBanks[0] & mask) * bankSize, true);
							break;
					}

					// 16K
					bankSize = 0x4000;
					switch((_romBanks[4] >> 16) & 0x01) {
						case 0x00:						// ROM
							Map(0x4000, 0x7FFF, GbMemoryType::PrgRom, (_romBanks[4] & 0xFFFF) * bankSize, true);
							break;
						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0x4000, 0x7FFF, GbMemoryType::CartRam, (_romBanks[4] & mask) * bankSize, true);
							break;
					}
					break;

				case ROM_MODE_2:

					// 16K
					bankSize = 0x4000;
					switch((_romBanks[0] >> 16) & 0x01) {
						case 0x00:						// ROM
							Map(0x0000, 0x3FFF, GbMemoryType::PrgRom, (_romBanks[0] & 0xFFFF) * bankSize, true);
							break;
						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0x0000, 0x3FFF, GbMemoryType::CartRam, (_romBanks[0] & mask) * bankSize, true);
							break;
					}

					// 8K
					bankSize = 0x2000;
					switch((_romBanks[4] >> 16) & 0x01) {
						case 0x00:						// ROM
							Map(0x4000, 0x5FFF, GbMemoryType::PrgRom, (_romBanks[4] & 0xFFFF) * bankSize, true);
							break;
						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0x4000, 0x5FFF, GbMemoryType::CartRam, (_romBanks[4] & mask) * bankSize, true);
							break;
					}

					// 8K
					bankSize = 0x2000;
					switch((_romBanks[6] >> 16) & 0x01) {
						case 0x00:						// ROM
							Map(0x6000, 0x7FFF, GbMemoryType::PrgRom, (_romBanks[6] & 0xFFFF) * bankSize, true);
							break;
						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0x6000, 0x7FFF, GbMemoryType::CartRam, (_romBanks[6] & mask) * bankSize, true);
							break;
					}
					break;

				case ROM_MODE_3:

					// 8K x 4
					bankSize = 0x2000;
					for(uint8_t i = 0; i < 4; i++) {

						switch((_romBanks[i * 2] >> 16) & 0x01) {
							case 0x00:						// ROM
								Map(i * bankSize, i * bankSize + 0x1FFF, GbMemoryType::PrgRom, (_romBanks[i * 2] & 0xFFFF) * bankSize, true);
								break;
							case 0x01:						// RAM
								mask = (ramRealSize / bankSize) - 1;
								Map(i * bankSize, i * bankSize + 0x1FFF, GbMemoryType::CartRam, (_romBanks[i * 2] & mask) * bankSize, true);
								break;
						}

					}
					break;

				case ROM_MODE_4:

					// 4K x 8
					bankSize = 0x1000;
					for(uint8_t i = 0; i < 8; i++) {

						switch((_romBanks[i] >> 16) & 0x01) {
							case 0x00:						// ROM
								Map(i * bankSize, i * bankSize + 0x0FFF, GbMemoryType::PrgRom, (_romBanks[i] & 0xFFFF) * bankSize, true);
								break;
							case 0x01:						// RAM
								mask = (ramRealSize / bankSize) - 1;
								Map(i * bankSize, i * bankSize + 0x0FFF, GbMemoryType::CartRam, (_romBanks[i] & mask) * bankSize, true);
								break;
						}

					}
					break;

			}

			// RAM banking
			_memoryManager->MapRegisters(0xA000, 0xBFFF, RegisterAccess::None);
			switch(_ramMode) {

				case RAM_MODE_0:

					// $A000-$BFFF
					bankSize = 0x2000;
					switch((_ramBanks[0] >> 16) & 0x03) {
						case 0x00:						// ROM
							Map(0xA000, 0xBFFF, GbMemoryType::PrgRom, (_ramBanks[0] & 0xFFFF) * bankSize, false);
							break;
						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0xA000, 0xBFFF, GbMemoryType::CartRam, (_ramBanks[0] & mask) * bankSize, false);
							break;
						case 0x02: case 0x03:		// FPGA-RAM
							Map(0xA000, 0xBFFF, GbMemoryType::CartRam, ramRealSize, false);
							break;
					}
					break;

				case RAM_MODE_1:

					// $A000-$AFFF
					bankSize = 0x1000;
					switch((_ramBanks[0] >> 16) & 0x03) {

						case 0x00:						// ROM
							Map(0xA000, 0xAFFF, GbMemoryType::PrgRom, (_ramBanks[0] & 0xFFFF) * bankSize, false);
							break;

						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0xA000, 0xAFFF, GbMemoryType::CartRam, (_ramBanks[0] & mask) * bankSize, false);
							break;

						case 0x02: case 0x03:		// FPGA-RAM
							Map(0xA000, 0xAFFF, GbMemoryType::CartRam, (_ramBanks[0] & 0x0001) * bankSize + ramRealSize, false);
							break;

					}

					// $B000-$BFFF
					bankSize = 0x1000;
					switch((_ramBanks[1] >> 16) & 0x03) {

						case 0x00:						// ROM
							Map(0xB000, 0xBFFF, GbMemoryType::PrgRom, (_ramBanks[1] & 0xFFFF) * bankSize, false);
							break;

						case 0x01:						// RAM
							mask = (ramRealSize / bankSize) - 1;
							Map(0xB000, 0xBFFF, GbMemoryType::CartRam, (_ramBanks[1] & mask) * bankSize, false);
							break;

						case 0x02: case 0x03:		// FPGA-RAM
							Map(0xB000, 0xBFFF, GbMemoryType::CartRam, (_ramBanks[1] & 0x0001) * bankSize + ramRealSize, false);
							break;

					}
					break;

			}
		}
	}

	uint8_t ReadRegister(uint16_t addr) override
	{
		switch(addr) {

			// FPGA RAM auto R/W
			case 0x00B3:
			{
				uint8_t retval = _fpgaRam[_fpgaRamAutoRwAddress];
				_fpgaRamAutoRwAddress += _fpgaRamAutoRwIncrement;
				return retval;
			}

			// MISC
			case 0x00CA: return MAPPER_VERSION;

				// ESP - WiFi
			case 0x00F0:
			{
				uint8_t espEnableFlag = _espEnable ? 0x01 : 0x00;
				uint8_t espIrqEnableFlag = _espIrqEnable ? 0x02 : 0x00;
				//UDBG("RAINBOW read flags %04x => %02xs\n", A, espEnableFlag | espIrqEnableFlag);
				MessageManager::Log("[Rainbow] read flags " + HexUtilities::ToHex(addr) + " => " + HexUtilities::ToHex(espEnableFlag | espIrqEnableFlag));
				return espEnableFlag | espIrqEnableFlag;
			}
			case 0x00F1:
			{
				uint8_t espMessageReceivedFlag = EspMessageReceived() ? 0x80 : 0;
				uint8_t espRtsFlag = _esp->getDataReadyIO() ? 0x40 : 0x00;
				return espMessageReceivedFlag | espRtsFlag;
			}
			case 0x00F2: return _espMessageSent ? 0x80 : 0;
		}


		uint8_t* ptr = _memoryManager->GetMappedBlock(addr);

		if(ptr) {
			return *ptr;
		}
		return 0xFF;

		return _memoryManager->DebugRead(addr);

		//Disabled RAM returns 0xFF on reads
		return 0xFF;
	}

	void WriteRegister(uint16_t addr, uint8_t value) override
	{
		bool mbc5CompatibilityMode = _bootRomMode ? false : _mbc5CompatibilityMode;

		if(mbc5CompatibilityMode) {
			switch(addr & 0x7000) {
				case 0x0000:
				case 0x1000:
					_ramEnabled = (value == 0x0A);
					break;

				case 0x2000:
					if(mbc5CompatibilityMode) _romBanks[4] = (value & 0xFF) | (_romBanks[4] & 0x100);
					break;

				case 0x3000:
					if(mbc5CompatibilityMode) _romBanks[4] = (_romBanks[4] & 0xFF) | ((value & 0x01) << 8);
					break;

				case 0x4000:
				case 0x5000:
					if(mbc5CompatibilityMode) _ramBanks[0] = value & 0x0F;
					break;
			}
		}

		switch(addr) {

			// ROM / RAM banking
			case 0x0070:
			case 0x0071:
			case 0x0072:
			case 0x0073:
			case 0x0074:
			case 0x0075:
			case 0x0076:
			case 0x0077:
				if(!mbc5CompatibilityMode) _romBanks[addr & 0x07] = (_romBanks[addr & 0x07] & 0x00FFFF) | ((value & 0x01) << 16);
				break;
			case 0x007A:
			case 0x007B:
				if(!mbc5CompatibilityMode) _ramBanks[addr & 0x01] = (_ramBanks[addr & 0x01] & 0x00FFFF) | ((value & 0x03) << 16);
				break;

			case 0x0080:
			case 0x0081:
			case 0x0082:
			case 0x0083:
			case 0x0084:
			case 0x0085:
			case 0x0086:
			case 0x0087:
				if(!mbc5CompatibilityMode) _romBanks[addr & 0x07] = (_romBanks[addr & 0x07] & 0x0100FF) | (value << 8);
				break;
			case 0x008A:
			case 0x008B:
				if(!mbc5CompatibilityMode) _ramBanks[addr & 0x01] = (_ramBanks[addr & 0x01] & 0x0300FF) | (value << 8);
				break;

			case 0x0090:
			case 0x0091:
			case 0x0092:
			case 0x0093:
			case 0x0094:
			case 0x0095:
			case 0x0096:
			case 0x0097:
				if(!mbc5CompatibilityMode) _romBanks[addr & 0x07] = (_romBanks[addr & 0x07] & 0x01FF00) | value;
				break;
			case 0x009A:
			case 0x009B:
				if(!mbc5CompatibilityMode) _ramBanks[addr & 0x01] = (_ramBanks[addr & 0x01] & 0x03FF00) | value;
				break;

				// ROM / RAM control
			case 0x00A0:
				_ramMode = (value & 0x80);
				_mbc5CompatibilityMode = (value & 0x08);
				_romMode = value & 0x07;
				break;

				// FPGA RAM auto R/W
			case 0x00B0: _fpgaRamAutoRwAddress = (_fpgaRamAutoRwAddress & 0x00ff) | ((value & 0x1f) << 8); break;
			case 0x00B1: _fpgaRamAutoRwAddress = (_fpgaRamAutoRwAddress & 0xff00) | (value); break;
			case 0x00B2: _fpgaRamAutoRwIncrement = value; break;
			case 0x00B3:
				_fpgaRam[_fpgaRamAutoRwAddress] = value;
				_fpgaRamAutoRwAddress += _fpgaRamAutoRwIncrement;
				break;

			// ESP - WiFi
			case 0x00F0:
				_espEnable = value & 0x01;
				_espIrqEnable = value & 0x02;
				break;
			case 0x00F1:
				if(_espEnable) EspClearMessageReceived();
				//else FCEU_printf("RAINBOW warning: $00F0.0 is not set\n");
				else MessageManager::Log("[Rainbow] warning: $00F0.0 is not set.");
				break;
			case 0x00F2:
				if(_espEnable) {
					_espMessageSent = false;
					uint8_t message_length = _fpgaRam[0x1800 + (_espTxAddress << 8)];
					_esp->rx(message_length);
					for(uint8_t i = 0; i < message_length; i++) {
						_esp->rx(_fpgaRam[0x1800 + (_espTxAddress << 8) + 1 + i]);
					}
					_espMessageSent = true;
				}
				//else FCEU_printf("RAINBOW warning: $00F0.0 is not set\n");
				else MessageManager::Log("[Rainbow] warning: $00F0.0 is not set.");
				break;
			case 0x00F3:
				_espRxAddress = value & 0x07;
				break;
			case 0x00F4:
				_espTxAddress = value & 0x07;
				break;

			case 0x00FF:
				_bootRomMode = (value & 0x01);
				break;
		}
		RefreshMappings();
	}

	void EspCheckNewMessage()
	{
		// get new message if needed
		if(_espEnable && _esp->getDataReadyIO() && _espHasReceivedMessage == false) {
			uint8_t message_length = _esp->tx();
			_fpgaRam[0x1800 + (_espRxAddress << 8)] = message_length;
			for(uint8_t i = 0; i < message_length; i++) {
				_fpgaRam[0x1800 + (_espRxAddress << 8) + 1 + i] = _esp->tx();
			}
			_espHasReceivedMessage = true;
		}
	}

	bool EspMessageReceived()
	{
		EspCheckNewMessage();
		return _espHasReceivedMessage;
	}

	void EspClearMessageReceived()
	{
		_espHasReceivedMessage = false;
	}

	void Serialize(Serializer& s) override
	{
		SV(_bootRomMode);
		SV(_mbc5CompatibilityMode);
		SV(_ramEnabled);
		SV(_romMode);
		SV(_ramMode);
		SVArray(_romBanks, 10);
		SVArray(_ramBanks, 2);
		SV(_fpgaRamAutoRwAddress);
		SV(_fpgaRamAutoRwIncrement);
		SV(_espEnable);
		SV(_espIrqEnable);
		SV(_espHasReceivedMessage);
		SV(_espMessageSent);
		SV(_espRxAddress);
		SV(_espTxAddress);
	}
};