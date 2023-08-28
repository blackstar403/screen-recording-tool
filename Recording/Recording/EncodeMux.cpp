#include "EncodeMux.h"

#define STREAM_DURATION   90.0
#define STREAM_FRAME_RATE 60 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC
#include <time.h>
#include <profileapi.h>

extern "C"
{
#include <libavutil/imgutils.h>
}



EncodeMux::EncodeMux()
{
}

EncodeMux::~EncodeMux()
{
}

int EncodeMux::Init(int argc, char** argv)
{	
	for (int i = 2; i + 1 < argc; i += 2) {
		if (!strcmp(argv[i], "-flags") || !strcmp(argv[i], "-fflags"))
			av_dict_set(&opt, argv[i] + 1, argv[i + 1], 0);
	}

	/* allocate the output media context */
	avformat_alloc_output_context2(&oc_, NULL, NULL, filename);
	if (!oc_) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc_, NULL, "mpeg", filename);
	}
	if (!oc_)
		return 1;

	fmt_ = oc_->oformat;

	return 0;
}

bool EncodeMux::Start()
{
	int ret = 0;
	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	if (fmt_->video_codec != AV_CODEC_ID_NONE) {
		AddStream(&video_st_, oc_, &video_codec_, fmt_->video_codec);
		have_video_ = 1;
	}
	if (fmt_->audio_codec != AV_CODEC_ID_NONE) {
		AddStream(&audio_st_, oc_, &audio_codec_, fmt_->audio_codec);
		have_audio_ = 1;
	}


	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	if (have_video_)
		OpenVideo(oc_, video_codec_, &video_st_, opt);

	if (have_audio_)
		OpenAudio(oc_, audio_codec_, &audio_st_, opt);

	av_dump_format(oc_, 0, filename, 1);

	/* open the output file, if needed */
	if (!(fmt_->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc_->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open '%s'\n", filename);
			return 1;
		}
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc_, &opt);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		return 1;
	}

	return 0;
}

void EncodeMux::Stop()
{
	av_write_trailer(oc_);

	/* Close each codec. */
	if (have_video_)
		CloseStream(oc_, &video_st_);
	if (have_audio_)
		CloseStream(oc_, &audio_st_);

	if (!(fmt_->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&oc_->pb);

	/* free the stream */
	avformat_free_context(oc_);
}

uint64_t os_gettime_ns1(void)
{
	LARGE_INTEGER current_time;
	QueryPerformanceCounter(&current_time);
	return (uint64_t)current_time.QuadPart;
}

int EncodeMux::EncodeAndMux(void* data)
{
	//int linesize[4] = {10240,10240,10240,10240};

	// 将数据复制到AVFrame
	/*av_image_copy(video_st_.frame->data
		, video_st_.frame->linesize
		, (const uint8_t**)data
		, linesize,
		AV_PIX_FMT_BGR0
		, 2560
		, 1440);*/

	/*if (av_compare_ts(video_st_.next_pts, video_st_.enc->time_base,audio_st_.next_pts, audio_st_.enc->time_base) <= 0) 
	{
		encode_video = !WriteVideoFrame(oc_, &video_st_);
	}
	else {
		encode_audio = !WriteAudioFrame(oc_, &audio_st_);
	}*/
	int encode_video = 1;
	int encode_audio = 1;
	
	while (encode_video || encode_audio) {
		/* select the stream to encode */
		if (encode_video &&
			(!encode_audio || av_compare_ts(video_st_.next_pts, video_st_.enc->time_base,
				audio_st_.next_pts, audio_st_.enc->time_base) <= 0)) {
			auto start_time = 
			encode_video = !WriteVideoFrame(oc_, &video_st_);
		}
		else {
			encode_audio = !WriteAudioFrame(oc_, &audio_st_);
		}
	}

	return 0;
}

void EncodeMux::OpenAudio(AVFormatContext* oc, const AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg)
{
	AVCodecContext* c;
	int nb_samples;
	int ret;
	AVDictionary* opt = NULL;

	c = ost->enc;

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open audio codec\n");
		exit(1);
	}

	/* init signal generator */
	ost->t = 0;
	ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	ost->frame = AllocAudioFrame(c->sample_fmt, &c->ch_layout,
		c->sample_rate, nb_samples);
	ost->tmp_frame = AllocAudioFrame(AV_SAMPLE_FMT_S16, &c->ch_layout,
		c->sample_rate, nb_samples);

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}

	/* create resampler context */
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		fprintf(stderr, "Could not allocate resampler context\n");
		exit(1);
	}

	/* set options */
	av_opt_set_chlayout(ost->swr_ctx, "in_chlayout", &c->ch_layout, 0);
	av_opt_set_int(ost->swr_ctx, "in_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_chlayout(ost->swr_ctx, "out_chlayout", &c->ch_layout, 0);
	av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

	/* initialize the resampling context */
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		fprintf(stderr, "Failed to initialize the resampling context\n");
		exit(1);
	}

}

