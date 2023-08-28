#pragma once
#include<string>

extern "C" {
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <memory>

typedef struct OutputStream {
	AVStream* st;
	AVCodecContext* enc;

	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;

	AVFrame* frame;
	AVFrame* tmp_frame;

	AVPacket* tmp_pkt;

	float t, tincr, tincr2;

	struct SwsContext* sws_ctx;
	struct SwrContext* swr_ctx;
} OutputStream;

class EncodeMux
{
public:
	EncodeMux();
	~EncodeMux();

	int Init(int argc, char** argv);
	bool Start();
	void Stop();

	int EncodeAndMux(void* data);


private:

	/* Add an output stream. */
	void AddStream(OutputStream* ost, AVFormatContext* oc, const AVCodec** codec, enum AVCodecID codec_id);

	void OpenAudio(AVFormatContext* oc, const AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg);

	void OpenVideo(AVFormatContext* oc, const AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg);

	AVFrame* AllocAudioFrame(enum AVSampleFormat sample_fmt,
		const AVChannelLayout* channel_layout,
		int sample_rate, int nb_samples);

	AVFrame* AllocFrame(enum AVPixelFormat pix_fmt, int width, int height);

	/*
	* encode one video frame and send it to the muxer
	* return 1 when encoding is finished, 0 otherwise
	*/
	int WriteVideoFrame(AVFormatContext* oc, OutputStream* ost);

	int WriteAudioFrame(AVFormatContext* oc, OutputStream* ost);

	void CloseStream(AVFormatContext* oc, OutputStream* ost);

	int WriteFrame(AVFormatContext* fmt_ctx, AVCodecContext* c,
		AVStream* st, AVFrame* frame, AVPacket* pkt);

	AVFrame* GetVideoFrame(OutputStream* ost);

	void FillYuvImage(AVFrame* pict, int frame_index,int width, int height);

	AVFrame* GetAudioFrame(OutputStream* ost);

private:
	OutputStream video_st_ = { 0 };
	OutputStream audio_st_ = { 0 };

	const AVOutputFormat* fmt_;
	AVFormatContext* oc_;
	const AVCodec* audio_codec_;
	const AVCodec* video_codec_;
	AVDictionary* opt = NULL;

	int have_video_ = 0;
	int have_audio_ = 0;

	const char* filename = "D:\\workspace\\screen-recording-tool\\Recording\\x64\\Debug\\test.mp4";
};

