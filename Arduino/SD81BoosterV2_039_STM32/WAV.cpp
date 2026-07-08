#include <stdint.h>
#include <SdFat.h>
#include "SD_handle.h"
#include "GLOBALS.h"
#include "dac_control.h"
#include "WAV.h"
#include <inttypes.h>

// --- Variables para el remuestreo (nuevas) ---
// Usamos un tipo de 64 bits para el acumulador para evitar desbordamientos
// N_BITS_FRAC define cuántos bits se usan para la parte fraccional del acumulador.
// Un valor de 16-24 bits suele ser suficiente para buena precisión.
#define N_BITS_FRAC 16
#define ACCUMULATOR_SCALE (1LL << N_BITS_FRAC) // 2^16

uint64_t wav_position_accumulator = 0; // Acumulador de posición en el WAV (formato Qx.N_BITS_FRAC)
uint64_t step_size = 0;                // Paso del acumulador por cada muestra del DAC
int load_wav_buffer_pending = -1;          // nothing pending


typedef struct {
    char        riff_header[4];    // "RIFF"
    uint32_t    wav_size;          // Tamaño del archivo - 8
    char        wave_header[4];    // "WAVE"
    char        fmt_header[4];     // "fmt "
    uint32_t    fmt_chunk_size;    // Tamaño del chunk 'fmt' (normalmente 16)
    uint16_t    audio_format;      // Formato de audio (1 para PCM)
    uint16_t    num_channels;      // Número de canales
    uint32_t    sample_rate;       // Frecuencia de muestreo 
    uint32_t    byte_rate;         // (Sample Rate * Num Channels * Bits Per Sample) / 8
    uint16_t    block_align;       // (Num Channels * Bits Per Sample) / 8
    uint16_t    bits_per_sample;   // Bits por muestra
    char        data_header[4];    // "data"
    uint32_t    data_size;         // Tamaño del chunk 'data'
} WavHeader;

WavHeader header;
uint32_t wav_size = 0;           // Total wav file size
SdFile wav_file;                // wav file
uint8_t current_buffer = 0;      // 0 or 1
int data_len[2] = {0,0};       // data size in each buffer
uint32_t buffer_pos = 0;        // data pos in current buffer
uint32_t file_pos = 0;          // next byte to be readed in the file 
uint8_t WAV_buffer[2][WAV_BUFFER_SIZE];
bool playing_wav = false;
uint64_t current_wav_index = 0;


uint32_t load_wav_buffer(uint8_t buf_num){
  wav_file.seekSet(file_pos);
  data_len[buf_num] = wav_file.read((void*) WAV_buffer[current_buffer], WAV_BUFFER_SIZE);
  log_3("loading block %d, datalen=(%d,%d) \n\r",buf_num,data_len[0],data_len[1]);
  file_pos+=data_len[buf_num];
  if (data_len[buf_num]<0) {
    wav_file.close();
    log_3("Last block loaded. \n\r");
  }
  load_wav_buffer_pending=-1;
  // for (int i=0;i<WAV_BUFFER_SIZE;i++){
  //   int valor = WAV_buffer[current_buffer][i];
  //   if (valor < 0x10) {
  //     Serial.print("0");
  //   }
  //   Serial.print(valor,HEX); Serial.print(" ");
  //   if (i%32==31) Serial.println();
  //}
  return data_len[buf_num];
}

