#ifndef WAV_H
#define WAV_H

#define WAV_BUFFER_SIZE 480
extern bool playing_wav;
extern int load_wav_buffer_pending;

void playWavFile(char* filename);
int get_wav_sample(void);
uint32_t load_wav_buffer(uint8_t buf_num);

#endif