void EncodeMux::OpenVideo(AVFormatContext* oc, const AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg)
{
	int ret;
	AVCodecContext* c = ost->enc;
	AVDictionary* opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open video codec\n");
		exit(1);
	}

	/* allocate and init a re-usable frame */
	ost->frame = AllocFrame(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	 * picture is needed too. It is then converted to the required
	 * output format. */
	ost->tmp_frame = NULL;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		ost->tmp_frame = AllocFrame(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!ost->tmp_frame) {
			fprintf(stderr, "Could not allocate temporary video frame\n");
			exit(1);
		}
	}

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}

}

int EncodeMux::WriteVideoFrame(AVFormatContext* oc, OutputStream* ost)
{
	return WriteFrame(oc, ost->enc, ost->st, GetVideoFrame(ost), ost->tmp_pkt);
}

int EncodeMux::WriteAudioFrame(AVFormatContext* oc, OutputStream* ost)
{
	AVCodecContext* c;
	AVFrame* frame;
	int ret;
	int dst_nb_samples;

	c = ost->enc;

	frame = GetAudioFrame(ost);

	if (frame) {
		/* convert samples from native format to destination codec format, using the resampler */
		/* compute destination number of samples */
		dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
			c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);

		/* when we pass a frame to the encoder, it may keep a reference to it
		 * internally;
		 * make sure we do not overwrite it here
		 */
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0)
			exit(1);

		/* convert to destination format */
		ret = swr_convert(ost->swr_ctx,
			ost->frame->data, dst_nb_samples,
			(const uint8_t**)frame->data, frame->nb_samples);
		if (ret < 0) {
			fprintf(stderr, "Error while converting\n");
			exit(1);
		}
		frame = ost->frame;

		AVRational bp = { 1, c->sample_rate };
		frame->pts = av_rescale_q(ost->samples_count, bp, c->time_base);
		ost->samples_count += dst_nb_samples;
	}

	return WriteFrame(oc, c, ost->st, frame, ost->tmp_pkt);
}

void EncodeMux::CloseStream(AVFormatContext* oc, OutputStream* ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	av_packet_free(&ost->tmp_pkt);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}

int EncodeMux::WriteFrame(AVFormatContext* fmt_ctx, AVCodecContext* c, AVStream* st, AVFrame* frame, AVPacket* pkt)
{
	int ret;

	// send the frame to the encoder
	ret = avcodec_send_frame(c, frame);
	if (ret < 0) {
		fprintf(stderr, "Error sending a frame to the encoder\n");
		exit(1);
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(c, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0) {
			fprintf(stderr, "Error encoding a frame\n");
			exit(1);
		}

		/* rescale output packet timestamp values from codec to stream timebase */
		av_packet_rescale_ts(pkt, c->time_base, st->time_base);
		pkt->stream_index = st->index;

		/* Write the compressed frame to the media file. */
		ret = av_interleaved_write_frame(fmt_ctx, pkt);
		/* pkt is now blank (av_interleaved_write_frame() takes ownership of
		 * its contents and resets pkt), so that no unreferencing is necessary.
		 * This would be different if one used av_write_frame(). */
		if (ret < 0) {
			fprintf(stderr, "Error while writing output packet\n");
			exit(1);
		}
	}

	return ret == AVERROR_EOF ? 1 : 0;
}

