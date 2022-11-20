#include <iostream>
#include <fstream>
extern "C" {
#include <libavformat/avformat.h>
#include "libavutil/avutil.h"
#include "libavcodec/codec.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
}

using std::cin;
using std::cout;
using std::endl;
using std::ofstream;
using std::ios_base;

int main() {
    AVFormatContext* ic = nullptr;
    //rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4
    const char* in_path = "C:\\Users\\casair\\Downloads\\ke.mp4"; //输入文件
    const char* out_path = "C:\\Users\\casair\\Desktop\\rjm-out.yuv"; //输出文件
    int ret;

    ret = avformat_open_input(&ic, in_path, NULL, NULL);
    if (ret < 0) {
        cout << "打开文件失败" << endl;
        return ret;
    }
    cout << "打开文件成功: " << in_path << endl;
    //av_dump_format(ic, 0, inputFile, 0); //打印媒体信息

    ret = avformat_find_stream_info(ic, NULL); //查询流信息
    if (ret < 0) {
        avformat_free_context(ic);
        cout << "查询流信息失败" << endl;
        return ret;
    }
    cout << "查询流信息成功...." << endl;

    const AVCodec* codec;
    ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0); //获取视频流索引
    if (ret < 0) {
        cout << "获取视频流索引失败" << endl;
        return ret;
    }
    cout << "获取视频流索引成功:" << ret << endl;
    int videoStreamIndex = ret;
    AVStream* videoStream = ic->streams[ret];
    cout << "视频编码:" << avcodec_get_name(videoStream->codecpar->codec_id) << endl; //输出视频流编码名称
    cout << "视频尺寸:" << videoStream->codecpar->width << "x" << videoStream->codecpar->height << endl;

    //const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id); //获取解码器

    AVCodecContext* avctx = avcodec_alloc_context3(codec);//分配AVCodecContext空间
    ret = avcodec_parameters_to_context(avctx, videoStream->codecpar); //拷贝解码器
    if (ret < 0) {
        cout << "拷贝解码器失败" << endl;
        return ret;
    }

    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        cout << "打开解码器失败" << endl;
        return ret;
    }
    cout << "打开解码器成功..." << endl;

    AVPacket* pkt = av_packet_alloc(); //分配包
    if (!pkt) {
        cout << "av_packet_alloc失败" << endl;
        return 1;
    }
    AVFrame* frame = av_frame_alloc(); //分配帧
    if (!frame) {
        cout << "av_frame_alloc失败" << endl;
        return 1;
    }

    FILE* pFile;
    fopen_s(&pFile, out_path, "wb+");

    int frame_count = 0;
    //int frame_count_max = 100;
    int frame_count_max = INT_MAX;
    while (true && frame_count < frame_count_max) {
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            cout << "av_read_frame失败" << endl;
            break;
        }

        if (pkt->stream_index != videoStreamIndex) {
            continue;
        }

        if (avcodec_send_packet(avctx, pkt) != 0) { //将读取的包发送到解码器上下文，进行解码
            cout << "avcodec_send_packet错误" << endl;
            break;
        }
        while (avcodec_receive_frame(avctx, frame) == 0) { //不断循环，取出avcodec_send_packet解码后的帧数据
            frame_count++;
            cout << "解码到第" << avctx->frame_number << "帧" << endl;

            int size = av_image_get_buffer_size((AVPixelFormat)frame->format, frame->width, frame->height, 1);
            uint8_t* buffer = (uint8_t*)av_malloc(size); //申请一块内存
            av_image_copy_to_buffer(buffer, size,
            	(const uint8_t* const*)frame->data,
            	(const int*)frame->linesize, (AVPixelFormat)frame->format,
            	frame->width, frame->height, 1); //将数据拷贝到buffer中

            fwrite(buffer, 1, size, pFile);//将buffer数据写入文件中去

            av_freep(&buffer);//清除buffer
        }
    }

    if (pFile)
        fclose(pFile);
    av_packet_free(&pkt);
    avcodec_close(avctx);
    avcodec_free_context(&avctx);
    av_frame_free(&frame);
    avformat_close_input(&ic);
    cout << "ffplay.exe -video_size 1920x1080 " << out_path << endl;
    return 0;
}