#pragma once

#include "feat/wave-reader.h"
#include "online2/online-nnet3-decoding.h"
#include "online2/online-nnet2-feature-pipeline.h"
#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"
#include "util/kaldi-thread.h"
#include "nnet3/nnet-utils.h"

#include <memory>

namespace kaldi {
	class Transcriber
	{
		typedef std::unique_ptr<ParseOptions> ParseOptionsPtr;
		typedef std::unique_ptr<OnlineNnet2FeaturePipelineInfo> OnlineNnet2FeaturePipelineInfoPtr;
		typedef std::unique_ptr<nnet3::DecodableNnetSimpleLoopedInfo> DecodableNnetSimpleLoopedInfoPtr;
		typedef std::unique_ptr<OnlineNnet2FeaturePipeline> OnlineNnet2FeaturePipelinePtr;
		typedef std::unique_ptr<SingleUtteranceNnet3Decoder> SingleUtteranceNnet3DecoderPtr;
		typedef std::unique_ptr<OnlineSilenceWeighting> OnlineSilenceWeightingPtr;

	public:
		Transcriber(const char* usage);
		~Transcriber();
		int32 ParseInit(int argc, char* argv[]);
		int32 LoadAM();
		int32 ParseStart(Vector<BaseFloat> wave_part);
		int32 ParseStop();

	public:
		size_t chunk_len;

	private:
		void ParseReset();

	private:
		ParseOptionsPtr po;
		OnlineNnet2FeaturePipelineConfig feature_opts;
		nnet3::NnetSimpleLoopedComputationOptions decodable_opts;
		LatticeFasterDecoderConfig decoder_opts;
		OnlineEndpointConfig endpoint_opts;
		TransitionModel trans_model;
		nnet3::AmNnetSimple am_nnet;
		OnlineNnet2FeaturePipelineInfoPtr feature_info;
		DecodableNnetSimpleLoopedInfoPtr decodable_info;
		fst::Fst<fst::StdArc>* decode_fst;
		fst::SymbolTable* word_syms;
		OnlineNnet2FeaturePipelinePtr feature_pipeline;
		SingleUtteranceNnet3DecoderPtr decoder;
		OnlineSilenceWeightingPtr silence_weighting;
		std::vector<std::pair<int32, BaseFloat>> delta_weights;

		BaseFloat chunk_length_secs;
		BaseFloat output_period;
		BaseFloat samp_freq;
		BaseFloat frame_shift;
		int32 frame_subsampling;
		int read_timeout;
		bool produce_time;
		int g_num_threads;

		int32 samp_count;// this is used for output refresh rate
		int32 check_period;
		int32 check_count;
		int32 frame_offset;

	};

	static std::string LatticeToString(const Lattice& lat, const fst::SymbolTable& word_syms) {
		LatticeWeight weight;
		std::vector<int32> alignment;
		std::vector<int32> words;
		GetLinearSymbolSequence(lat, &alignment, &words, &weight);

		std::ostringstream msg;
		for (size_t i = 0; i < words.size(); i++) {
			std::string s = word_syms.Find(words[i]);
			if (s.empty()) {
				KALDI_WARN << "Word-id " << words[i] << " not in symbol table.";
				msg << "<#" << std::to_string(i) << "> ";
			}
			else
				msg << s << " ";
		}
		return msg.str();
	}

	static std::string GetTimeString(int32 t_beg, int32 t_end, BaseFloat time_unit) {
		char buffer[100];
		double t_beg2 = t_beg * time_unit;
		double t_end2 = t_end * time_unit;
		snprintf(buffer, 100, "%.2f %.2f", t_beg2, t_end2);
		return std::string(buffer);
	}

	static int32 GetLatticeTimeSpan(const Lattice& lat) {
		std::vector<int32> times;
		LatticeStateTimes(lat, &times);
		return times.back();
	}

	static std::string LatticeToString(const CompactLattice& clat, const fst::SymbolTable& word_syms) {
		if (clat.NumStates() == 0) {
			KALDI_WARN << "Empty lattice.";
			return "";
		}
		CompactLattice best_path_clat;
		CompactLatticeShortestPath(clat, &best_path_clat);

		Lattice best_path_lat;
		ConvertLattice(best_path_clat, &best_path_lat);
		return LatticeToString(best_path_lat, word_syms);
	}
}
