// Referencia: el cargador de EightyOne con sus opciones por defecto. Como en el
// SD81, solo arranca si hay "#!basic-start=N" (desde la linea N o la siguiente)
#include "zx81/zx81BasicLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>
#include <cstdint>
static void PatchZX81AutoRun(unsigned char* data, int dataLen, int startLine)
{
    const int startOfRam = 16393, PROG_OFFSET = 116;
    (void)dataLen;
    int progEnd = (data[3] | data[4] << 8) - startOfRam;   // D_FILE
    int offset = PROG_OFFSET;
    while (offset < progEnd) {
        int lineNum = (data[offset] << 8) | data[offset + 1];
        if (lineNum >= startLine) { uint16_t n = startOfRam + offset; data[32] = n & 0xFF; data[33] = n >> 8; return; }
        int lineSize = data[offset + 2] | (data[offset + 3] << 8);
        offset += 4 + lineSize;
    }
}
static int ExtractBasicStartLine(const std::string& text)
{
    std::istringstream scan(text); std::string line;
    while (std::getline(scan, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.substr(0, 14) == "#!basic-start=") { try { return std::stoi(line.substr(14)); } catch (...) {} }
    }
    return -2;
}
int main(int argc, char** argv)
{
    std::ifstream ifs(argv[1], std::ios::binary); std::ostringstream ss; ss << ifs.rdbuf(); std::string text = ss.str();
    int start = ExtractBasicStartLine(text);
    auto loader = std::make_unique<zx81BasicLoader>(false);
    loader->LoadBasicFromString(text, argv[1], false, false, false, false, false, 10);
    if (loader->ProgramLength() == 0) { std::cout << "ERROR: " << loader->ErrorMsg() << "\n"; return 1; }
    if (start >= 0) PatchZX81AutoRun(loader->ProgramData(), loader->ProgramLength(), start);
    std::ofstream o(argv[2], std::ios::binary); o.write((char*)loader->ProgramData(), loader->ProgramLength());
    return 0;
}
