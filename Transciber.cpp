#include "Transciber.h"

namespace kaldi {
	Transcriber::Transcriber(const char* usage)
		: chunk_length_secs(0.18)
		, output_period(1)
		, samp_freq(16000.0)
		, read_timeout(-1)
		, produce_time(false)
		, g_num_threads(1)
		, word_syms(NULL)
	{
		po = ParseOptionsPtr(new ParseOptions(usage));
	}

	Transcriber::~Transcriber()
	{
	}

	int32 Transcriber::ParseInit(int argc, char* argv[])
	{
		po->Register("samp-freq", &samp_freq,
			"Sampling frequency of the input signal (coded as 16-bit slinear).");
		po->Register("chunk-length", &chunk_length_secs,
			"Length of chunk size in seconds, that we process.");
		po->Register("output-period", &output_period,
			"How often in seconds, do we check for changes in output.");
		po->Register("num-threads-startup", &g_num_threads,
			"Number of threads used when initializing iVector extractor.");
		po->Register("read-timeout", &read_timeout,
			"Number of seconds of timeout for TCP audio data to appear on the stream. Use -1 for blocking.");
		po->Register("produce-time", &produce_time,
			"Prepend begin/end times between endpoints (e.g. '5.46 6.81 <text_output>', in seconds)");

		feature_opts.Register(po.get());
		decodable_opts.Register(po.get());
		decoder_opts.Register(po.get());
		endpoint_opts.Register(po.get());

		po->Read(argc, argv);

		if (po->NumArgs() != 3) {
			po->PrintUsage();
			return 0;
		}

		return 1;
	}

	int32 Transcriber::LoadAM()
	{
		try
		{
			std::string nnet3_rxfilename = po->GetArg(1),
				fst_rxfilename = po->GetArg(2),
				word_syms_filename = po->GetArg(3);

			feature_info = OnlineNnet2FeaturePipelineInfoPtr(new OnlineNnet2FeaturePipelineInfo(feature_opts));

			frame_shift = feature_info->FrameShiftInSeconds();
			frame_subsampling = decodable_opts.frame_subsampling_factor;

			KALDI_VLOG(1) << "Loading AM...";

			{
				bool binary;
				Input ki(nnet3_rxfilename, &binary);
				trans_model.Read(ki.Stream(), binary);
				am_nnet.Read(ki.Stream(), binary);
				SetBatchnormTestMode(true, &(am_nnet.GetNnet()));
				SetDropoutTestMode(true, &(am_nnet.GetNnet()));
				nnet3::CollapseModel(nnet3::CollapseModelConfig(), &(am_nnet.GetNnet()));
			}

			decodable_info = DecodableNnetSimpleLoopedInfoPtr(
				new nnet3::DecodableNnetSimpleLoopedInfo(decodable_opts, &am_nnet)
			);

			KALDI_VLOG(1) << "Loading FST...";

			decode_fst = fst::ReadFstKaldiGeneric(fst_rxfilename);

			if (!word_syms_filename.empty())
				if (!(word_syms = fst::SymbolTable::ReadText(word_syms_filename)))
					KALDI_ERR << "Could not read symbol table from file "
					<< word_syms_filename;

			samp_count = 0;// this is used for output refresh rate
			chunk_len = static_cast<size_t>(chunk_length_secs * samp_freq);
			check_period = static_cast<int32>(samp_freq * output_period);
			check_count = check_period;
			frame_offset = 0;

			feature_pipeline = OnlineNnet2FeaturePipelinePtr(
				new OnlineNnet2FeaturePipeline(*feature_info)
			);
			decoder = SingleUtteranceNnet3DecoderPtr(
				new SingleUtteranceNnet3Decoder(decoder_opts, trans_model,
					*decodable_info,
					*decode_fst, feature_pipeline.get())
			);

			ParseReset();
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what();
			return 0;
		}

		return 1;
	}

