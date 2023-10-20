#pragma once
#include "NES/APU/BaseExpansionAudio.h"
#include "NES/Mappers/Audio/RainbowPulse.h"
#include "NES/Mappers/Audio/RainbowSaw.h"
#include "NES/APU/NesApu.h"
#include "NES/NesConsole.h"
#include "Utilities/Serializer.h"

class RNBWAudio : public BaseExpansionAudio
{
private:
	RNBWPulse _pulse1;
	RNBWPulse _pulse2;
	RNBWSaw _saw;
	bool _haltAudio = false;
	int32_t _lastOutput = 0;

protected:
	void Serialize(Serializer& s) override
	{
		SV(_pulse1);
		SV(_pulse2);
		SV(_saw);
		SV(_lastOutput);
		SV(_haltAudio);
	}

	void ClockAudio() override
	{
		if(!_haltAudio) {
			_pulse1.Clock();
			_pulse2.Clock();
			_saw.Clock();
		}

		int32_t outputLevel = _pulse1.GetVolume() + _pulse2.GetVolume() + _saw.GetVolume();
		_console->GetApu()->AddExpansionAudioDelta(AudioChannel::VRC6, outputLevel - _lastOutput);
		_lastOutput = outputLevel;
	}

public:
	RNBWAudio(NesConsole* console) : BaseExpansionAudio(console)
	{
		Reset();
	}

	void Reset()
	{
		_lastOutput = 0;
		_haltAudio = false;
	}

	void WriteRegister(uint16_t addr, uint8_t value)
	{
		switch(addr) {
			case 0x41A0: case 0x41A1: case 0x41A2:
				//_pulse1.WriteReg(addr, value);
				_pulse1.WriteReg(addr & 0x03, value);
				break;

			case 0x41A3: case 0x41A4: case 0x41A5:
				//_pulse2.WriteReg(addr, value);
				_pulse2.WriteReg((addr - 3) & 0x03, value);
				break;

			case 0x41A6: case 0x41A7: case 0x41A8:
				//_saw.WriteReg(addr, value);
				_saw.WriteReg((addr - 6) & 0x03, value);
				break;
		}
	}

	// TODO
	// void GetMapperStateEntries(vector<MapperStateEntry>& entries)
	// {
	// }

};