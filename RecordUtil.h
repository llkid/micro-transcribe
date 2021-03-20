#pragma once

#include "transcribebin/Transcriber.h"

#include <Windows.h>
#include <mmeapi.h>
#include <mmsystem.h>
#include <memory>
#include <string>
#include <vector>

#pragma comment(lib, "Winmm.lib")


namespace kaldi {

	struct UserData
	{
		char* buf = NULL;
		WAVEHDR whdr;
		DWORD len_;

		UserData(DWORD user) {
			whdr.dwUser = user;
			whdr.dwFlags = 0;
		}

		void SetLen(DWORD len) {
			if (!buf) {
				buf = new char[len];
				whdr.lpData = buf;
				whdr.dwBufferLength = len;
				len_ = len;
			}
		}

		void FreeBuf() {
			if (buf) {
				delete[] buf;
				buf = NULL;
			}
		}
	};

	class RecordUtil
	{
	public:
		using TranscriberPtr = std::unique_ptr<Transcriber>;
		TranscriberPtr transcriber;

	public:
		explicit RecordUtil();
		~RecordUtil();
		int32 parseInit(int argc, char* argv[]);
		void waveWork();
		void waveStop();
		void waveClose();

	private:
		WAVEINCAPS waveIncaps;
		HWAVEIN hWaveIn;
		WAVEFORMATEX pwfx;
		UserData data_fro{ 1 };
		UserData data_dst{ 2 };
	};

}
