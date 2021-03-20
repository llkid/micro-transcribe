/// 本地转写

#include "transcribebin/RecordUtil.h"
#include <conio.h>


int main(int argc, char* argv[]) {

	UINT inDevs = waveInGetNumDevs();
	std::cout << "waveInGetNumDevs: " << inDevs << '\n';

	if (!inDevs) {
		std::cout << "\n当前没有发现可用音频输入设备，请按ESC键退出\n";
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