void playWavFile(char* filename){
  if (playing_wav) wav_file.close();
  wav_file.open(filename);
  if (!sd.exists(filename)) log_0("file %s does not exists.", filename);
  if (!wav_file.isOpen()) log_0("Error opening %s.", filename);
  wav_size = wav_file.fileSize();
  log_0("wav size=%u",wav_size);
  int result = wav_file.read(&header,sizeof(header));
  if ((result < sizeof(header)  /*Bad header*/) || 
      (header.audio_format !=1)        /*PCM*/) {
      log_0("Bad header format.\n\r");
      return;            // bad wav file
  }
  #ifdef LOG0
    Serial.print("Chunk size: "); Serial.println(header.fmt_chunk_size);    // Tamaño del chunk 'fmt' (normalmente 16)
    Serial.print("Audio format: ("); Serial.print(header.audio_format);Serial.print(")");
    switch (header.audio_format) {
      case 1: 
        Serial.println("PCM");
        break;
      default:
        Serial.println("Other");
    } 
    Serial.print("Num Channels: "); Serial.println(header.num_channels);    // Número de canales
    Serial.print("Sample rate: "); Serial.println(header.sample_rate);       // Frecuencia de muestreo 
    Serial.print("Byte rate: "); Serial.println(header.byte_rate);         // (Sample Rate * Num Channels * Bits Per Sample) / 8
    Serial.print("Block align: "); Serial.println(header.block_align);       // (Num Channels * Bits Per Sample) / 8
    Serial.print("Bits per sample: "); Serial.println(header.bits_per_sample);   // Bits por muestra
  #endif
  file_pos = sizeof(header); // skip header
  playing_wav = true;
  load_wav_buffer(0);
  if (file_pos < wav_size)
    load_wav_buffer(1);
  load_wav_buffer_pending = -1;

  current_buffer = 0;
  current_wav_index = 0;

  // Calcula el paso: (Frecuencia_WAV / Frecuencia_DAC) * ACCUMULATOR_SCALE
  // Usamos aritmética de punto fijo para mantener la precisión
  step_size = ( (uint64_t)header.sample_rate * ACCUMULATOR_SCALE ) / SAMPLE_RATE;

  wav_position_accumulator = 0; // Reiniciar acumulador
  log_0("Playing wav %s", filename);
}

void switch_buffer(void){
  current_buffer = (current_buffer+1) & 1;
  buffer_pos = 0;
}

