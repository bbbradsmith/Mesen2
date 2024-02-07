#pragma once
#include "pch.h"

enum class CgbCompat : uint8_t
{
	Gameboy = 0x00,
	GameboyColorSupport = 0x80,
	GameboyColorExclusive = 0xC0,
};

struct GameboyHeader
{
	//Starts at 0x134
	char Title[11];
	char ManufacturerCode[4];
	CgbCompat CgbFlag;
	char LicenseeCode[2];
	uint8_t SgbFlag;
	uint8_t CartType;
	uint8_t PrgRomSize;
	uint8_t CartRamSize;
	uint8_t DestCode;
	uint8_t OldLicenseeCode;
	uint8_t MaskRomVersion;
	uint8_t HeaderChecksum;
	uint8_t GlobalChecksum[2];

	uint32_t GetCartRamSize()
	{
		uint32_t tCartRamSize = 0;

		if(CartType == 5 || CartType == 6) {
			//MBC2 has 512x4bits of cart ram
			tCartRamSize = 0x200;
		} else {
			switch(CartRamSize) {
				case 0: tCartRamSize = 0; break;
				case 1: tCartRamSize = 0x800; break;
				case 2: tCartRamSize = 0x2000; break;
				case 3: tCartRamSize = 0x8000; break;
				case 4: tCartRamSize = 0x20000; break;
				case 5: tCartRamSize = 0x10000; break;
			}
		}

		if(CartType == 0xFA || CartType == 0xFB) {
			//RNBW has 8KB of FPGA-RAM
			tCartRamSize += 0x2000;
		}

		return tCartRamSize;
	}

	bool HasBattery()
	{
		switch(CartType) {
			case 0x03: case 0x06: case 0x09: case 0x0D:
			case 0x0F: case 0x10: case 0x13: case 0x1B:
			case 0x1E: case 0x22: case 0xFF:
				return true;
		}

		return false;
	}

	string GetCartName()
	{
		int nameLength = 11;
		for(int i = 0; i < 11; i++) {
			if(Title[i] == 0) {
				nameLength = i;
				break;
			}
		}
		string name = string(Title, nameLength);

		size_t lastNonSpace = name.find_last_not_of(' ');
		if(lastNonSpace != string::npos) {
			return name.substr(0, lastNonSpace + 1);
		} else {
			return name;
		}
	}
};
