#include <iostream>
#include <thread>
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}
using namespace std;
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib, "avcodec.lib")

static double r2d(AVRational r)
{
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

void XSleep(int ms)
{
	//c++ 11
	chrono::milliseconds du(ms);
	this_thread::sleep_for(du);
}

int main(int argc, char* argv[])
{
    
	cout << "Test Demux FFmpeg.club" << endl;

	const char *path = "v1080.mp4";

	//初始化封装库
	av_register_all();

	//初始化网络库 (可以打开rtsp rtmp http 协议的流媒体视频)
	avformat_network_init();

	//注册解码器
	avcodec_register_all();

	//参数设置
	AVDictionary *opts = NULL;
	//设置rtsp流以tcp协议打开
	av_dict_set(&opts, "rtsp_transport", "tcp", 0);

	//网络延时时间
	av_dict_set(&opts, "max_delay", "500", 0);

	//解封装上下文
	AVFormatContext *ic = NULL;
	int re = avformat_open_input(
		&ic,
		path,
		0,		//0	表示自动选择解封器
		&opts	//参数设置， 比如rtsp的延时时间
	);
	if (re != 0)
	{
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "open" << path << "failed!:" << buf << endl;
		return -1;
	}
	cout << "open" << path << "success!" << endl;


	//获取流信息
	re = avformat_find_stream_info(ic, 0);

	//获取总时长 毫秒
	int totalMs = ic->duration / (AV_TIME_BASE / 1000);
	cout << "总时长：" << totalMs << "毫秒" << endl;

	//打印视频流的详细信息
	av_dump_format(ic, 0, path, 0);


	//音视频索引，读取时区分音视频
	int videoStream = 0;
	int audioStream = 1;

	//获取音频流信息 （遍历 函数获取）
	for (int i = 0; i < ic->nb_streams; i++)
	{
		AVStream *as = ic->streams[i];

		//音频 AVMEDIA_TYPE_AUDIO
		if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStream = i;
			cout << "音频信息" << endl;
			//采样率
			cout << "sample_rate = " << as->codecpar->sample_rate << endl;
			// AVSampleFormat
			cout << "format = " << as->codecpar->format << endl;
			//通道数
			cout << "channels = " << as->codecpar->channels << endl;
			cout << "codec_id = " << as->codecpar->codec_id << endl;
			cout << "audio fps = " << r2d(as->avg_frame_rate) << endl;

			//一帧数据？？ 单通道样本数
			cout << "frame_size = " << as->codec->frame_size << endl;
			// 1024 * 2 * 2 = 4096  fps = sample_rate/frame_size

		}
		//视频 AVMEDIA_TYPE_VIDEO
		else if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = i;
			cout << "视频信息" << endl;
			//宽高数据有可能没有 比较可靠的是解码后的AVFamily
			//宽度
			cout << "width = " << as->codecpar->width << endl;
			//高度
			cout << "height = " << as->codecpar->height << endl;
			//帧率 fps AVRational
			cout << "video fps = " << r2d(as->avg_frame_rate) << endl;
		}
	}
	//第二种获取流信息的方法
	//获取视频流
	videoStream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	cout << "channels = " << ic->streams[videoStream]->codecpar->channels << endl;
	//获取音频流
	audioStream = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	cout << "channels = " << ic->streams[audioStream]->codecpar->channels << endl;


	/////////////////////////////////////////////打开音视频解码器begin/////////////////////////////////////////////////////////////
	///视频解码器打开
	///找到视频解码器
	AVCodec *vcodec = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id);
	//这个地方有可能找不到视频解码器，所以加个判断
	if (!vcodec)
	{
		cout << "can't find the codec id " << ic->streams[videoStream]->codecpar->codec_id << endl;
		getchar();
		return -1;
	}
	//找到解码器
	cout << "find the codec id " << ic->streams[videoStream]->codecpar->codec_id << endl;

	///创建解码器上下文
	AVCodecContext *vc = avcodec_alloc_context3(vcodec);

	///配置解码器上下文参数
	avcodec_parameters_to_context(vc, ic->streams[videoStream]->codecpar);
	//八线程解码
	vc->thread_count = 8;

	///打开解码器上下文
	re = avcodec_open2(vc, 0, 0);
	//如果re！=0就是失败了
	if (re != 0)
	{
		//用buffer来存放错误信息
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "open" << path << "failed!:" << buf << endl;
		getchar();
		return -1;
	}
	//打开成功
	cout << "video avcodec_open2 success!" << endl;

	///音频解码器打开
	///找到音频解码器
	AVCodec *acodec = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id);
	//这个地方有可能找不到音频解码器，所以加个判断
	if (!acodec)
	{
		cout << "can't find the codec id " << ic->streams[audioStream]->codecpar->codec_id << endl;
		getchar();
		return -1;
	}
	//找到解码器
	cout << "find the codec id " << ic->streams[audioStream]->codecpar->codec_id << endl;

	///创建解码器上下文
	AVCodecContext *ac = avcodec_alloc_context3(acodec);

	///配置解码器上下文参数
	avcodec_parameters_to_context(ac, ic->streams[audioStream]->codecpar);
	//八线程解码
	ac->thread_count = 8;

	///打开解码器上下文
	re = avcodec_open2(ac, 0, 0);
	//如果re！=0就是失败了
	if (re != 0)
	{
		//用buffer来存放错误信息
		char buf[1024] = { 0 };
		av_strerror(re, buf, sizeof(buf) - 1);
		cout << "open" << path << "failed!:" << buf << endl;
		getchar();
		return -1;
	}
	//打开成功
	cout << "audio avcodec_open2 success!" << endl;
	/////////////////////////////////////////////打开音视频解码器end/////////////////////////////////////////////////////////////



	

	//malloc AVPacket并初始化 一帧数据
	AVPacket *pkt = av_packet_alloc();

	//AVFrame比AVPacket的空间大很多
	AVFrame *frame = av_frame_alloc();

	//像素格式和尺寸转换上下文
	SwsContext *vctx = NULL;


	for (;;)
	{

		int re = av_read_frame(ic, pkt);

		//re != 0 是失败的  re == 0是成功的
		if (re != 0)
		{
			//到了结尾处 回到开头第三秒的位置 循环播放
			cout << "===================================end=====================================" << endl;
			int ms = 3000; //三秒位置 根据时间基数（分数）转换
			long long pos = (double)ms / (double)1000 * r2d(ic->streams[pkt->stream_index]->time_base);
			//av_seek_frame 相当于拖拉条：满足条件后定位到关键帧，AVSEEK_FLAG_BACKWARD往后找；如果任意帧很危险，如果不是关键帧是解码不出来的
			//并没有定位到你想要的真正位置，只是定位到你想要的位置的前一个关键帧位置
			av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
			continue;
		}

		cout << "pkt->size = " << pkt->size << endl;

		//显示时间
		cout << "pkt->pts = " << pkt->pts << endl;
		//转换为毫秒，方便做同步
		cout << "pkt->pts ms = " << pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000) << endl;


		//解码时间
		cout << "pkt->dts = " << pkt->dts << endl;

		AVCodecContext *cc = 0;

		//一帧的数据
		if (pkt->stream_index == videoStream)
		{
			cout << "图像" << endl;
			//vc：视频解码器上下文
			cc = vc;

		}
		if (pkt->stream_index == audioStream)
		{
			cout << "音频" << endl;
			//ac：音频解码器上下文
			cc = ac;
		}


		/////////////////////////////////////////////解码视频begin/////////////////////////////////////////////////////////////

		//avcodec_send_frame:把packet写入解码队列
		//avcodec_receive_frame：从已解码成功的数据中取出一个frame；  会一直receive 直到receive不到为止； 发送一个可能会收到多个（可能发出一个收到10个，要循环一直收）

		///解码视频
		//发送packet到解码线程   传NULL后调用多次receive取出所有缓冲帧
		re = avcodec_send_packet(cc, pkt);
		//释放，引用计数-1 为0释放空间
		// 如果不释放空间 内存会一直被占用
		av_packet_unref(pkt);

		//有可能失败，re不等于0就是失败
		if (re != 0)
		{
			//用buffer来存放错误信息
			char buf[1024] = { 0 };
			av_strerror(re, buf, sizeof(buf) - 1);
			cout << "avcodec_send_packet failed!:" << buf << endl;
			getchar();
			continue;
		}
		for (;;)
		{
			//从线程中获取解码接口，一次send可能对应多次receive，要进行多次接收，直到收不到为止
			re = avcodec_receive_frame(cc, frame);
			//收不到就结束
			if (re != 0)
			{
				break;
			}
			//linesize：对于图像格式存的是一行数据的大小；对于音频格式存的是左右声道的大小
			//存放的目的主要是对齐，按行做拷贝，不然会出现错位的情况
			cout << "receive frame " << frame->format << " " << frame->linesize[0] << endl;

			//vc表示视频
			if (cc == vc)
			{
				vctx = sws_getCachedContext(
					vctx,							//传NULL会新创建
					frame->width, frame->height,	//输入的宽和高
					(AVPixelFormat)frame->format,	//格式(YUV420P)
					frame->width, frame->height,	//输出的宽和高
					AV_PIX_FMT_RGBA,				//输入格式RGBA
					SWS_BILINEAR,					//尺寸变换的算法
					0,0,0							//无用参数，全部设为0
					);
				cout << "像素格式尺寸转换上下文创建或者获取成功" << endl;
			}
			

		}
		








		/////////////////////////////////////////////解码视频end/////////////////////////////////////////////////////////////






		//XSleep()函数 打印慢点
		//XSleep(500);

	}

	av_frame_free(&frame);
	av_packet_free(&pkt);


	if (ic)
	{
		//释放封装上下文，并且把ic置为0
		avformat_close_input(&ic);
	}

	getchar();
	return 0;
}