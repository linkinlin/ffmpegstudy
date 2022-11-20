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
    const char* in_path = "C:\\Users\\casair\\Downloads\\ke.mp4"; //�����ļ�
    const char* out_path = "C:\\Users\\casair\\Desktop\\rjm-out.yuv"; //����ļ�
    int ret;

    ret = avformat_open_input(&ic, in_path, NULL, NULL);
    if (ret < 0) {
        cout << "���ļ�ʧ��" << endl;
        return ret;
    }
    cout << "���ļ��ɹ�: " << in_path << endl;
    //av_dump_format(ic, 0, inputFile, 0); //��ӡý����Ϣ

    ret = avformat_find_stream_info(ic, NULL); //��ѯ����Ϣ
    if (ret < 0) {
        avformat_free_context(ic);
        cout << "��ѯ����Ϣʧ��" << endl;
        return ret;
    }
    cout << "��ѯ����Ϣ�ɹ�...." << endl;

    const AVCodec* codec;
    ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0); //��ȡ��Ƶ������
    if (ret < 0) {
        cout << "��ȡ��Ƶ������ʧ��" << endl;
        return ret;
    }
    cout << "��ȡ��Ƶ�������ɹ�:" << ret << endl;
    int videoStreamIndex = ret;
    AVStream* videoStream = ic->streams[ret];
    cout << "��Ƶ����:" << avcodec_get_name(videoStream->codecpar->codec_id) << endl; //�����Ƶ����������
    cout << "��Ƶ�ߴ�:" << videoStream->codecpar->width << "x" << videoStream->codecpar->height << endl;

    //const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id); //��ȡ������

    AVCodecContext* avctx = avcodec_alloc_context3(codec);//����AVCodecContext�ռ�
    ret = avcodec_parameters_to_context(avctx, videoStream->codecpar); //����������
    if (ret < 0) {
        cout << "����������ʧ��" << endl;
        return ret;
    }

    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        cout << "�򿪽�����ʧ��" << endl;
        return ret;
    }
    cout << "�򿪽������ɹ�..." << endl;

    AVPacket* pkt = av_packet_alloc(); //�����
    if (!pkt) {
        cout << "av_packet_allocʧ��" << endl;
        return 1;
    }
    AVFrame* frame = av_frame_alloc(); //����֡
    if (!frame) {
        cout << "av_frame_allocʧ��" << endl;
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
            cout << "av_read_frameʧ��" << endl;
            break;
        }

        if (pkt->stream_index != videoStreamIndex) {
            continue;
        }

        if (avcodec_send_packet(avctx, pkt) != 0) { //����ȡ�İ����͵������������ģ����н���
            cout << "avcodec_send_packet����" << endl;
            break;
        }
        while (avcodec_receive_frame(avctx, frame) == 0) { //����ѭ����ȡ��avcodec_send_packet������֡����
            frame_count++;
            cout << "���뵽��" << avctx->frame_number << "֡" << endl;

            int size = av_image_get_buffer_size((AVPixelFormat)frame->format, frame->width, frame->height, 1);
            uint8_t* buffer = (uint8_t*)av_malloc(size); //����һ���ڴ�
            av_image_copy_to_buffer(buffer, size,
            	(const uint8_t* const*)frame->data,
            	(const int*)frame->linesize, (AVPixelFormat)frame->format,
            	frame->width, frame->height, 1); //�����ݿ�����buffer��

            fwrite(buffer, 1, size, pFile);//��buffer����д���ļ���ȥ

            av_freep(&buffer);//���buffer
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