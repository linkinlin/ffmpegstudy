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
    av_log_set_level(AV_LOG_INFO); //设置日志级别

    AVFormatContext* ic = nullptr;
    int ret;
    const char* in_path = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mp4"; //输入文件
    const char* out_path = "C:\\Users\\casair\\Desktop\\rtsp-out.mp4"; //输出文件
    avformat_network_init();
    //rtsp参数配置
    AVDictionary* options = NULL;
    //http://www.ffmpeg.org/ffmpeg-all.html#rtsp
    av_dict_set(&options, "rtsp_transport", "udp", 0); //设置rtsp传输协议 udp tcp
    av_dict_set(&options, "max_delay", "500", 0);//设置最大延迟(500000=0.5s)
    av_dict_set(&options, "fflags", "nobuffer", 0);//取消缓存，减少延迟
    av_dict_set(&options, "buffer_size", "1024000", 0); //设置缓存,减少花屏
    av_dict_set(&options, "timeout", "3000000", 0);//设置超时时间(3000000=3s)

    ret = avformat_open_input(&ic, in_path, NULL, &options);
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "open file[%s]fail\n", in_path);
        return ret;
    }
    av_log(ic, AV_LOG_INFO, "open file[%s]success\n", in_path);
    av_dump_format(ic, 0, in_path, 0); //打印媒体信息

    ret = avformat_find_stream_info(ic, NULL); //查询流信息
    if (ret < 0) {
        avformat_free_context(ic);
        av_log(ic, AV_LOG_ERROR, "avformat_find_stream_info fail\n");
        return ret;
    }

    const AVCodec* codec;
    ret = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0); //获取视频流索引
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "av_find_best_stream fail\n");
        return ret;
    }
    int videoStreamIndex = ret;
    AVStream* videoStream = ic->streams[ret];
    //cout << "视频编码:" << avcodec_get_name(videoStream->codecpar->codec_id) << endl; //输出视频流编码名称
    //cout << "视频尺寸:" << videoStream->codecpar->width << "x" << videoStream->codecpar->height << endl;

    //const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id); //获取解码器

    AVCodecContext* avctx = avcodec_alloc_context3(codec);//分配AVCodecContext空间
    ret = avcodec_parameters_to_context(avctx, videoStream->codecpar); //拷贝解码器
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "avcodec_parameters_to_context fail\n");
        return ret;
    }

    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        av_log(ic, AV_LOG_ERROR, "avcodec_open2 fail\n");
        return ret;
    }

    AVPacket* pkt = av_packet_alloc(); //分配包
    if (!pkt) {
        av_log(ic, AV_LOG_ERROR, "av_packet_alloc fail\n");
        return 1;
    }
    AVFrame* frame = av_frame_alloc(); //分配帧
    if (!frame) {
        av_log(ic, AV_LOG_ERROR, "av_frame_alloc fail\n");
        return 1;
    }



    const AVOutputFormat* ofmt = NULL; //输出格式
    AVFormatContext* oc = NULL; //输出上下文
    avformat_alloc_output_context2(&oc, NULL, NULL, out_path);
    if (!oc) {
        av_log(ic, AV_LOG_ERROR, "avformat_alloc_output_context2 fail\n");
        return 1;
    }

    ofmt = oc->oformat; //设置输出格式

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
    av_dump_format(oc, 0, out_path, 1); //打印媒体信息


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
        //如果包的索引超过map 或者 对应的流为-1， 跳过处理
        if (pkt->stream_index >= stream_map_size || in_out_stream_index_map[pkt->stream_index] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        inStream = ic->streams[pkt->stream_index]; //根据包获取对应输入流
        pkt->stream_index = in_out_stream_index_map[pkt->stream_index]; //重写stream_index
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