int convert_sample(){
    int32_t left_channel_sample = 0;
    int32_t right_channel_sample = 0; // Solo se usa si el audio es estéreo
    int bytes_per_sample_channel = header.bits_per_sample / 8;
    // Calcular cuántos bytes ocupa un "frame" de audio (una muestra de todos los canales para un instante)
    int bytes_per_audio_frame = bytes_per_sample_channel * header.num_channels;

    #ifdef LOG3
    if ((buffer_pos==0)){
      for (int i=0;i<WAV_BUFFER_SIZE;i++){
        int valor = WAV_buffer[current_buffer][i];
        if (valor < 0x10) {
          Serial.print("0");
        }
        Serial.print(valor,HEX); Serial.print(" ");
        if (i%32==31) Serial.println();
      }
    
      Serial.print("RAW: ");Serial.print(WAV_buffer[current_buffer][buffer_pos]); 
      Serial.print(" CB="); Serial.print(current_buffer);
      Serial.print(" BP="); Serial.print(buffer_pos);
      Serial.print(" BPSC="); Serial.print(bytes_per_sample_channel);
      Serial.print(" BPAF="); Serial.print(bytes_per_audio_frame);
      Serial.print(" BITSPS="); Serial.print(header.bits_per_sample);
      Serial.print(" NUMCH="); Serial.println(header.num_channels);

    }
    #endif
      // Puntero a la ubicación actual en el búfer de datos crudos
    uint8_t *current_data_ptr = &WAV_buffer[current_buffer][buffer_pos];
    // -- Lectura del canal izquierdo (o el único canal si es mono) --
    if (bytes_per_sample_channel == 1) { // 8-bit PCM (usualmente sin signo, de 0 a 255)
        left_channel_sample = (int32_t)(*current_data_ptr); 
        current_data_ptr++;
    } else if (bytes_per_sample_channel == 2) { // 16-bit PCM (firmado, Little-Endian)
        left_channel_sample = (int32_t)(*(int16_t*)current_data_ptr)+16384;
        // escalar a 8 bits y centrar en 128
        left_channel_sample = (left_channel_sample >> 8);
        current_data_ptr += 2;
    } else if (bytes_per_sample_channel == 3) { // 24-bit PCM (firmado, Little-Endian)
        // Los 3 bytes son LSB, Medio, MSB. Necesitamos extender el signo.
        left_channel_sample = (int32_t)current_data_ptr[0] |
                              ((int32_t)current_data_ptr[1] << 8) |
                              ((int32_t)current_data_ptr[2] << 16);
        // Extensión de signo: Si el bit 23 es 1, llenar los bits superiores con 1s.
        if (left_channel_sample & 0x00800000) { // Bit 23 (el bit más significativo de 24 bits)
            left_channel_sample |= 0xFF000000; // Rellenar con 1s los bits 24-31
        }
        // Escalar a 8 bits y centrar en 128
        left_channel_sample = (left_channel_sample+16777216) >> 16;
        current_data_ptr += 3;
    } else if (bytes_per_sample_channel == 4) { // 32-bit PCM (firmado, Little-Endian)
        // Escalar a 8 bits y centrar en 128
        left_channel_sample = (*(int32_t*)current_data_ptr)>> 24 + 128;
        current_data_ptr += 4;
    }

    // -- Si es estéreo, leer el canal derecho y promediar --
    if (header.num_channels == 2) {
        if (bytes_per_sample_channel == 1) {
            right_channel_sample = (int32_t)(*current_data_ptr);
        } else if (bytes_per_sample_channel == 2) {
            right_channel_sample = (int32_t)(*(int16_t*)current_data_ptr)+16384;
            right_channel_sample = right_channel_sample >> 8;
        } else if (bytes_per_sample_channel == 3) {
            right_channel_sample = (int32_t)current_data_ptr[0] |
                                   ((int32_t)current_data_ptr[1] << 8) |
                                   ((int32_t)current_data_ptr[2] << 16);
            right_channel_sample = (right_channel_sample+16777216) >> 16;
        } else if (bytes_per_sample_channel == 4) {
            right_channel_sample = (*(int32_t*)current_data_ptr)>> 24 + 128;
        }
        current_data_ptr += bytes_per_sample_channel; // Avanzar el puntero

        // Promediar los dos canales para obtener una muestra mono
        left_channel_sample = (left_channel_sample  + right_channel_sample) / 2;
    }

    int8_t final_8bit_sample = (int8_t)(left_channel_sample);
    #ifdef LOG3
    if ((buffer_pos==0)){
      Serial.print("left="); Serial.print(left_channel_sample);
      Serial.print(" right="); Serial.print(right_channel_sample);
      Serial.print(" final="); Serial.println(final_8bit_sample);
    }
    #endif
    return (int)final_8bit_sample; // Retornar como int, pero es un valor de 8 bits.
}

int get_next_sample(void){
  int sample = 0; // Usamos int32_t para manejar 16/24 bits con holgura
  int bytes_per_sample = header.bits_per_sample / 8;
  int bytes_per_frame = bytes_per_sample * header.num_channels; // Bytes por grupo de muestras (mono/estéreo)
  if (buffer_pos+bytes_per_frame-1 < data_len[current_buffer]) {
    sample = convert_sample();
    buffer_pos+=bytes_per_frame;
  } else if (file_pos < wav_size) {
    log_3("loading new wav block (%u)\n\r",file_pos);
    load_wav_buffer_pending = current_buffer;
    //load_wav_buffer(current_buffer);
    switch_buffer();
    sample = convert_sample();
    buffer_pos+=bytes_per_frame;
  } else {
    sample = -1;
    playing_wav = false;
    log_0("Wav file finshed. \n\r");
  }
  return sample;
}

int get_wav_sample(void){
    static uint8_t sample;
    if (!playing_wav) return -1;
    // A cada SAMPLE_RATE/header.sample_rate muestras del DAC, avanzamos una muestra del wav.
    // Almacenar el índice WAV anterior
    uint64_t prev_wav_index  = current_wav_index;

    // 1. Avanzar el acumulador
    wav_position_accumulator += step_size;

    // 2. Calcular el nuevo índice de la muestra WAV actual
    current_wav_index = wav_position_accumulator >> N_BITS_FRAC;
     if (current_wav_index > prev_wav_index) {
      sample = get_next_sample();
    }
    return (int) sample;
}
