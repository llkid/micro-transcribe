/// ����תд

#include "transcribebin/RecordUtil.h"
#include <conio.h>


int main(int argc, char* argv[]) {

	UINT inDevs = waveInGetNumDevs();
	std::cout << "waveInGetNumDevs: " << inDevs << '\n';

	if (!inDevs) {
		std::cout << "\n��ǰû�з��ֿ�����Ƶ�����豸���밴ESC���˳�\n";
		return 1;
	}

	kaldi::RecordUtil recordUtil;
	if (recordUtil.parseInit(argc, argv)) {
		recordUtil.waveWork();
	}

	while (true)
	{
		int key = _getch();
		if (key == 27) {
			recordUtil.waveClose();
			recordUtil.waveStop();
		}
	}

	return 0;
}