	void Transcriber::ParseReset()
	{
		decoder->InitDecoding(frame_offset);
		silence_weighting = OnlineSilenceWeightingPtr(
			new OnlineSilenceWeighting(
				trans_model,
				feature_info->silence_weighting_config,
				decodable_opts.frame_subsampling_factor
			)
		);

		delta_weights.clear();
	}

	int32 Transcriber::ParseStart(Vector<BaseFloat> wave_part)
	{
		try
		{
			/// TODO read normal data
			feature_pipeline->AcceptWaveform(samp_freq, wave_part);
			samp_count += chunk_len;

			if (silence_weighting->Active() &&
				feature_pipeline->IvectorFeature() != NULL) {
				silence_weighting->ComputeCurrentTraceback(decoder->Decoder());
				silence_weighting->GetDeltaWeights(feature_pipeline->NumFramesReady(),
					frame_offset * decodable_opts.frame_subsampling_factor,
					&delta_weights);
				feature_pipeline->UpdateFrameWeights(delta_weights);
			}

			decoder->AdvanceDecoding();

			if (samp_count > check_count) {
				if (decoder->NumFramesDecoded() > 0) {
					Lattice lat;
					decoder->GetBestPath(false, &lat);
					TopSort(&lat); // for LatticeStateTimes(),
					std::string msg = LatticeToString(lat, *word_syms);

					// get time-span after previous endpoint,
					if (produce_time) {
						int32 t_beg = frame_offset;
						int32 t_end = frame_offset + GetLatticeTimeSpan(lat);
						msg = GetTimeString(t_beg, t_end, frame_shift * frame_subsampling) + " " + msg;
					}

					KALDI_VLOG(1) << "Temporary transcript: " << msg;
					std::cout << msg << '\n';
				}
				check_count += check_period;
			}

			if (decoder->EndpointDetected(endpoint_opts)) {
				decoder->FinalizeDecoding();
				frame_offset += decoder->NumFramesDecoded();
				CompactLattice lat;
				decoder->GetLattice(true, &lat);
				std::string msg = LatticeToString(lat, *word_syms);

				// get time-span between endpoints,
				if (produce_time) {
					int32 t_beg = frame_offset - decoder->NumFramesDecoded();
					int32 t_end = frame_offset;
					msg = GetTimeString(t_beg, t_end, frame_shift * frame_subsampling) + " " + msg;
				}

				KALDI_VLOG(1) << "Endpoint, sending message: " << msg;
				std::cout << msg << '\n';

				ParseReset();
			}
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what();
			return 0;
		}

		return 1;
	}

	int32 Transcriber::ParseStop()
	{
		try
		{
			/// TODO read last data
			feature_pipeline->InputFinished();

			if (silence_weighting->Active() &&
				feature_pipeline->IvectorFeature() != NULL) {
				silence_weighting->ComputeCurrentTraceback(decoder->Decoder());
				silence_weighting->GetDeltaWeights(feature_pipeline->NumFramesReady(),
					frame_offset * decodable_opts.frame_subsampling_factor,
					&delta_weights);
				feature_pipeline->UpdateFrameWeights(delta_weights);
			}

			decoder->AdvanceDecoding();
			decoder->FinalizeDecoding();
			frame_offset += decoder->NumFramesDecoded();
			if (decoder->NumFramesDecoded() > 0) {
				CompactLattice lat;
				decoder->GetLattice(true, &lat);
				std::string msg = LatticeToString(lat, *word_syms);

				// get time-span from previous endpoint to end of audio,
				if (produce_time) {
					int32 t_beg = frame_offset - decoder->NumFramesDecoded();
					int32 t_end = frame_offset;
					msg = GetTimeString(t_beg, t_end, frame_shift * frame_subsampling) + " " + msg;
				}

				KALDI_VLOG(1) << "EndOfAudio, sending message: " << msg;
				std::cout << msg << '\n';
			}
			else
				std::cout << '\n';

			ParseReset();
		}
		catch (const std::exception& e)
		{
			std::cerr << e.what();
			return 0;
		}
		return 1;
	}

}
