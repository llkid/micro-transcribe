#include "RecordUtil.h"
#include <iostream>

namespace kaldi {

	void waveFormatInit(
		LPWAVEFORMATEX waveFormat,
		WORD nChannels,
		DWORD nSamplesPerSec,
		WORD wBitsPerSample
	);

	void CALLBACK waveInProc(
		HWAVEIN   hwi,
		UINT      uMsg,
		DWORD_PTR dwInstance,
		DWORD_PTR dwParam1,
		DWORD_PTR dwParam2
	);

	RecordUtil::RecordUtil()
		: waveIncaps({ 0 })
		, hWaveIn(NULL)
		, pwfx({ 0 })
	{
		const char *usage =
			"Reads in audio from a network socket and performs online\n"
			"decoding with neural nets (nnet3 setup), with iVector-based\n"
			"speaker adaptation and endpointing.\n"
			"Note: some configuration values and inputs are set via config\n"
			"files whose filenames are passed as options\n"
			"\n"
			"Usage: online2-tcp-nnet3-decode-faster [options] <nnet3-in> "
			"<fst-in> <word-symbol-table>\n";

		transcriber = TranscriberPtr(new kaldi::Transcriber(usage));
	}

	RecordUtil::~RecordUtil()
	{
		data_fro.FreeBuf();
		data_dst.FreeBuf();
	}

	int32 RecordUtil::parseInit(int argc, char* argv[])
	{
		if (transcriber->ParseInit(argc, argv))
			if (transcriber->LoadAM()) {
				size_t to_read = transcriber->chunk_len * sizeof(int16);
				data_fro.SetLen(to_read);
				data_dst.SetLen(to_read);
			}
			else
				return 0;
		else
			return 0;

		return 1;
	}

	void RecordUtil::waveWork()
	{
		MMRESULT mmres = waveInGetDevCaps(0, &waveIncaps, sizeof(WAVEINCAPS));
		if (MMSYSERR_NOERROR == mmres) {
			std::wcout << "\n��Ƶ�����豸: " << waveIncaps.szPname << '\n';

			waveFormatInit(&pwfx, 1, 16000, 16);
			printf("\n�������Ƶ�����豸");
			printf("\n����������1ͨ�� 16kHz 16bit\n");

			if (MMSYSERR_NOERROR == waveInOpen(
				&hWaveIn,
				WAVE_MAPPER,
				&pwfx,
				(DWORD_PTR)waveInProc,
				(DWORD_PTR)this,
				CALLBACK_FUNCTION
			)) {
				mmres = waveInPrepareHeader(hWaveIn, &data_fro.whdr, sizeof(WAVEHDR));
				if (MMSYSERR_NOERROR == mmres)
					printf("\n׼��������1");
				mmres = waveInPrepareHeader(hWaveIn, &data_dst.whdr, sizeof(WAVEHDR));
				if (MMSYSERR_NOERROR == mmres) {
					printf("\n׼��������2");

					mmres = waveInAddBuffer(hWaveIn, &data_fro.whdr, sizeof(WAVEHDR));
					if (MMSYSERR_NOERROR == mmres)
						printf("\n��������1������Ƶ�����豸");
					mmres = waveInAddBuffer(hWaveIn, &data_dst.whdr, sizeof(WAVEHDR));

					if (MMSYSERR_NOERROR == mmres) {
						printf("\n��������2������Ƶ�����豸\n");

						mmres = waveInStart(hWaveIn);
						if (MMSYSERR_NOERROR == mmres)
							printf("��ʼ¼��!!!\n");
					}
				}
			}

		}
		else {
			std::wcout << L"\nû�з��ֿ��õ������豸:" << mmres << '\n';
		}
	}

	void RecordUtil::waveStop()
	{
		waveInUnprepareHeader(hWaveIn, &data_fro.whdr, sizeof(WAVEHDR));
		waveInUnprepareHeader(hWaveIn, &data_dst.whdr, sizeof(WAVEHDR));
		waveInStop(hWaveIn);
	}

	void RecordUtil::waveClose()
	{
		waveInClose(hWaveIn);
	}

	static Vector<BaseFloat> ChsToVectorF(char * data, int32_t len)
	{
		Vector<BaseFloat> fvec;

		fvec.Resize(static_cast<MatrixIndexT>(len));
		int16* buf = reinterpret_cast<int16*>(data);

		for (size_t i = 0; i < len; ++i)
			fvec(i) = static_cast<BaseFloat>(buf[i]);

		return fvec;
	}


	static void waveFormatInit(
		LPWAVEFORMATEX waveFormat,
		WORD nChannels,
		DWORD nSamplesPerSec,
		WORD wBitsPerSample
	) {																				// 256kbps = 16000 * 16
		waveFormat->wFormatTag = WAVE_FORMAT_PCM;									// WAVE_FORMAT_PCM = 1 
		waveFormat->nChannels = nChannels;											// 1
		waveFormat->nSamplesPerSec = nSamplesPerSec;								// 16000
		waveFormat->nBlockAlign = nChannels * wBitsPerSample / 8;					// 2
		waveFormat->nAvgBytesPerSec = nSamplesPerSec * waveFormat->nBlockAlign;		// 32000
		waveFormat->wBitsPerSample = wBitsPerSample;								// 16
		waveFormat->cbSize = 0; // For WAVE_FORMAT_PCM format, this memeber is ignored.
	}

	static void CALLBACK waveInProc(
		HWAVEIN   hwi,
		UINT      uMsg,
		DWORD_PTR dwInstance,
		DWORD_PTR dwParam1,
		DWORD_PTR dwParam2
	) {
		switch (uMsg)
		{
		case WIM_CLOSE:
			printf("\n�豸�Ѿ��ر�...\n");
			break;
		case WIM_DATA: {
			// ��ȡ¼������
			auto user = reinterpret_cast<RecordUtil*>(dwInstance);
			// ��ȡ¼����Ϣ�ṹ��
			LPWAVEHDR pWhdr = reinterpret_cast<LPWAVEHDR>(dwParam1);
			// �Ѿ�¼�Ƶ���Ϣ����
			DWORD len = pWhdr->dwBytesRecorded;
			LPSTR data = pWhdr->lpData;
			/*printf("\n������%d����...\n", pWhdr->dwUser);
			printf("�ɼ������ݳ���Ϊ:%ld %s\n", len, data);*/

			/// ��������
			auto&& wave_part = ChsToVectorF(data, static_cast<int32_t>(len / 2));
			len
				? user->transcriber->ParseStart(wave_part)
				: user->transcriber->ParseStop();

			waveInPrepareHeader(hwi, (LPWAVEHDR)dwParam1, sizeof(WAVEHDR));
			waveInAddBuffer(hwi, (LPWAVEHDR)dwParam1, sizeof(WAVEHDR));

			break;
		}
		case WIM_OPEN:
			printf("\n�豸�Ѿ���...\n");
			break;
		default:
			break;
		}
	}

}
