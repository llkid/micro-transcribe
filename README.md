# micro-transcribe
kaldi 本地转写测试

win10平台、vs2017、win sdk8.1
依赖：编译好的kaldi源码
1、OpenBLAS-v0.2.14-Win64-int64
2、openfst
3、portaudio && asiosdk_2.3.3_2019-06-14

kaldi编译：
1、配置windows下的文件夹进行编译工作
2、在（kaldi/src）源码目录下新建transcribebin（自己需要的文件夹），添加入口源文件
3、修改../kaldi/src下的Makefile文件，transcribebin下添加需要的Makefile
4、修改../kaldi下的CMakeLists文件
5、参考../kaldi/windows下的INSTALL.md Markdown文件进行编译
6、使用vs2017编译../kaldi/kaldiwin_vs2017_OPENBLAS下的vs工程

可能需要的依赖库:
1、libgcc_s_seh-1.dll
2、libgfortran-3.dll
3、libopenblas.dll（64bits）

添加必要的模型与配置文件
1、online.conf
2、final.mdl
3、HCLG.fst
4、words.txt

运行参数：
  ./micro-transcribe.exe --samp-freq=16000 --frames-per-chunk=20 --extra-left-context-initial=0 --frame-subsampling-factor=3 --config=.../online.conf --min-active=200 --max-active=7000 --beam=15.0 --lattice-beam=6.0 --acoustic-scale=1.0 .../final.mdl .../HCLG.fst .../words.txt
