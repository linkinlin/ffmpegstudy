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
    av_log_set_level(AV_LOG_INFO); //������־����

    AVFormatContext* ic = nullptr;
    int ret;
    const char* in_path = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4"; //�����ļ�
    const char* out_path = "C:\\Users\\casair\\Desktop\\rtsp-out.mp4"; //����ļ�
    avformat_network_init();
    //rtsp��������
    AVDictionary* options = NULL;
    //http://www.ffmpeg.org/ffmpeg-all.html#rtsp
    av_dict_set(&options, "rtsp_transport", "udp", 0); //����rtsp����Э�� udp tcp
    av_dict_set(&options, "max_delay", "500", 0);//��������ӳ�(500000=0.5s)
    av_dict_set(&options, "fflags", "nobuffer", 0);//ȡ�����棬�����ӳ�
    av_dict_set(&options, "buffer_size", "1024000", 0); //���û���,���ٻ���
    av_dict_set(&options, "timeout", "3000000", 0);//���ó�ʱʱ��(3000000=3s)

    ret = avformat_open_input(&ic, in_path, NULL, &options);
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "open file[%s]fail\n", in_path);
        return ret;
    }
    av_log(ic, AV_LOG_INFO, "open file[%s]success\n", in_path);
    av_dump_format(ic, 0, in_path, 0); //��ӡý����Ϣ

    ret = avformat_find_stream_info(ic, NULL); //��ѯ����Ϣ
    if (ret < 0) {
        avformat_free_context(ic);
        av_log(ic, AV_LOG_ERROR, "avformat_find_stream_info fail\n");
        return ret;
    }

    const AVCodec* codec;
    ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0); //��ȡ��Ƶ������
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "av_find_best_stream fail\n");
        return ret;
    }
    int videoStreamIndex = ret;
    AVStream* videoStream = ic->streams[ret];
    //cout << "��Ƶ����:" << avcodec_get_name(videoStream->codecpar->codec_id) << endl; //�����Ƶ����������
    //cout << "��Ƶ�ߴ�:" << videoStream->codecpar->width << "x" << videoStream->codecpar->height << endl;

    //const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id); //��ȡ������

    AVCodecContext* avctx = avcodec_alloc_context3(codec);//����AVCodecContext�ռ�
    ret = avcodec_parameters_to_context(avctx, videoStream->codecpar); //����������
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "avcodec_parameters_to_context fail\n");
        return ret;
    }

    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "avcodec_open2 fail\n");
        return ret;
    }

    AVPacket* pkt = av_packet_alloc(); //�����
    if (!pkt) {
        av_log(ic, AV_LOG_ERROR, "av_packet_alloc fail\n");
        return 1;
    }
    AVFrame* frame = av_frame_alloc(); //����֡
    if (!frame) {
        av_log(ic, AV_LOG_ERROR, "av_frame_alloc fail\n");
        return 1;
    }



    const AVOutputFormat* ofmt = NULL; //�����ʽ
    AVFormatContext* oc = NULL; //���������
    avformat_alloc_output_context2(&oc, NULL, NULL, out_path);
    if (!oc) {
        av_log(ic, AV_LOG_ERROR, "avformat_alloc_output_context2 fail\n");
        return 1;
    }

    ofmt = oc->oformat; //���������ʽ

    int out_stream_index = 0;
    int stream_map_size = ic->nb_streams;
    int* in_out_stream_index_map = NULL;
    in_out_stream_index_map = (int*)av_mallocz_array(stream_map_size, sizeof(*in_out_stream_index_map));

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVStream* outStream = NULL;
        AVStream* inStream = ic->streams[i];
        AVCodecParameters* inStreamCodecpar = inStream->codecpar;
        if (inStreamCodecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            inStreamCodecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            inStreamCodecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            in_out_stream_index_map[i] = -1;
            continue;
        }

        in_out_stream_index_map[i] = out_stream_index++;
        outStream = avformat_new_stream(oc, NULL);
        if (!outStream) {
            av_log(ic, AV_LOG_ERROR, "avformat_new_stream fail\n");
            continue;
        }

        ret = avcodec_parameters_copy(outStream->codecpar, inStreamCodecpar);
        if (ret < 0) {
            av_log(ic, AV_LOG_ERROR, "avcodec_parameters_copy fail\n");
            continue;
        }

        outStream->codecpar->codec_tag = 0;
    }
    av_dump_format(oc, 0, out_path, 1); //��ӡý����Ϣ


    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open2(&oc->pb, out_path, AVIO_FLAG_WRITE, NULL, NULL);
        if (ret < 0) {
            av_log(ic, AV_LOG_ERROR, "avcodec_parameters_copy fail\n");
            return 1;
        }
    }

    ret = avformat_write_header(oc, NULL);
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "avformat_write_header fail\n");
        return 1;
    }
    int dts = 0;
    int dts_last = 0;
    while (true) {
        AVStream* inStream, * outStream;
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            av_log(ic, AV_LOG_ERROR, "av_read_frame fail\n");
            break;
        }
        //���������������map ���� ��Ӧ����Ϊ-1�� ��������
        if (pkt->stream_index >= stream_map_size || in_out_stream_index_map[pkt->stream_index] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        inStream = ic->streams[pkt->stream_index]; //���ݰ���ȡ��Ӧ������
        pkt->stream_index = in_out_stream_index_map[pkt->stream_index]; //��дstream_index
        outStream = oc->streams[pkt->stream_index];

        //pkt->pts = av_rescale_q_rnd(pkt->pts, inStream->time_base, outStream->time_base, AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        //pkt->dts = av_rescale_q_rnd(pkt->dts, inStream->time_base, outStream->time_base, AVRounding(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
       // pkt->duration = av_rescale_q(pkt->duration, inStream->time_base, outStream->time_base);
        
        //dts = pkt->dts;
        //if (dts < dts_last) {
          //  continue;
        //}

       // dts_last = dts;

        ret = av_interleaved_write_frame(oc, pkt);
        if (ret < 0) {
            av_log(ic, AV_LOG_ERROR, "av_interleaved_write_frame fail\n");
            continue;
        }
        av_packet_unref(pkt);
    }

   
    av_packet_free(&pkt);
    avcodec_close(avctx);
    avcodec_free_context(&avctx);
    av_frame_free(&frame);
    avformat_close_input(&ic);
    //cout << "ffplay.exe -video_size 1920x1080 " << out_path << endl;
    return 0;
}