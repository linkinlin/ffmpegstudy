#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
//#include "opencv2/opencv.hpp"
//#include "opencv2/core/core.hpp"
//#include "opencv2/imgproc/types_c.h"


char av_error[AV_ERROR_MAX_STRING_SIZE] = { 0 };
#define av_err2str(errnum) av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)

//const char *filter_descr = "scale=78:24,transpose=cclock";
const char * filter_descr = "drawtext=fontfile=C\\:Users\\casair\\Desktop\\AlibabaPuHuiTi-2-55-Regular.ttf:text='hello world':x=10:y=10:fontsize=24:fontcolor=white:shadowy=2";
/* other way:
   scale=78:24 [scl]; [scl] transpose=cclock // assumes "[in]" and "[out]" to be input output pads respectively
 */

static int width, height;
static enum AVPixelFormat pix_fmt;
//static FILE* video_dst_file = NULL;
static uint8_t* video_dst_data[4] = { NULL };
static int      video_dst_linesize[4];
static int video_dst_bufsize;
//static int video_frame_count = 0;

static int video_frame_count = 0;

static AVFormatContext *fmt_ctx;
static AVCodecContext *dec_ctx;
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;
static int video_stream_index = -1;
static int64_t last_pts = AV_NOPTS_VALUE;
static int output_video_frame(AVFrame* frame);


static int open_input_file(const char *filename)
{
    const AVCodec *dec;
    int ret;

    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;

    /* create decoding context */
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

    /* init the video decoder */
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }

    //设置高度和宽度
    //设置格式
    /*width = dec_ctx->width;
    height = dec_ctx->height;*/
    //pix_fmt = dec_ctx->pix_fmt;
    /* allocate image where the decoded image will be put */
    width = dec_ctx->width;
    height = dec_ctx->height;
    pix_fmt = dec_ctx->pix_fmt;
    ret = av_image_alloc(video_dst_data, video_dst_linesize,
        width, height, pix_fmt, 1);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw video buffer\n");
        return -1;
    }
    video_dst_bufsize = ret;

    return 0;
}

static int init_filters(const char *filters_descr)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE };

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            time_base.num, time_base.den,
            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static void display_frame(const AVFrame *frame, AVRational time_base)
{
    int x, y;
    uint8_t *p0, *p;
    int64_t delay;

    if (frame->pts != AV_NOPTS_VALUE) {
        if (last_pts != AV_NOPTS_VALUE) {
            /* sleep roughly the right amount of time;
             * usleep is in microseconds, just like AV_TIME_BASE. */
            delay = av_rescale_q(frame->pts - last_pts, time_base, { 1, AV_TIME_BASE });
            if (delay > 0 && delay < 1000000)
                Sleep(delay);
        }
        last_pts = frame->pts;
    }

    /* Trivial ASCII grayscale display. */
    p0 = frame->data[0];
    puts("\033c");
    for (y = 0; y < frame->height; y++) {
        p = p0;
        for (x = 0; x < frame->width; x++)
            putchar(" .-+#"[*(p++) / 52]);
        putchar('\n');
        p0 += frame->linesize[0];
    }
    fflush(stdout);
}

int main(int argc, char **argv)
{
    char *filename = "C:\\Users\\casair\\Downloads\\pugongying.mp4";
    int ret;
    AVPacket *packet;
    AVFrame *frame;
    AVFrame *filt_frame;

    /*if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", filename);
        exit(1);
    }*/

    frame = av_frame_alloc();
    filt_frame = av_frame_alloc();
    packet = av_packet_alloc();
    if (!frame || !filt_frame || !packet) {
        fprintf(stderr, "Could not allocate frame or packet\n");
        exit(1);
    }

    if ((ret = open_input_file(filename)) < 0)
        goto end;
    if ((ret = init_filters(filter_descr)) < 0)
        goto end;

    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(fmt_ctx, packet)) < 0)
            break;

        if (packet->stream_index == video_stream_index) {
            ret = avcodec_send_packet(dec_ctx, packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(dec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                    goto end;
                }

                video_frame_count++;
                output_video_frame(frame);

                frame->pts = frame->best_effort_timestamp;

                /* push the decoded frame into the filtergraph */
                if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                    break;
                }

                /* pull filtered frames from the filtergraph */
                while (1) {
                    ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    if (ret < 0)
                        goto end;
                    //display_frame(filt_frame, buffersink_ctx->inputs[0]->time_base);
                    //转化成图片
                    video_frame_count++;
                    output_video_frame(frame);
                    /*char str[100];
                    sprintf(str, "C:\\Users\\casair\\Downloads\\outdir\\\\%d.png", video_frame_count);
                    char* video_dst_filename = str;
                    savePicture(frame, video_dst_filename);*/
                    av_frame_unref(filt_frame);
                   
                }
                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);
    }
