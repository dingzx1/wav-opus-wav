#include <opus_types.h>
#include  <opus.h>
#include <cstring>
#include <memory>
#include <vector>
#define DR_WAV_IMPLEMENTATION
#include<fstream>
#include<string>
#include "dr_wav.h"
#include <iostream>
using namespace std;

#define FRAME_SIZE 640
#define MAX_FRAME_SIZE (6*FRAME_SIZE)

#define MAX_CHANNELS 1
#define MAX_PACKET_SIZE (3*1276)

#pragma pack(push)
#pragma pack(1)
#pragma pack(pop)

#ifndef  nullptr
#define  nullptr NULL
#endif


struct head {
    char id1[4];
    int cs1;
    char fmt[4];
    char sc1id[4];
    int sc1s;
    short af;
    short nc;
    int sr;
    int br;
    short ba;
    short bps;
    char sc2id[4];
    int sc2s;
};
short* analyse_wav_int16(unsigned char* data, size_t n_byte, int* ch, int* sr, int* n) {
    head h;
    memcpy(&h, data, sizeof(head));
    *ch = h.nc;
    *sr = h.sr;
    *n = h.sc2s / (h.ba / h.nc);
    if (h.sc2s + 44 != n_byte) {
        printf("Error! h.sc2s + 44 = %d, n_byte = %d\n", h.sc2s + 44, n_byte);
        exit(1);
    }
    short* tmp = new short[*n];
    memcpy(tmp, data + sizeof(head), sizeof(short) * (*n));
    return tmp;
}
void write_wav_int16(char* path, short* data, int sr, int ch, int n) {
    head h;
    memcpy(h.id1, "RIFF", 4);
    h.cs1 = n * sizeof(short) + 44 - 8;
    memcpy(h.fmt, "WAVE", 4);
    memcpy(h.sc1id, "fmt ", 4);
    h.sc1s = 16;
    h.af = 1;
    h.nc = ch;
    h.sr = sr;
    h.bps = sizeof(short) * 8;
    h.ba = ch * h.bps / 8;
    h.br = sr * ch * h.bps / 8;
    memcpy(h.sc2id, "data", 4);
    h.sc2s = n * sizeof(short);
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        printf("Can not open %s\n", path);
        exit(1);
    }
    fwrite(&h, sizeof(head), 1, fp);
    fwrite(data, sizeof(short), n, fp);
    fclose(fp);
}
void write_wav_int16_stdout(short* data, int sr, int ch, int n) {
    head h;
    memcpy(h.id1, "RIFF", 4);
    h.cs1 = n * sizeof(short) + 44 - 8;
    memcpy(h.fmt, "WAVE", 4);
    memcpy(h.sc1id, "fmt ", 4);
    h.sc1s = 16;
    h.af = 1;
    h.nc = ch;
    h.sr = sr;
    h.bps = sizeof(short) * 8;
    h.ba = ch * h.bps / 8;
    h.br = sr * ch * h.bps / 8;
    memcpy(h.sc2id, "data", 4);
    h.sc2s = n * sizeof(short);
    fwrite(&h, sizeof(head), 1, stdout);
    fwrite(data + sizeof(head), sizeof(short), n, stdout);
}

