//
//  main.cpp
//  ExtractAACFormat
//
//  Created by Ych-caihui on 2022/8/7.
//

#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavformat/avio.h>
}

#define ADTS_HEADER_LEN  7;

const int sampling_frequencies[] = {
    96000,  // 0x0
    88200,  // 0x1
    64000,  // 0x2
    48000,  // 0x3
    44100,  // 0x4
    32000,  // 0x5
    24000,  // 0x6
    22050,  // 0x7
    16000,  // 0x8
    12000,  // 0x9
    11025,  // 0xa
    8000   // 0xb
    // 0xc d e f是保留的
};

int adts_header(char * const p_adts_header, const int data_length,
                const int profile, const int samplerate,
                const int channels)
{
    
    int sampling_frequency_index = 3; // 默认使用48000hz
    int adtsLen = data_length + 7;
    
    int frequencies_size = sizeof(sampling_frequencies) / sizeof(sampling_frequencies[0]);
    int i = 0;
    for(i = 0; i < frequencies_size; i++)
    {
        if(sampling_frequencies[i] == samplerate)
        {
            sampling_frequency_index = i;
            break;
        }
    }
    if(i >= frequencies_size)
    {
        printf("unsupport samplerate:%d\n", samplerate);
        return -1;
    }
    
    p_adts_header[0] = 0xff;         //syncword:0xfff                          高8bits
    p_adts_header[1] = 0xf0;         //syncword:0xfff                          低4bits
    p_adts_header[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
    p_adts_header[1] |= (0 << 1);    //Layer:0                                 2bits
    p_adts_header[1] |= 1;           //protection absent:1                     1bit
    
    p_adts_header[2] = (profile)<<6;            //profile:profile               2bits
    p_adts_header[2] |= (sampling_frequency_index & 0x0f)<<2; //sampling frequency index:sampling_frequency_index  4bits
    p_adts_header[2] |= (0 << 1);             //private bit:0                   1bit
    p_adts_header[2] |= (channels & 0x04)>>2; //channel configuration:channels  高1bit
    
    p_adts_header[3] = (channels & 0x03)<<6; //channel configuration:channels 低2bits
    p_adts_header[3] |= (0 << 5);               //original：0                1bit
    p_adts_header[3] |= (0 << 4);               //home：0                    1bit
    p_adts_header[3] |= (0 << 3);               //copyright id bit：0        1bit
    p_adts_header[3] |= (0 << 2);               //copyright id start：0      1bit
    p_adts_header[3] |= ((adtsLen & 0x1800) >> 11);           //frame length：value   高2bits
    
    p_adts_header[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
    p_adts_header[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
    p_adts_header[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits
    p_adts_header[6] = 0xfc;      //‭11111100‬       //buffer fullness:0x7ff 低6bits
    // number_of_raw_data_blocks_in_frame：
    //    表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧。
    
    return 0;
}

int main(int argc, const char * argv[]) {
    
    int ret = -1;
    char error[1024];
    const char *in_fileName = "/Users/ych-caihui/Movies/wawa.mp4";
    const char *aac_fileName = "/Users/ych-caihui/Movies/wawa.aac";
    
    FILE * aac_fd = nullptr;
    int audio_index = -1;
    int len = 0;
    
    AVFormatContext * ifmt_ctx = nullptr;
    AVPacket pkt;
    
    //设置打印级别
    av_log_set_level(AV_LOG_DEBUG);
    
    aac_fd = fopen(aac_fileName, "wb");
    if (!aac_fd){
        av_log(NULL, AV_LOG_DEBUG, "Could not open destination file %s\n", aac_fileName);
        return -1;
    }
    
    //打开输入文件
    if((ret = avformat_open_input(&ifmt_ctx, in_fileName, nullptr, nullptr)) < 0) {
        av_strerror(ret, error, 1024);
        av_log(nullptr, AV_LOG_ERROR, "Could not open source file: %s, %d(%s)\n", in_fileName, ret, error);
        return -1;
    }
    
    
    //获取解码信息
    if((ret = avformat_find_stream_info(ifmt_ctx, nullptr)) < 0) {
        av_strerror(ret, error, 1024);
        av_log(nullptr, AV_LOG_ERROR, "failed to find stream information: %s %d(%s)", in_fileName, ret, error);
        return -1;
    }
    
    //dump媒体信息
    av_dump_format(ifmt_ctx, 0, in_fileName, 0);
    
    //初始化packet
    av_init_packet(&pkt);
    
    audio_index = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    
    
    //打印aac级别
    std::cout << "audio profile:" << ifmt_ctx->streams[audio_index]->codecpar->profile << std::endl;
    
    if(ifmt_ctx->streams[audio_index]->codecpar->codec_id != AV_CODEC_ID_AAC) {
        std::cout << "the media file no caontain AAC stream, it's codec_id_is " << ifmt_ctx->streams[audio_index]->codecpar->codec_id << std::endl;
        return -1;
    }
    
    //读取媒体文件，并把aac数据写入到本地文件
    while (av_read_frame(ifmt_ctx, &pkt) >= 0 ) {
        if(pkt.stream_index == audio_index) {
            char adts_header_buf[7] = {0};
            adts_header(adts_header_buf, pkt.size,
                        ifmt_ctx->streams[audio_index]->codecpar->profile,
                        ifmt_ctx->streams[audio_index]->codecpar->sample_rate,
                        ifmt_ctx->streams[audio_index]->codecpar->channels);
            fwrite(adts_header_buf, 1, 7, aac_fd);  // 写adts header , ts流不适用，ts流分离出来的packet带了adts header
            len = fwrite( pkt.data, 1, pkt.size, aac_fd);   // 写adts data
            if(len != pkt.size) {
                av_log(NULL, AV_LOG_DEBUG, "warning, length of writed data isn't equal pkt.size(%d, %d)\n",
                       len,
                       pkt.size);
            }
        }
        av_packet_unref(&pkt);
    }
    
    if(ifmt_ctx) {
      avformat_close_input(&ifmt_ctx);
    }
    if(aac_fd) {
      fclose(aac_fd);
    }
    
    return 0;
}