end:
    avfilter_graph_free(&filter_graph);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_frame_free(&filt_frame);
    av_packet_free(&packet);

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        exit(1);
    }

    exit(0);
}

//输出视频帧到图片
static int output_video_frame(AVFrame* frame)
{
    char str[100];
    sprintf(str, "C:\\Users\\casair\\Downloads\\outdir\\\\%d.png", video_frame_count);
    char* video_dst_filename = str;

  /*  cv::Mat rgb_img;
    cv::Mat yum_img(frame->height, frame->width, CV_8UC3);
    yum_img.data = (uchar *)frame->data;
    cv::cvtColor(yum_img, rgb_img, CV_YUV420sp2RGB);
    cv::imwrite(video_dst_filename, rgb_img);*/





    //AVFormatContext* pFormatCtx = avformat_alloc_context();
    ////pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);
    //pFormatCtx->oformat = av_guess_format("png", NULL, NULL);

   /* if (video_frame_count >= 10) {
        return -1;
    }*/
    //frame->coded_picture_number
   
    //FILE*  video_dst_file = fopen(video_dst_filename, "wb+");
    //if (!video_dst_file) {
    //    fprintf(stderr, "Could not open destination file %s\n", video_dst_filename);
    //    return -1;
    //}


    //av_image_copy(video_dst_data, video_dst_linesize,
    //    (const uint8_t**)(frame->data), frame->linesize,
    //    pix_fmt, width, height);
   
    ///* write to rawvideo file */
    //fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
    //fclose(video_dst_file);
    return 0;
}

//int savePicture(AVFrame* pFrame, char* out_name) {//编码保存图片
//
//    int width = pFrame->width;
//    int height = pFrame->height;
//    AVCodecContext* pCodeCtx = NULL;
//
//
//    AVFormatContext* pFormatCtx = avformat_alloc_context();
//    // 设置输出文件格式
//    pFormatCtx->oformat = av_guess_format("mjpeg", NULL, NULL);
//
//    // 创建并初始化输出AVIOContext
//    if (avio_open(&pFormatCtx->pb, out_name, AVIO_FLAG_READ_WRITE) < 0) {
//        printf("Couldn't open output file.");
//        return -1;
//    }
//
//    // 构建一个新stream
//    AVStream* pAVStream = avformat_new_stream(pFormatCtx, 0);
//    if (pAVStream == NULL) {
//        return -1;
//    }
//
//    AVCodecParameters* parameters = pAVStream->codecpar;
//    parameters->codec_id = pFormatCtx->oformat->video_codec;
//    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
//    parameters->format = AV_PIX_FMT_YUVJ420P;
//    parameters->width = pFrame->width;
//    parameters->height = pFrame->height;
//
//    const AVCodec* pCodec = avcodec_find_encoder(pAVStream->codecpar->codec_id);
//
//    if (!pCodec) {
//        printf("Could not find encoder\n");
//        return -1;
//    }
//
//    pCodeCtx = avcodec_alloc_context3(pCodec);
//    if (!pCodeCtx) {
//        fprintf(stderr, "Could not allocate video codec context\n");
//        exit(1);
//    }
//
//    if ((avcodec_parameters_to_context(pCodeCtx, pAVStream->codecpar)) < 0) {
//        fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
//            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
//        return -1;
//    }
//
//    pCodeCtx->time_base = { 1, 25 };
//
//    if (avcodec_open2(pCodeCtx, pCodec, NULL) < 0) {
//        printf("Could not open codec.");
//        return -1;
//    }
//
//    int ret = avformat_write_header(pFormatCtx, NULL);
//    if (ret < 0) {
//        printf("write_header fail\n");
//        return -1;
//    }
//
//    int y_size = width * height;
//
//    //Encode
//    // 给AVPacket分配足够大的空间
//    AVPacket pkt;
//    av_new_packet(&pkt, y_size * 3);
//
//    // 编码数据
//    ret = avcodec_send_frame(pCodeCtx, pFrame);
//    if (ret < 0) {
//        printf("Could not avcodec_send_frame.");
//        return -1;
//    }
//
//    // 得到编码后数据
//    ret = avcodec_receive_packet(pCodeCtx, &pkt);
//    if (ret < 0) {
//        printf("Could not avcodec_receive_packet");
//        return -1;
//    }
//
//    ret = av_write_frame(pFormatCtx, &pkt);
//
//    if (ret < 0) {
//        printf("Could not av_write_frame");
//        return -1;
//    }
//
//    av_packet_unref(&pkt);
//
//    //Write Trailer
//    av_write_trailer(pFormatCtx);
//
//
//    avcodec_close(pCodeCtx);
//    avio_close(pFormatCtx->pb);
//    avformat_free_context(pFormatCtx);
//
//    return 0;
//}
