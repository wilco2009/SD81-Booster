#pragma once
extern bool debug_monitor_loaded;
bool snapfile_open(const char* path);
bool snapfile_write(const void* data, uint16_t n);
void snapfile_close(void);
bool snapfile_exists(const char* path);
int32_t rom_file_read(uint32_t offset, uint8_t* buf, uint16_t n);
bool z81in_open(const char* path);
int z81in_read(void);
bool z81in_seek(uint32_t pos);
uint32_t z81in_pos(void);
void z81in_close(void);