AVFrame* EncodeMux::GetVideoFrame(OutputStream* ost)
{
	AVCodecContext* c = ost->enc;

	AVRational tb_b = { 1,1 };

	/* check if we want to generate more frames */
	if (av_compare_ts(ost->next_pts, c->time_base,
		STREAM_DURATION, tb_b) > 0)
		return NULL;

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally; make sure we do not overwrite it here */
	if (av_frame_make_writable(ost->frame) < 0)
		exit(1);

	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		/* as we only generate a YUV420P picture, we must convert it
		 * to the codec pixel format if needed */
		if (!ost->sws_ctx) {
			ost->sws_ctx = sws_getContext(c->width, c->height,
				AV_PIX_FMT_YUV420P,
				c->width, c->height,
				c->pix_fmt,
				SCALE_FLAGS, NULL, NULL, NULL);
			if (!ost->sws_ctx) {
				fprintf(stderr,
					"Could not initialize the conversion context\n");
				exit(1);
			}
		}
		FillYuvImage(ost->tmp_frame, ost->next_pts, c->width, c->height);
		sws_scale(ost->sws_ctx, (const uint8_t* const*)ost->tmp_frame->data,
			ost->tmp_frame->linesize, 0, c->height, ost->frame->data,
			ost->frame->linesize);
	}
	else {
		FillYuvImage(ost->frame, ost->next_pts, c->width, c->height);
	}

	ost->frame->pts = ost->next_pts++;

	return ost->frame;
}

void EncodeMux::FillYuvImage(AVFrame* pict, int frame_index, int width, int height)
{
	int x, y, i;

	i = frame_index;

	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}

AVFrame* EncodeMux::GetAudioFrame(OutputStream* ost)
{
	AVFrame* frame = ost->tmp_frame;
	int j, i, v;
	int16_t* q = (int16_t*)frame->data[0];

	AVRational tb_b = { 1,1 };
	/* check if we want to generate more frames */
	if (av_compare_ts(ost->next_pts, ost->enc->time_base,
		STREAM_DURATION, tb_b) > 0)
		return NULL;

	for (j = 0; j < frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->enc->ch_layout.nb_channels; i++)
			*q++ = v;
		ost->t += ost->tincr;
		ost->tincr += ost->tincr2;
	}

	frame->pts = ost->next_pts;
	ost->next_pts += frame->nb_samples;

	return frame;
}

AVFrame* EncodeMux::AllocFrame(AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame* frame;
	int ret;

	frame = av_frame_alloc();
	if (!frame)
		return NULL;

	frame->format = pix_fmt;
	frame->width = width;
	frame->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}

	return frame;
}

AVFrame* EncodeMux::AllocAudioFrame(AVSampleFormat sample_fmt, const AVChannelLayout* channel_layout, int sample_rate, int nb_samples)
{
	AVFrame* frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Error allocating an audio frame\n");
		exit(1);
	}

	frame->format = sample_fmt;
	av_channel_layout_copy(&frame->ch_layout, channel_layout);
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		if (av_frame_get_buffer(frame, 0) < 0) {
			fprintf(stderr, "Error allocating an audio buffer\n");
			exit(1);
		}
	}

	return frame;
}

void EncodeMux::AddStream(OutputStream* ost, AVFormatContext* oc, const AVCodec** codec, AVCodecID codec_id)
{
	AVCodecContext* c;
	int i;

	/* find the encoder */
	if (codec_id == AV_CODEC_ID_H264)
	{
		*codec = avcodec_find_encoder_by_name("h264_nvenc");
	}
	else
	{
		*codec = avcodec_find_encoder(codec_id);
	}
		
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n",
			avcodec_get_name(codec_id));
		exit(1);
	}

	ost->tmp_pkt = av_packet_alloc();
	if (!ost->tmp_pkt) {
		fprintf(stderr, "Could not allocate AVPacket\n");
		exit(1);
	}

	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if (!c) {
		fprintf(stderr, "Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;

	switch ((*codec)->type)
	{
	case AVMEDIA_TYPE_AUDIO:
	{
		c->sample_fmt = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate = 64000;
		c->sample_rate = 44100;
		if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i]; i++) {
				if ((*codec)->supported_samplerates[i] == 44100)
					c->sample_rate = 44100;
			}
		}

		AVChannelLayout tmp_layout = { AV_CHANNEL_ORDER_NATIVE,2, AV_CH_LAYOUT_STEREO ,NULL };
		av_channel_layout_copy(&c->ch_layout, &tmp_layout);

		ost->st->time_base = { 1, c->sample_rate };
		break;
	}


	case AVMEDIA_TYPE_VIDEO:
	{
		c->codec_id = codec_id;

		c->bit_rate = 40000000;
		/* Resolution must be a multiple of two. */
		c->width = 2560;
		c->height = 1440;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		ost->st->time_base = { 1, STREAM_FRAME_RATE };
		c->time_base = ost->st->time_base;

		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B-frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		break;
	}

	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}