int Wav2Opus(short* data_wav, int n_sample, int complexity,int bitrate, char* data_enc) {
    int sampleRate = 16000;
    int channels = 1;
    int err = 0;
    int n_sample_per_frame=FRAME_SIZE;
    int n_byte_encode;
    OpusEncoder *encoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_AUDIO, &err);
    if (!encoder || err < 0) {
        fprintf(stderr, "failed to create an encoder: %s\n", opus_strerror(err));
        if (!encoder) {
            opus_encoder_destroy(encoder);
        }
        return false;
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitrate));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(complexity));
    unsigned char cbits[MAX_PACKET_SIZE];
    short* in = new short[n_sample_per_frame];
    int i_wav = 0;
    int i_enc = 0;
    int n_block = n_sample / n_sample_per_frame;
    for (int i = 0; i < n_block; i++) {
        i_wav = i * n_sample_per_frame;
        memcpy(in, data_wav + i_wav, sizeof(short) * n_sample_per_frame);
        n_byte_encode = opus_encode(encoder, in, channels * FRAME_SIZE, cbits, MAX_PACKET_SIZE);
        memcpy(data_enc+i_enc,&n_byte_encode,sizeof(int));
        i_enc = i_enc + sizeof(int); 
        memcpy(data_enc + i_enc, cbits, sizeof(char) * n_byte_encode);
        i_enc += n_byte_encode;
       
    }
    delete[] in;
    opus_encoder_destroy(encoder);
    return i_enc;

    
}
int Opus2Wav(char* data_enc,int n_enc,short* data_dec)
{
    int i_enc = 0;
    int i_dec = 0;
    uint32_t sampleRate = 16000;
    uint16_t channels = 1;
    int err = 0;
    OpusDecoder* decoder = opus_decoder_create(sampleRate, channels, &err);
    if (!decoder || err < 0) {
        fprintf(stderr, "failed to create decoder: %s\n", opus_strerror(err));
        if (!decoder) {
            opus_decoder_destroy(decoder);
        }
        return false;
    }
    int nbBytes;
    short* buffer_decode = new short[MAX_FRAME_SIZE];
    unsigned char cbits[MAX_PACKET_SIZE];
    while (i_enc< n_enc) 
    {
        memset(&cbits, 0, MAX_PACKET_SIZE);
        memcpy(&nbBytes, data_enc+i_enc, sizeof(int));
        i_enc += sizeof(int);
        memcpy(cbits, data_enc + i_enc, nbBytes);
        i_enc+=nbBytes;
        int frame_size = opus_decode(decoder, cbits, nbBytes, buffer_decode, MAX_FRAME_SIZE, 0);
        if (frame_size < 0) 
        {
            fprintf(stderr, "decode failed: %s\n", opus_strerror(frame_size));
            break;
        }
        memcpy( data_dec+i_dec, buffer_decode, sizeof(short)*frame_size);
        i_dec += frame_size;
    }     
    opus_decoder_destroy(decoder);
    delete[] buffer_decode;
    return i_dec; 
}
int read_from_stdin(unsigned char** data){
        size_t len_buffer = 1000;
        if ((*data) == NULL) {
            (*data) = new unsigned char[len_buffer];
        }
        unsigned char* tmp0 = new unsigned char[1000];
        int n_byte_read = 0;
        int n_byte_read_all = 0;
        while (1) {
            
            n_byte_read = fread(tmp0, 1, 1000, stdin);
            if (n_byte_read_all + n_byte_read > len_buffer) {
                size_t len_buffer_old = len_buffer;
                while (n_byte_read_all + n_byte_read > len_buffer) {
                    len_buffer *= 2;
                }
                unsigned char* tmp = new unsigned char[len_buffer];
                memcpy(tmp, *data, len_buffer_old);
                delete[](*data);
                *data = tmp;
                tmp = NULL;
            }
            if (n_byte_read > 0) {
                memcpy((*data) + n_byte_read_all, tmp0, n_byte_read);
            }
            n_byte_read_all+= n_byte_read;
            if (feof(stdin)) {
                break;
            }
        }
        delete[] tmp0;
        return n_byte_read_all;
    }
void encode_decode(int complexity,int bitrate) {
    int n_channel, sample_rate, n_sample;
    unsigned char* data_stdin = NULL;
    size_t n_byte = read_from_stdin(&data_stdin); 
    short* data_wav = analyse_wav_int16(data_stdin, n_byte, &n_channel,
        &sample_rate, &n_sample);    
    delete[] data_stdin;
    char* data_enc = new char[n_sample * sizeof(short)];
    short* data_dec = new short[n_sample];
    int n_enc = Wav2Opus(data_wav, n_sample, complexity,bitrate,data_enc);
    int n_dec = Opus2Wav(data_enc,n_enc,data_dec);    
    write_wav_int16_stdout(data_dec, sample_rate, n_channel, n_dec);
    delete[] data_wav;
    delete[] data_enc;
    delete[] data_dec;
}
int main(int argc, char **argv) { 
   
  if (argc != 3) {
    exit(1);
  }
  int complexity = atoi(argv[1]);
  int bitrate = atoi(argv[2]);
  encode_decode(complexity,bitrate);
  return 0;
}

