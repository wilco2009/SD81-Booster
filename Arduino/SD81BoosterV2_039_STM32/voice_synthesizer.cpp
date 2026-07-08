#include "voice_synthesizer.h"
#include "allophones.h" // Contiene los datos de los alófonos
#include <string.h>
#include <stdlib.h> // Para strtol (hexadecimal to long)
#include <Arduino.h> // Para Serial.println, si se usa para depuración
#include "dac_control.h"


static bool mode_alof = false;
// Estructura para almacenar nuestro mapeo
typedef struct {
    const char* secuencia;   // La cadena de texto de entrada (ej., "A", "...", "/PA1/")
    uint8_t longitud;        // Longitud de la secuencia
    const uint8_t* codigoHex; // CAMBIADO: Ahora es un puntero a una secuencia de uint8_t
} t_alofono;

const uint8_t* numero[] = {
    (const uint8_t[]){al_ZZ,al_EH,al_XR,al_OW,0xff},                                        // zero
    (const uint8_t[]){al_WW,al_WW,al_AX,al_AX,al_NN1,al_PA2,al_PA3,al_PA3,0xff},         // one
    (const uint8_t[]){al_TT2,al_UW2,0xff},                                                      // two
    (const uint8_t[]){al_TH,al_RR2,al_IY,0xff},                                                 // three
    (const uint8_t[]){al_FF,al_OR,0xff},                                                        // four
    (const uint8_t[]){al_FF,al_AY,al_VV,0xff},                                                  // five
    (const uint8_t[]){al_SS,al_SS,al_IH,al_PA3,al_KK2,al_SS,0xff},                              // six
    (const uint8_t[]){al_SS,al_SS,al_EH,al_VV,al_EH,al_NN1,0xff},                               // seven
    (const uint8_t[]){al_EY,al_PA3,al_TT2,0xff},                                                // eight
    (const uint8_t[]){al_NN2,al_AY,al_NN1,0xff},                                                // nine
    (const uint8_t[]){al_TT2,al_EH,al_NN1,0xff},                                                // ten
    (const uint8_t[]){al_EH,al_LL,al_EH,al_VV,al_EH,al_NN1,0xff} ,                              // eleven
    (const uint8_t[]){al_TT2,al_WW,al_EH,al_LL,al_VV,0xff},                                     // twelve
    (const uint8_t[]){al_TH,al_ER2,al_PA2,al_DD2,al_PA3,al_TT2,al_IY,al_NN1,0xff},              // thirdteen
    (const uint8_t[]){al_FF,al_OR,al_PA3,al_TT2,al_IY,al_NN1,0xff},                             // fourteen
    (const uint8_t[]){al_FF,al_IH,al_FF,al_PA3,al_TT2,al_EH,al_NN1,0xff},                       // fiveteen
    (const uint8_t[]){al_SS,al_SS,al_IH,al_PA3,al_KK2,al_SS,al_PA3,al_TT2,al_IY,al_NN1,0xff},   // sixteen
    (const uint8_t[]){al_SS,al_SS,al_EH,al_VV,al_EH,al_NN1,al_PA3,al_TT2,al_IY,al_NN1,0xff},    // seventeen
    (const uint8_t[]){al_EY,al_PA3,al_TT2,al_IY,al_NN1,0xff},                                   // eightteen
    (const uint8_t[]){al_NN2,al_AY,al_NN1,al_EH,al_PA3,al_TT2,al_IY,al_NN1,0xff}                // nineteen
};

const uint8_t* decenas[] = {
    (const uint8_t[]){al_TT2,al_WW,al_EH,al_NN1,al_PA3,al_TT2,al_IY,0xff},                  // twenty
    (const uint8_t[]){al_TH,al_ER2,al_PA3,al_TT2,al_IY ,0xff},                              // thirty
    (const uint8_t[]){al_FF,al_OR,al_PA3,al_TT2,al_IY,0xff},                                // fourty
    (const uint8_t[]){al_FF,al_IH,al_VV,al_EH,al_PA3,al_TT2,al_IY,0xff},                    // fivety
    (const uint8_t[]){al_SS,al_SS,al_IH,al_PA3,al_KK2,al_SS,al_PA3,al_TT2,al_IY,0xff},      // sixty
    (const uint8_t[]){al_SS,al_SS,al_EH,al_VV,al_EH,al_NN1,al_PA3,al_TT2,al_IY,0xff},       // seventy
    (const uint8_t[]){al_EY,al_PA3,al_TT2,al_IY,0xff},                                      // eighty
    (const uint8_t[]){al_NN2,al_AY,al_NN1,al_EH,al_PA3,al_TT2,al_IY,0xff}                   // ninety
};

const uint8_t centena[] = {al_HH2,al_AX,al_AX,al_NN1,al_PA2,al_DD2,al_RR2,al_PA2,al_DD1,0xff};
const uint8_t millar[] = {al_TH,al_AW,al_NN1,al_SS,al_SS,al_AE,al_NN1,al_PA2,al_DD1,0xff};
const uint8_t millon[] = {al_MM,al_IH,al_LL,al_YY1,al_AX,al_NN1,0xff};
const uint8_t billon[] = {al_BB2,al_BB2,al_IH,al_LL,al_YY1,al_AX,al_NN1,0xff};

const uint8_t snd_and[] = {al_AE,al_NN1,al_PA2,al_DD1,0xff};


// Alófonos de múltiples caracteres y de un solo carácter

const t_alofono alof[] = {
    {"PA1", 3, (const uint8_t[]){al_PA1,0xff}}, // Uso de literal compuesto explícito
    {"PA2", 3, (const uint8_t[]){al_PA2,0xff}},
    {"PA3", 3, (const uint8_t[]){al_PA3,0xff}},
    {"PA4", 3, (const uint8_t[]){al_PA4,0xff}},
    {"PA5", 3, (const uint8_t[]){al_PA5,0xff}},
    {"TT2", 3, (const uint8_t[]){al_TT2,0xff}},
    {"RR1", 3, (const uint8_t[]){al_RR1,0xff}},
    {"TT1", 3, (const uint8_t[]){al_TT1,0xff}},
    {"DH1", 3, (const uint8_t[]){al_DH1,0xff}},
    {"DD1", 3, (const uint8_t[]){al_DD1,0xff}},
    {"UW1", 3, (const uint8_t[]){al_UW1,0xff}},
    {"YY2", 3, (const uint8_t[]){al_YY2,0xff}},
    {"HH1", 3, (const uint8_t[]){al_HH1,0xff}},
    {"BB1", 3, (const uint8_t[]){al_BB1,0xff}},
    {"UW2", 3, (const uint8_t[]){al_UW2,0xff}},
    {"DD2", 3, (const uint8_t[]){al_DD2,0xff}},
    {"GG3", 3, (const uint8_t[]){al_GG3,0xff}},
    {"NN2", 3, (const uint8_t[]){al_NN2,0xff}},
    {"HH2", 3, (const uint8_t[]){al_HH2,0xff}},
    {"BB2", 3, (const uint8_t[]){al_BB2,0xff}},
    {"GG2", 3, (const uint8_t[]){al_GG2,0xff}},
    {"DH2", 3, (const uint8_t[]){al_DH2,0xff}},
    {"ER1", 3, (const uint8_t[]){al_ER1,0xff}},
    {"ER2", 3, (const uint8_t[]){al_ER2,0xff}},
    {"YY1", 3, (const uint8_t[]){al_YY1,0xff}},
    {"KK2", 3, (const uint8_t[]){al_KK2,0xff}},
    {"KK1", 3, (const uint8_t[]){al_KK1,0xff}},
    {"RR2", 3, (const uint8_t[]){al_RR2,0xff}},
    {"GG1", 3, (const uint8_t[]){al_GG1,0xff}},
    {"KK3", 3, (const uint8_t[]){al_KK3,0xff}},
    {"NN1", 3, (const uint8_t[]){al_NN1,0xff}},
    {"OY",  2, (const uint8_t[]){al_OY,0xff}},
    {"AY",  2, (const uint8_t[]){al_AY,0xff}},
    {"EH",  2, (const uint8_t[]){al_EH,0xff}},
    {"PP",  2, (const uint8_t[]){al_PP,0xff}},
    {"JH",  2, (const uint8_t[]){al_JH,0xff}},
    {"IH",  2, (const uint8_t[]){al_IH,0xff}},
    {"AX",  2, (const uint8_t[]){al_AX,0xff}},
    {"MM",  2, (const uint8_t[]){al_MM,0xff}},
    {"IY",  2, (const uint8_t[]){al_IY,0xff}},
    {"EY",  2, (const uint8_t[]){al_EY,0xff}},
    {"AO",  2, (const uint8_t[]){al_AO,0xff}},
    {"AA",  2, (const uint8_t[]){al_AA,0xff}},
    {"AE",  2, (const uint8_t[]){al_AE,0xff}},
    {"TH",  2, (const uint8_t[]){al_TH,0xff}},
    {"UH",  2, (const uint8_t[]){al_UH,0xff}},
    {"AW",  2, (const uint8_t[]){al_AW,0xff}},
    {"VV",  2, (const uint8_t[]){al_VV,0xff}},
    {"SH",  2, (const uint8_t[]){al_SH,0xff}},
    {"ZH",  2, (const uint8_t[]){al_ZH,0xff}},
    {"FF",  2, (const uint8_t[]){al_FF,0xff}},
    {"ZZ",  2, (const uint8_t[]){al_ZZ,0xff}},
    {"NG",  2, (const uint8_t[]){al_NG,0xff}},
    {"LL",  2, (const uint8_t[]){al_LL,0xff}},
    {"WW",  2, (const uint8_t[]){al_WW,0xff}},
    {"XR",  2, (const uint8_t[]){al_XR,0xff}},
    {"WH",  2, (const uint8_t[]){al_WH,0xff}},
    {"CH",  2, (const uint8_t[]){al_CH,0xff}},
    {"OW",  2, (const uint8_t[]){al_OW,0xff}},
    {"SS",  2, (const uint8_t[]){al_SS,0xff}},
    {"OR",  2, (const uint8_t[]){al_OR,0xff}},
    {"AR",  2, (const uint8_t[]){al_AR,0xff}},
    {"YR",  2, (const uint8_t[]){al_YR,0xff}},
    {"EL",  2, (const uint8_t[]){al_EL,0xff}},
    {NULL, 0, NULL}
};

const t_alofono words[] = {

    {"INVESTIGATORS", 13, (const uint8_t[]){al_IH, al_IH, al_NN1, al_VV, al_EH, al_SS, al_PA2, al_PA3, al_TT2, al_IH, al_PA1, al_GG1, al_EY, al_PA2, al_TT2, al_ER1, al_ZZ, 0xFF}}, 
    {"IRRESPONSIBLE", 13, (const uint8_t[]){al_IH, al_RR1, al_IH, al_SS, al_SS, al_PA3, al_PP, al_AA, al_SS, al_AX, al_NN1, al_SS, al_PA2, al_PA3, al_BB2, al_LL, 0xFF}}, 
    {"INVESTIGATOR", 12, (const uint8_t[]){al_IH, al_IH, al_NN1, al_VV, al_EH, al_SS, al_PA2, al_PA3, al_TT2, al_IH, al_PA1, al_GG1, al_EY, al_PA2, al_TT2, al_ER1, 0xFF}}, 
    {"INVESTIGATE", 11, (const uint8_t[]){al_IH, al_IH, al_NN1, al_VV, al_EH, al_SS, al_PA2, al_PA3, al_TT2, al_IH, al_PA1, al_GG1, al_EY, al_PA2, al_TT2, 0xFF}},
    {"CORRECTING", 10, (const uint8_t[]){al_KK1, al_ER2, al_EH, al_EH, al_PA2, al_KK2, al_PA2, al_TT2, al_IH, al_NG, 0xFF}},
    {"COGNITIVE", 9, (const uint8_t[]){al_KK3, al_AA, al_AA, al_GG3, al_NN1, al_IH, al_PA3, al_TT2, al_IH, al_VV, 0xFF}}, 
    {"CORRECTED", 9, (const uint8_t[]){al_KK1, al_ER2, al_EH, al_EH, al_PA2, al_KK2, al_PA2, al_TT2, al_IH, al_PA2, al_DD1, 0xFF}}, 
    {"SEPTEMBER", 9, (const uint8_t[]){al_SS, al_SS, al_EH, al_PA3, al_PP, al_PA3, al_TT2, al_EH, al_EH, al_PA1, al_BB2, al_ER1, 0xFF}},
    {"SINCERELY", 9, (const uint8_t[]){al_SS, al_SS, al_IH, al_IH, al_NN1, al_SS, al_YR, al_LL, al_IY, 0xFF}},
    {"SINCERITY", 9, (const uint8_t[]){al_SS, al_SS, al_IH, al_IH, al_NN1, al_SS, al_EH, al_EH, al_RR1, al_IH, al_PA2, al_PA3, al_TT2, al_IY, 0xFF}}, 
    {"INTERFACE", 9, (const uint8_t[]){al_IH, al_NN1, al_TT2, al_ER1, al_FF, al_EY, al_SS, 0xFF}},
    {"CHECKERS", 8, (const uint8_t[]){al_CH, al_EH, al_EH, al_PA3, al_KK1, al_ER1, al_ZZ, 0xFF}},
    {"CHECKING", 8, (const uint8_t[]){al_CH, al_EH, al_EH, al_PA3, al_KK1, al_IH, al_NG, 0xFF}},
    {"COMPUTER", 8, (const uint8_t[]){al_KK1, al_AX, al_MM, al_PP, al_YY1, al_UW1, al_TT2, al_ER1, 0xFF}}, 
    {"CORRECTS", 8, (const uint8_t[]){al_KK1, al_ER2, al_EH, al_EH, al_PA2, al_KK2, al_PA2, al_TT1, al_SS, 0xFF}}, 
    {"DAUGHTER", 8, (const uint8_t[]){al_DD2, al_AO, al_TT2, al_ER1, 0xFF}}, 
    {"DECEMBER", 8, (const uint8_t[]){al_DD2, al_IY, al_SS, al_SS, al_EH, al_EH, al_MM, al_PA1, al_BB2, al_ER1, 0xFF}},
    {"EIGHTEEN", 8, (const uint8_t[]){al_EY, al_PA2, al_PA3, al_TT2, al_IY, al_NN1, 0xFF}},
    {"FEBRUARY", 8, (const uint8_t[]){al_FF, al_EH, al_EH, al_PA1, al_BB1, al_RR2, al_UW2, al_XR, al_IY, 0xFF}},
    {"FREEZERS", 8, (const uint8_t[]){al_FF, al_FF, al_RR1, al_IY, al_ZZ, al_ER1, al_ZZ, 0xFF}},
    {"FREEZING", 8, (const uint8_t[]){al_FF, al_FF, al_RR1, al_IY, al_ZZ, al_IH, al_NG, 0xFF}},
    {"NINETEEN", 8, (const uint8_t[]){al_NN1, al_AY, al_NN1, al_PA2, al_PA3, al_TT2, al_IY, al_NN1, 0xFF}}, 
    {"NOVEMBER", 8, (const uint8_t[]){al_NN2, al_OW, al_VV, al_EH, al_EH, al_MM, al_PA1, al_BB2, al_ER1, 0xFF}}, 
    {"PLEDGING", 8, (const uint8_t[]){al_PP, al_LL, al_EH, al_EH, al_PA3, al_JH, al_IH, al_NG, 0xFF}},
    {"SATURDAY", 8, (const uint8_t[]){al_SS, al_SS, al_AE, al_PA3, al_TT2, al_PA2, al_DD2, al_EY, 0xFF}},
    {"THUHRDAY", 8, (const uint8_t[]){al_TH, al_ER2, al_ZZ, al_PA2, al_DD2, al_EY, 0xFF}},
    {"BOOSTER", 7, (const uint8_t[]){al_BB1, al_UW2, al_SS, al_TT2, al_ER1, al_RR1, 0xFF}},
    {"CHECKED", 7, (const uint8_t[]){al_CH, al_EH, al_EH, al_PA3, al_KK2, al_PA2, al_TT2, 0xFF}},
    {"CHECKER", 7, (const uint8_t[]){al_CH, al_EH, al_EH, al_PA3, al_KK1, al_ER1, 0xFF}},
    {"CORRECT", 7, (const uint8_t[]){al_KK1, al_ER2, al_EH, al_EH, al_PA2, al_KK2, al_PA2, al_TT1, 0xFF}},
    {"FREEZER", 7, (const uint8_t[]){al_FF, al_FF, al_RR1, al_IY, al_ZZ, al_ER1, 0xFF}},
    {"JANUARY", 7, (const uint8_t[]){al_JH, al_AE, al_AE, al_NN1, al_YY2, al_XR, al_IY, 0xFF}},
    {"OCTOBER", 7, (const uint8_t[]){al_AO, al_AO, al_PA2, al_KK2, al_PA3, al_TT2, al_OW, al_PA1, al_BB2, al_ER1, 0xFF}},
    {"PLASTIC", 7, (const uint8_t[]){al_PP, al_LL, al_AE, al_SS, al_PA3, al_TT2, al_IH, al_KK1, 0xFF}},
    {"SIXTEEN", 7, (const uint8_t[]){al_SS, al_SS, al_IH, al_PA3, al_KK2, al_SS, al_PA2, al_PA3, al_TT2, al_IY, al_NN1, 0xFF}},
    {"TUESDAY", 7, (const uint8_t[]){al_TT2, al_UW2, al_ZZ, al_PA2, al_DD2, al_EY, 0xFF}}, 
    {"MINUTES", 7, (const uint8_t[]){al_MM, al_IH, al_NN1, al_IH, al_TT2, al_SS, 0xFF}}, 
    {"AUGUST", 6, (const uint8_t[]){al_AO, al_AO, al_PA2, al_GG2, al_AX, al_SS, al_PA3, al_TT1, 0xFF}},
    {"COLLIDE", 7, (const uint8_t[]){al_KK3, al_AX, al_LL, al_AY, al_DD1, 0xFF}}, 
    {"COOKIE", 6, (const uint8_t[]){al_KK3, al_UH, al_KK1, al_IY, 0xFF}},
    {"EQUALS", 6, (const uint8_t[]){al_IY, al_PA2, al_PA3, al_KK3, al_WH, al_AX, al_EL, al_ZZ, 0xFF}},
    {"EXTENT", 6, (const uint8_t[]){al_EH, al_KK1, al_SS, al_TT2, al_EH, al_EH, al_NN1, al_TT2, 0xFF}},
    {"FRIDAY", 6, (const uint8_t[]){al_FF, al_RR2, al_AY, al_PA2, al_DD2, al_EY, 0xFF}}, 
    {"FROZEN", 6, (const uint8_t[]){al_FF, al_FF, al_RR1, al_OW, al_ZZ, al_EH, al_NN1, 0xFF}}, 
    {"MONDAY", 6, (const uint8_t[]){al_MM, al_AX, al_AX, al_NN1, al_PA2, al_DD2, al_EY, 0xFF}},
    {"SUNDAY", 6, (const uint8_t[]){al_SS, al_SS, al_AX, al_AX, al_NN1, al_PA2, al_DD2, al_EY, 0xFF}},
    {"TALKED", 6, (const uint8_t[]){al_TT2, al_AO, al_AO, al_PA3, al_KK2, al_PA3, al_TT2, 0xFF}},
    {"TALKER", 6, (const uint8_t[]){al_TT2, al_AO, al_AO, al_PA3, al_KK1, al_ER1, 0xFF}}, 
    {"TWENTY", 6, (const uint8_t[]){al_TT2, al_WH, al_EH, al_EH, al_NN1, al_IY, 0xFF}},
    {"APRIL", 5, (const uint8_t[]){al_EY, al_PA3, al_PP, al_RR2, al_IH, al_IH, al_LL, 0xFF}},
    {"CHECK", 5, (const uint8_t[]){al_CH, al_EH, al_EH, al_PA3, al_KK2, 0xFF}},
    {"COOOP", 5, (const uint8_t[]){al_KK3, al_UW2, al_PA3, al_PP, 0xFF}},
    {"CROWN", 5, (const uint8_t[]){al_KK1, al_RR2, al_AW, al_NN1, 0xFF}},
    {"EIGHT", 5, (const uint8_t[]){al_EY, al_TT1, 0xFF}},
    {"EQUAL", 5, (const uint8_t[]){al_IY, al_PA2, al_PA3, al_KK3, al_WH, al_AX, al_EL, 0xFF}},
    {"ERROR", 5, (const uint8_t[]){al_EH, al_XR, al_OR, 0xFF}},
    {"FIFTY", 5, (const uint8_t[]){al_FF, al_FF, al_IH, al_FF, al_FF, al_TT2, al_IY, 0xFF}},
    {"HELLO", 5, (const uint8_t[]){al_HH1, al_EH, al_LL, al_OW, 0xFF}},
    {"MARCH", 5, (const uint8_t[]){al_MM, al_AR, al_PA3, al_CH, 0xFF}},
    {"MONTH", 5, (const uint8_t[]){al_MM, al_AX, al_NN1, al_TH, 0xFF}},
    {"SIXTY", 5, (const uint8_t[]){al_SS, al_SS, al_IH, al_PA3, al_KK2, al_SS, al_PA2, al_PA3, al_TT2, al_IY, 0xFF}},
    {"TALKS", 5, (const uint8_t[]){al_TT2, al_AO, al_AO, al_PA2, al_KK2, al_SS, 0xFF}},
    {"THREE", 5, (const uint8_t[]){al_TH, al_RR1, al_IY, 0xFF}},
    {"WORLD", 5, (const uint8_t[]){al_WW, al_ER1, al_LL, al_DD1, 0xFF}},
    {"DATE", 4, (const uint8_t[]){al_DD2, al_EY, al_PA3, al_TT2, 0xFF}},
    {"FIVE", 4, (const uint8_t[]){al_FF, al_AY, al_VV, 0xFF}},
    {"FOUR", 4, (const uint8_t[]){al_FF, al_OR, al_RR2, 0xFF}},
    {"HAVE", 4, (const uint8_t[]){al_HH1, al_AE, al_VV, 0xFF}},
    {"JUNE", 4, (const uint8_t[]){al_JH, al_UW2, al_NN1, 0xFF}},
    {"NINE", 4, (const uint8_t[]){al_NN1, al_AY, al_NN2, 0xFF}},
    {"RAY", 3, (const uint8_t[]){al_RR1, al_EH, al_EY, 0xFF}},
    {"RAYS", 4, (const uint8_t[]){al_RR1, al_EH, al_EY, al_ZZ, 0xFF}}, 
    {"TALK", 4, (const uint8_t[]){al_TT2, al_AO, al_AO, al_PA2, al_KK2, 0xFF}},
    {"THIS", 4, (const uint8_t[]){al_DH1, al_IH,al_SS,al_SS, 0xFF}},
    {"TIME", 4, (const uint8_t[]){al_TT2, al_AA, al_AY, al_MM, 0xFF}},
    {"WHAT", 4, (const uint8_t[]){al_WH, al_AX, al_TT1, 0xFF}},
    {"WHOA", 4, (const uint8_t[]){al_WH, al_AO, 0xFF}},
    {"WILL", 4, (const uint8_t[]){al_WW, al_IH, al_LL, 0xFF}},
    {"ZX81", 4, (const uint8_t[]){al_ZZ, al_IY, al_PA1, al_EH, al_KK2, al_SS, al_PA1, al_EY, al_PA3, al_TT2, al_IY, al_PA1, al_WW, al_AX, al_NN1, 0xFF}},
    {"ACK", 3, (const uint8_t[]){al_AE, al_KK2, 0xFF}},
    {"ACT", 3, (const uint8_t[]){al_AE, al_KK2, al_TT1, 0xFF}},
    {"ADD", 3, (const uint8_t[]){al_AE, al_DD1, 0xFF}},
    {"AMP", 3, (const uint8_t[]){al_AE, al_MM, al_PP, 0xFF}},
    {"ASH", 3, (const uint8_t[]){al_AE, al_SH, 0xFF}},
    {"ASK", 3, (const uint8_t[]){al_AE, al_SS, al_KK2, 0xFF}},
    {"BAD", 3, (const uint8_t[]){al_BB1, al_AE, al_DD1, 0xFF}},
    {"BED", 3, (const uint8_t[]){al_BB1, al_EH, al_DD1, 0xFF}},
    {"BIG", 3, (const uint8_t[]){al_BB1, al_IH, al_GG1, 0xFF}},
    {"BOX", 3, (const uint8_t[]){al_BB1, al_AO, al_KK2, al_SS, 0xFF}},
    {"BUT", 3, (const uint8_t[]){al_BB1, al_AX, al_TT1, 0xFF}}, 
    {"CAR", 3, (const uint8_t[]){al_KK1, al_AR, al_RR1, 0xFF}},
    {"CHA", 3, (const uint8_t[]){al_CH, al_AE, 0xFF}},
    {"DHA", 3, (const uint8_t[]){al_DH1, al_AE, 0xFF}},
    {"DHE", 3, (const uint8_t[]){al_DH1, al_EH, 0xFF}},
    {"DHI", 3, (const uint8_t[]){al_DH1, al_IH, 0xFF}},
    {"DHO", 3, (const uint8_t[]){al_DH1, al_AO, 0xFF}},
    {"DHU", 3, (const uint8_t[]){al_DH1, al_AX, 0xFF}},
    {"END", 3, (const uint8_t[]){al_EH, al_NN1, al_DD1, 0xFF}},
    {"EST", 3, (const uint8_t[]){al_EH, al_SS, al_TT1, 0xFF}},
    {"GET", 3, (const uint8_t[]){al_GG1, al_EH, al_TT1, 0xFF}},
    {"HAS", 3, (const uint8_t[]){al_HH1, al_AE, al_ZZ, 0xFF}},
    {"HIM", 3, (const uint8_t[]){al_HH1, al_IH, al_MM, 0xFF}},
    {"ICK", 3, (const uint8_t[]){al_IH, al_KK2, 0xFF}},
    {"IMP", 3, (const uint8_t[]){al_IH, al_MM, al_PP, 0xFF}},
    {"ING", 3, (const uint8_t[]){al_IH, al_NG, 0xFF}},
    {"INK", 3, (const uint8_t[]){al_IH, al_NG, al_KK2, 0xFF}},
    {"JOB", 3, (const uint8_t[]){al_JH, al_AO, al_BB1, 0xFF}},
    {"KEY", 3, (const uint8_t[]){al_KK1, al_IY, 0xFF}}, 
    {"MAY", 3, (const uint8_t[]){al_MM, al_EY, 0xFF}},
    {"NOT", 3, (const uint8_t[]){al_NN1, al_AO, al_TT1, 0xFF}},
    {"NOW", 3, (const uint8_t[]){al_NN1, al_AW, 0xFF}},
    {"NGA", 3, (const uint8_t[]){al_NG, al_AE, 0xFF}},
    {"NGE", 3, (const uint8_t[]){al_NG, al_EH, 0xFF}},
    {"NGI", 3, (const uint8_t[]){al_NG, al_IH, 0xFF}},
    {"NGO", 3, (const uint8_t[]){al_NG, al_AO, 0xFF}},
    {"NGU", 3, (const uint8_t[]){al_NG, al_AX, 0xFF}},
    {"OLD", 3, (const uint8_t[]){al_OW, al_LL, al_DD1, 0xFF}},
    {"OR", 2, (const uint8_t[]){al_OR, al_RR2, 0xFF}}, 
    {"OUR", 3, (const uint8_t[]){al_AW, al_RR1, 0xFF}}, 
    {"OUT", 3, (const uint8_t[]){al_AW, al_TT1, 0xFF}},
    {"RED", 3, (const uint8_t[]){al_RR1, al_EH, al_EH, al_PA1, al_DD1, 0xFF}}, 
    {"SHI", 3, (const uint8_t[]){al_SH, al_IH, 0xFF}},
    {"SHO", 3, (const uint8_t[]){al_SH, al_AO, 0xFF}},
    {"SHU", 3, (const uint8_t[]){al_SH, al_AX, 0xFF}},
    {"SIX", 3, (const uint8_t[]){al_SS, al_IH, al_KK1, al_SS, 0xFF}},
    {"SUN", 3, (const uint8_t[]){al_SS, al_AX, al_NN1, 0xFF}},
    {"TEN", 3, (const uint8_t[]){al_TT1, al_EH, al_NN1, 0xFF}},
    {"THA", 3, (const uint8_t[]){al_TH, al_AE, 0xFF}},
    {"THE", 3, (const uint8_t[]){al_DD1, al_IY, 0xFF}},
    {"THI", 3, (const uint8_t[]){al_TH, al_IH, 0xFF}},
    {"THO", 3, (const uint8_t[]){al_TH, al_AO, 0xFF}},
    {"THU", 3, (const uint8_t[]){al_TH, al_AX, 0xFF}},
    {"TOP", 3, (const uint8_t[]){al_TT1, al_AO, al_PP, 0xFF}},
    {"TWO", 3, (const uint8_t[]){al_TT1, al_UW2, 0xFF}}, 
    {"WHA", 3, (const uint8_t[]){al_WH, al_AE, 0xFF}},
    {"WHE", 3, (const uint8_t[]){al_WH, al_EH, 0xFF}},
    {"WHI", 3, (const uint8_t[]){al_WH, al_IH, 0xFF}},
    {"WHU", 3, (const uint8_t[]){al_WH, al_AX, 0xFF}},
    {"YES", 3, (const uint8_t[]){al_YY1, al_EH, al_SS, 0xFF}},
    {"AB", 2, (const uint8_t[]){al_AE, al_BB1, 0xFF}}, 
    {"AD", 2, (const uint8_t[]){al_AE, al_DD1, 0xFF}},
    {"AF", 2, (const uint8_t[]){al_AE, al_FF, 0xFF}}, 
    {"AG", 2, (const uint8_t[]){al_AE, al_GG1, 0xFF}},
    {"AH", 2, (const uint8_t[]){al_AE, al_HH1, 0xFF}}, 
    {"AJ", 2, (const uint8_t[]){al_AE, al_JH, 0xFF}},
    {"AK", 2, (const uint8_t[]){al_AE, al_KK1, 0xFF}}, 
    {"AL", 2, (const uint8_t[]){al_AE, al_LL, 0xFF}},
    {"AM", 2, (const uint8_t[]){al_AE, al_MM, 0xFF}}, 
    {"AN", 2, (const uint8_t[]){al_AE, al_NN1, 0xFF}},
    {"AP", 2, (const uint8_t[]){al_AE, al_PP, 0xFF}}, 
    {"AR", 2, (const uint8_t[]){al_AE, al_RR1, 0xFF}},
    {"AS", 2, (const uint8_t[]){al_AE, al_SS, 0xFF}}, 
    {"AT", 2, (const uint8_t[]){al_AE, al_TT1, 0xFF}},
    {"AV", 2, (const uint8_t[]){al_AE, al_VV, 0xFF}}, 
    {"AW", 2, (const uint8_t[]){al_AE, al_WW, 0xFF}},
    {"AY", 2, (const uint8_t[]){al_AE, al_YY1, 0xFF}}, 
    {"AZ", 2, (const uint8_t[]){al_AE, al_ZZ, 0xFF}},
    {"BA", 2, (const uint8_t[]){al_BB1, al_AE, 0xFF}}, 
    {"BE", 2, (const uint8_t[]){al_BB1, al_EH, 0xFF}},
    {"BI", 2, (const uint8_t[]){al_BB1, al_IH, 0xFF}}, 
    {"BO", 2, (const uint8_t[]){al_BB1, al_AO, 0xFF}},
    {"BU", 2, (const uint8_t[]){al_BB1, al_AX, 0xFF}}, 
    {"CE", 2, (const uint8_t[]){al_SS, al_EH, 0xFF}},
    {"CI", 2, (const uint8_t[]){al_SS, al_IH, 0xFF}},
    {"CO", 2, (const uint8_t[]){al_KK1, al_AO, 0xFF}}, 
    {"CU", 2, (const uint8_t[]){al_KK1, al_AX, 0xFF}},
    {"DA", 2, (const uint8_t[]){al_DD1, al_AE, 0xFF}}, 
    {"DE", 2, (const uint8_t[]){al_DD1, al_EH, 0xFF}},
    {"DI", 2, (const uint8_t[]){al_DD1, al_IH, 0xFF}},
    {"DO", 2, (const uint8_t[]){al_DD1, al_AO, 0xFF}}, 
    {"DU", 2, (const uint8_t[]){al_DD1, al_AX, 0xFF}},
    {"EB", 2, (const uint8_t[]){al_EH, al_BB1, 0xFF}}, 
    {"ED", 2, (const uint8_t[]){al_EH, al_DD1, 0xFF}},
    {"EF", 2, (const uint8_t[]){al_EH, al_FF, 0xFF}}, 
    {"EG", 2, (const uint8_t[]){al_EH, al_GG1, 0xFF}},
    {"EH", 2, (const uint8_t[]){al_EH, al_HH1, 0xFF}}, 
    {"EJ", 2, (const uint8_t[]){al_JH, al_EH, 0xFF}},
    {"EK", 2, (const uint8_t[]){al_EH, al_KK1, 0xFF}}, 
    {"EL", 2, (const uint8_t[]){al_EH, al_LL, 0xFF}},
    {"EM", 2, (const uint8_t[]){al_EH, al_MM, 0xFF}}, 
    {"EN", 2, (const uint8_t[]){al_EH, al_NN1, 0xFF}},
    {"EP", 2, (const uint8_t[]){al_EH, al_PP, 0xFF}}, 
    {"ER", 2, (const uint8_t[]){al_EH, al_RR1, 0xFF}},
    {"ES", 2, (const uint8_t[]){al_EH, al_SS, 0xFF}}, 
    {"ET", 2, (const uint8_t[]){al_EH, al_TT1, 0xFF}},
    {"EV", 2, (const uint8_t[]){al_EH, al_VV, 0xFF}}, 
    {"EW", 2, (const uint8_t[]){al_EH, al_WW, 0xFF}},
    {"EY", 2, (const uint8_t[]){al_EH, al_YY1, 0xFF}}, 
    {"EZ", 2, (const uint8_t[]){al_EH, al_ZZ, 0xFF}},
    {"FA", 2, (const uint8_t[]){al_FF, al_AE, 0xFF}}, 
    {"FE", 2, (const uint8_t[]){al_FF, al_EH, 0xFF}},
    {"FI", 2, (const uint8_t[]){al_FF, al_IH, 0xFF}}, 
    {"FO", 2, (const uint8_t[]){al_FF, al_AO, 0xFF}},
    {"FU", 2, (const uint8_t[]){al_FF, al_AX, 0xFF}}, 
    {"GA", 2, (const uint8_t[]){al_GG1, al_AE, 0xFF}},
    {"GE", 2, (const uint8_t[]){al_GG1, al_EH, 0xFF}}, 
    {"GI", 2, (const uint8_t[]){al_GG1, al_IH, 0xFF}},
    {"GO", 2, (const uint8_t[]){al_GG1, al_AO, 0xFF}}, 
    {"GU", 2, (const uint8_t[]){al_GG1, al_AX, 0xFF}},
    {"HA", 2, (const uint8_t[]){al_HH1, al_AE, 0xFF}}, 
    {"HE", 2, (const uint8_t[]){al_HH1, al_EH, 0xFF}},
    {"HI", 2, (const uint8_t[]){al_HH1, al_IH, 0xFF}}, 
    {"HO", 2, (const uint8_t[]){al_HH1, al_AO, 0xFF}},
    {"HU", 2, (const uint8_t[]){al_HH1, al_AX, 0xFF}}, 
    {"IB", 2, (const uint8_t[]){al_IH, al_BB1, 0xFF}}, 
    {"ID", 2, (const uint8_t[]){al_IH, al_DD1, 0xFF}},
    {"IF", 2, (const uint8_t[]){al_IH, al_FF, 0xFF}}, 
    {"IG", 2, (const uint8_t[]){al_IH, al_GG1, 0xFF}},
    {"IH", 2, (const uint8_t[]){al_IH, al_HH1, 0xFF}}, 
    {"IJ", 2, (const uint8_t[]){al_JH, al_IH, 0xFF}},
    {"IK", 2, (const uint8_t[]){al_IH, al_KK1, 0xFF}}, 
    {"IL", 2, (const uint8_t[]){al_IH, al_LL, 0xFF}},
    {"IM", 2, (const uint8_t[]){al_IH, al_MM, 0xFF}}, 
    {"IN", 2, (const uint8_t[]){al_IH, al_NN1, 0xFF}},
    {"IP", 2, (const uint8_t[]){al_IH, al_PP, 0xFF}}, 
    {"IR", 2, (const uint8_t[]){al_IH, al_RR1, 0xFF}},
    {"IS", 2, (const uint8_t[]){al_IH, al_SS,al_SS,0xFF}}, 
    {"IT", 2, (const uint8_t[]){al_IH, al_TT1, 0xFF}},
    {"IV", 2, (const uint8_t[]){al_IH, al_VV, 0xFF}}, 
    {"IW", 2, (const uint8_t[]){al_IH, al_WW, 0xFF}},
    {"IY", 2, (const uint8_t[]){al_IH, al_YY1, 0xFF}}, 
    {"IZ", 2, (const uint8_t[]){al_IH, al_ZZ, 0xFF}},
    {"JA", 2, (const uint8_t[]){al_JH, al_AE, 0xFF}}, 
    {"JE", 2, (const uint8_t[]){al_JH, al_EH, 0xFF}},
    {"JI", 2, (const uint8_t[]){al_JH, al_IH, 0xFF}}, 
    {"JO", 2, (const uint8_t[]){al_JH, al_AO, 0xFF}},
    {"JU", 2, (const uint8_t[]){al_JH, al_AX, 0xFF}}, 
    {"KA", 2, (const uint8_t[]){al_KK1, al_AE, 0xFF}}, 
    {"KE", 2, (const uint8_t[]){al_KK1, al_EH, 0xFF}},
    {"KI", 2, (const uint8_t[]){al_KK1, al_IH, 0xFF}}, 
    {"KO", 2, (const uint8_t[]){al_KK1, al_AO, 0xFF}},
    {"KU", 2, (const uint8_t[]){al_KK1, al_AX, 0xFF}}, 
    {"LA", 2, (const uint8_t[]){al_LL, al_AE, 0xFF}},
    {"LE", 2, (const uint8_t[]){al_LL, al_EH, 0xFF}}, 
    {"LI", 2, (const uint8_t[]){al_LL, al_IH, 0xFF}},
    {"LO", 2, (const uint8_t[]){al_LL, al_AO, 0xFF}}, 
    {"LU", 2, (const uint8_t[]){al_LL, al_AX, 0xFF}},
    {"MA", 2, (const uint8_t[]){al_MM, al_AE, 0xFF}}, 
    {"ME", 2, (const uint8_t[]){al_MM, al_EH, 0xFF}},
    {"MI", 2, (const uint8_t[]){al_MM, al_IH, 0xFF}}, 
    {"MO", 2, (const uint8_t[]){al_MM, al_AO, 0xFF}},
    {"MU", 2, (const uint8_t[]){al_MM, al_AX, 0xFF}}, 
    {"MY", 2, (const uint8_t[]){al_MM, al_AY, 0xFF}},
    {"NA", 2, (const uint8_t[]){al_NN1, al_AE, 0xFF}}, 
    {"NE", 2, (const uint8_t[]){al_NN1, al_EH, 0xFF}},
    {"NI", 2, (const uint8_t[]){al_NN1, al_IH, 0xFF}}, 
    {"NO", 2, (const uint8_t[]){al_NN1, al_AO, 0xFF}},
    {"NU", 2, (const uint8_t[]){al_NN1, al_AX, 0xFF}}, 
    {"OB", 2, (const uint8_t[]){al_AO, al_BB1, 0xFF}}, 
    {"OD", 2, (const uint8_t[]){al_AO, al_DD1, 0xFF}},
    {"OF", 2, (const uint8_t[]){al_AO, al_FF, 0xFF}}, 
    {"OG", 2, (const uint8_t[]){al_AO, al_GG1, 0xFF}},
    {"OH", 2, (const uint8_t[]){al_AO, al_HH1, 0xFF}}, 
    {"OJ", 2, (const uint8_t[]){al_JH, al_AO, 0xFF}},
    {"OK", 2, (const uint8_t[]){al_AO, al_KK1, 0xFF}}, 
    {"OL", 2, (const uint8_t[]){al_AO, al_LL, 0xFF}},
    {"OM", 2, (const uint8_t[]){al_AO, al_MM, 0xFF}}, 
    {"ON", 2, (const uint8_t[]){al_AO, al_NN1, 0xFF}},
    {"OP", 2, (const uint8_t[]){al_AO, al_PP, 0xFF}}, 
    {"OR", 2, (const uint8_t[]){al_AO, al_RR1, 0xFF}},
    {"OS", 2, (const uint8_t[]){al_AO, al_SS, 0xFF}}, 
    {"OT", 2, (const uint8_t[]){al_AO, al_TT1, 0xFF}},
    {"OV", 2, (const uint8_t[]){al_AO, al_VV, 0xFF}}, 
    {"OW", 2, (const uint8_t[]){al_AO, al_WW, 0xFF}},
    {"OY", 2, (const uint8_t[]){al_AO, al_YY1, 0xFF}}, 
    {"OZ", 2, (const uint8_t[]){al_AO, al_ZZ, 0xFF}},
    {"PA", 2, (const uint8_t[]){al_PP, al_AE, 0xFF}}, 
    {"PE", 2, (const uint8_t[]){al_PP, al_EH, 0xFF}},
    {"PI", 2, (const uint8_t[]){al_PP, al_IH, 0xFF}}, 
    {"PO", 2, (const uint8_t[]){al_PP, al_AO, 0xFF}},
    {"PU", 2, (const uint8_t[]){al_PP, al_AX, 0xFF}}, 
    {"RA", 2, (const uint8_t[]){al_RR1, al_AE, 0xFF}},
    {"RE", 2, (const uint8_t[]){al_RR1, al_EH, 0xFF}}, 
    {"RI", 2, (const uint8_t[]){al_RR1, al_IH, 0xFF}},
    {"RO", 2, (const uint8_t[]){al_RR1, al_AO, 0xFF}}, 
    {"RU", 2, (const uint8_t[]){al_RR1, al_AX, 0xFF}},
    {"SA", 2, (const uint8_t[]){al_SS, al_AE, 0xFF}}, 
    {"SE", 2, (const uint8_t[]){al_SS, al_EH, 0xFF}},
    {"SI", 2, (const uint8_t[]){al_SS, al_IH, 0xFF}}, 
    {"SO", 2, (const uint8_t[]){al_SS, al_AO, 0xFF}},
    {"SU", 2, (const uint8_t[]){al_SS, al_AX, 0xFF}}, 
    {"TA", 2, (const uint8_t[]){al_TT1, al_AE, 0xFF}},
    {"TE", 2, (const uint8_t[]){al_TT1, al_EH, 0xFF}}, 
    {"TH", 2, (const uint8_t[]){al_DH1,0xff}},
    {"TI", 2, (const uint8_t[]){al_TT1, al_IH, 0xFF}}, 
    {"TO", 2, (const uint8_t[]){al_TT1, al_AO, 0xFF}},
    {"TU", 2, (const uint8_t[]){al_TT1, al_AX, 0xFF}}, 
    {"UB", 2, (const uint8_t[]){al_AX, al_BB1, 0xFF}}, 
    {"UD", 2, (const uint8_t[]){al_AX, al_DD1, 0xFF}},
    {"UF", 2, (const uint8_t[]){al_AX, al_FF, 0xFF}}, 
    {"UG", 2, (const uint8_t[]){al_AX, al_GG1, 0xFF}},
    {"UH", 2, (const uint8_t[]){al_AX, al_HH1, 0xFF}}, 
    {"UJ", 2, (const uint8_t[]){al_JH, al_AX, 0xFF}},
    {"UK", 2, (const uint8_t[]){al_AX, al_KK1, 0xFF}}, 
    {"UL", 2, (const uint8_t[]){al_AX, al_LL, 0xFF}},
    {"UM", 2, (const uint8_t[]){al_AX, al_MM, 0xFF}}, 
    {"UN", 2, (const uint8_t[]){al_AX, al_NN1, 0xFF}},
    {"UP", 2, (const uint8_t[]){al_AX, al_PP, 0xFF}}, 
    {"UR", 2, (const uint8_t[]){al_AX, al_RR1, 0xFF}},
    {"US", 2, (const uint8_t[]){al_AX, al_SS, 0xFF}}, 
    {"UT", 2, (const uint8_t[]){al_AX, al_TT1, 0xFF}},
    {"UV", 2, (const uint8_t[]){al_AX, al_VV, 0xFF}}, 
    {"UW", 2, (const uint8_t[]){al_AX, al_WW, 0xFF}},
    {"UY", 2, (const uint8_t[]){al_AX, al_YY1, 0xFF}}, 
    {"UZ", 2, (const uint8_t[]){al_AX, al_ZZ, 0xFF}},
    {"VA", 2, (const uint8_t[]){al_VV, al_AE, 0xFF}}, 
    {"VE", 2, (const uint8_t[]){al_VV, al_EH, 0xFF}},
    {"VI", 2, (const uint8_t[]){al_VV, al_IH, 0xFF}}, 
    {"VO", 2, (const uint8_t[]){al_VV, al_AO, 0xFF}},
    {"VU", 2, (const uint8_t[]){al_VV, al_AX, 0xFF}}, 
    {"WA", 2, (const uint8_t[]){al_WW, al_AE, 0xFF}}, 
    {"WE", 2, (const uint8_t[]){al_WW, al_EH, 0xFF}},
    {"WI", 2, (const uint8_t[]){al_WW, al_IH, 0xFF}}, 
    {"WO", 2, (const uint8_t[]){al_WW, al_AO, 0xFF}},
    {"WU", 2, (const uint8_t[]){al_WW, al_AX, 0xFF}}, 
    {"YA", 2, (const uint8_t[]){al_YY1, al_AE, 0xFF}}, 
    {"YE", 2, (const uint8_t[]){al_YY1, al_EH, 0xFF}},
    {"YI", 2, (const uint8_t[]){al_YY1, al_IH, 0xFF}}, 
    {"YO", 2, (const uint8_t[]){al_YY1, al_AO, 0xFF}},
    {"YU", 2, (const uint8_t[]){al_YY1, al_AX, 0xFF}}, 
    {"ZA", 2, (const uint8_t[]){al_ZZ, al_AE, 0xFF}},
    {"ZE", 2, (const uint8_t[]){al_ZZ, al_EH, 0xFF}}, 
    {"ZI", 2, (const uint8_t[]){al_ZZ, al_IH, 0xFF}},
    {"ZO", 2, (const uint8_t[]){al_ZZ, al_AO, 0xFF}}, 
    {"ZU", 2, (const uint8_t[]){al_ZZ, al_AX, 0xFF}},
    {" ", 1, (const uint8_t[]){al_PA2, 0xFF}},
    {",", 1, (const uint8_t[]){al_PA3, 0xFF}},
    {";", 1, (const uint8_t[]){al_PA4, 0xFF}},
    {":", 1, (const uint8_t[]){al_PA4, 0xFF}},
    {"A", 1, (const uint8_t[]){al_EY,0xff}},
    {"B", 1, (const uint8_t[]){al_BB2,al_IY,0xff}},
    {"C", 1, (const uint8_t[]){al_SS,al_SS,al_IY,0xff}},
    {"D", 1, (const uint8_t[]){al_DD2,al_IY,0xff}},
    {"E", 1, (const uint8_t[]){al_IY,0xff}},
    {"F", 1, (const uint8_t[]){al_EH,al_EH,al_FF,0xff}},
    {"G", 1, (const uint8_t[]){al_JH,al_IY,0xff}},
    {"H", 1, (const uint8_t[]){al_EY,al_PA2,al_PA3,al_CH,0xff}},
    {"I", 1, (const uint8_t[]){al_AY, 0xFF}},
    {"I", 1, (const uint8_t[]){al_AA,al_AY,0xff}},
    {"J", 1, (const uint8_t[]){al_JH,al_EH,al_EY,0xff}},
    {"K", 1, (const uint8_t[]){al_KK1, al_EH, al_EY, 0xFF}},
    {"L", 1, (const uint8_t[]){al_EH, al_EH, al_EL, 0xFF}},
    {"M", 1, (const uint8_t[]){al_EH, al_EH, al_MM, 0xFF}},
    {"N", 1, (const uint8_t[]){al_EH, al_EH, al_NN1, 0xFF}},
    {"O", 1, (const uint8_t[]){al_OW, 0xFF}}, 
    {"P", 1, (const uint8_t[]){al_PP, al_IY, 0xFF}},
    {"Q", 1, (const uint8_t[]){al_KK1, al_YY1, al_UW2, 0xFF}},
    {"R", 1, (const uint8_t[]){al_AR, al_RR2, 0xFF}},
    {"S", 1, (const uint8_t[]){al_EH, al_EH, al_SS, al_SS, 0xFF}},
    {"T", 1, (const uint8_t[]){al_TT2, al_IY, 0xFF}},
    {"U", 1, (const uint8_t[]){al_YY1, al_UW2, 0xFF}},
    {"V", 1, (const uint8_t[]){al_VV, al_IY, 0xFF}},
    {"W", 1, (const uint8_t[]){al_DD2, al_AX, al_PA2, al_BB2, al_EL, al_YY1, al_UW2, 0xff}},
    {"X", 1, (const uint8_t[]){al_EH, al_EH, al_PA3, al_KK2, al_SS, al_SS, 0xFF}},
    {"Y", 1, (const uint8_t[]){al_WW, al_AY, 0xFF}},
    {"Z", 1, (const uint8_t[]){al_ZZ, al_IY, 0xFF}},

    {NULL, 0, NULL}
};
// --- Variables internas del sintetizador de voz ---
volatile bool voice_is_playing = false;
static const char* current_alophone_sequence_hex = NULL;
static const uint8_t* current_alophone_sequence = NULL;
static uint16_t current_alophone_index_in_sequence = 0; // Índice en la secuencia hexadecimal
static int current_alophone_data_index = 0; // Índice dentro de la muestra de alófono actual
static int current_alophone_id = -1; // ID del alófono actual (0-63)

// Contador de muestras para la interpolación de la frecuencia de muestreo
static uint32_t voice_sample_counter = 0;
static uint32_t voice_sample_period_ns = 0; // Período en nanosegundos de una muestra de AY
static uint32_t sp0256_sample_period_ns = 0; // Período en nanosegundos de una muestra de SP0256


// --- Funciones internas ---

bool stringcomp(const char* str1, char* str2, int len){
#ifdef LOG3
  Serial.print("Comparing "); Serial.print(str1); Serial.print(" vs ");Serial.println(str2);
#endif
  for (int i=0;i<len;i++){
    if (toupper(str1[i])!=str2[i]) {
#ifdef LOG3
        Serial.println("not match");
#endif
        return false;
    }
  }
#ifdef LOG3
  Serial.println("match");
#endif
  return true;
};

int source_index = 0;
int dest_index = 0;

void procesa_unidad(uint64_t value, uint8_t* out){
    if (value !=0) {
        Serial.print(value); Serial.println(" unidades");
        for (int j=0; numero[value][j]!=0xff;j++){
            out[dest_index] = numero[value][j];
            dest_index++;
        }
    }
}

void procesa_decena(uint64_t decena, uint8_t* out){
    if (decena !=0) {
        Serial.print(decena); Serial.println(" decenas");
        for (int j=0; decenas[decena-2][j]!=0xff;j++){
            out[dest_index] = decenas[decena-2][j];
            dest_index++;
        }
    }
}   

void procesa_centena(uint64_t value, uint8_t* out){
    if (value > 0){
        Serial.print(value); Serial.println(" centenas");
        procesa_unidad(value,out);
        out[dest_index] = al_PA5;
        dest_index++;
        for (int j=0; centena[j]!=0xff;j++){
            out[dest_index] = centena[j];
            dest_index++;
        }
    }
}   

void procesa_and(uint64_t value, uint8_t* out){
    uint64_t unidad = value % 10;
    uint64_t decena = (value % 100)/10;
    uint64_t centena = (value % 1000)/100;
    uint64_t millar = (value % 1000000)/1000;
    uint64_t millon = (value % 1000000000)/1000000;
    uint64_t billon = value/1000000000;
    if (((billon!=0) ||(millon!=0) ||(millar!=0) || (centena !=0)) && (decena!=0 || unidad !=0) ) {
        out[dest_index] = al_PA4;
        dest_index++;
        for (int j=0; snd_and[j]!=0xff;j++){
            out[dest_index] = snd_and[j];
            dest_index++;
        }
    }
}

void procesa_decena_y_unidad(uint64_t value, uint8_t* out){
    if ((value % 100) < 20){
        procesa_unidad(value % 100,out);
    }
    else {
        int unidad = value % 10;
        int decena = (value % 100)/10;
        procesa_decena(decena,out);
        procesa_unidad(unidad,out);
    }
}

void procesa_millar(uint64_t value, uint8_t* out){
    if (value > 0){
        Serial.print(value); Serial.println(" miles");
        uint64_t unidad = value % 10;
        uint64_t decena = (value % 100)/10;
        uint64_t cientos = (value % 1000)/100;
        uint64_t miles = value/1000;
        procesa_millar(miles,out);
        procesa_centena(cientos,out);
        procesa_and(value,out);
        procesa_decena_y_unidad(value,out);
        //procesa_unidad(value,out);
        out[dest_index] = al_PA4;
        dest_index++;
        for (int j=0; millar[j]!=0xff;j++){
            out[dest_index] = millar[j];
            dest_index++;
        }
    }
}   

void procesa_millon(uint64_t value, uint8_t* out){
    if (value > 0){
        Serial.print(value); Serial.println(" millones");
        uint64_t unidad = value % 10;
        uint64_t decena = (value % 100)/10;
        uint64_t cientos = (value % 1000)/100;
        uint64_t miles = (value % 1000000) /1000;
        uint64_t millones = (value % 1000000000) /1000000;
        uint64_t billones = value / 1000000000;
        Serial.print("millares "); Serial.print(miles); Serial.print(" centenas "); Serial.print(cientos); Serial.print(" decenas "); Serial.print(decena); Serial.print(" unidades "); Serial.println(unidad);
        //procesa_millar(miles,out);
        procesa_centena(cientos,out);
        procesa_and(value, out);
        procesa_decena_y_unidad(value,out);
        //procesa_unidad(value,out);
        out[dest_index] = al_PA4;
        dest_index++;
        for (int j=0; millon[j]!=0xff;j++){
            out[dest_index] = millon[j];
            dest_index++;
        }
    }
}

void procesa_billon(uint64_t value, uint8_t* out){
    if (value > 0){
        Serial.print(value); Serial.println(" millones");
        uint64_t unidad = value % 10;
        uint64_t decena = (value % 100)/10;
        uint64_t cientos = (value % 1000)/100;
        uint64_t miles = (value % 1000000) /1000;
        uint64_t millones = (value % 1000000000) /1000000;
        uint64_t billones = value / 1000000000;
        Serial.print("millares "); Serial.print(miles); Serial.print(" centenas "); Serial.print(cientos); Serial.print(" decenas "); Serial.print(decena); Serial.print(" unidades "); Serial.println(unidad);
        procesa_billon(billones,out);
        procesa_millon(millones,out);
        procesa_millar(miles,out);
        procesa_centena(cientos,out);
        procesa_and(value, out);
        procesa_decena_y_unidad(value,out);
        //procesa_unidad(value,out);
        out[dest_index] = al_PA4;
        dest_index++;
        for (int j=0; millon[j]!=0xff;j++){
            out[dest_index] = millon[j];
            dest_index++;
        }
    }
}
// convierte una secuencia de alofonos en modo texto a un array binario 
void text_to_speech(const char* text, uint8_t* out){
    source_index = 0;
    dest_index = 0;
    mode_alof = false; // starting in english mode
    while (text[source_index]!=0){
        if (text[source_index]=='/') {
            mode_alof = !mode_alof;
            source_index++;
        } else if (mode_alof) {
            bool found = false;
            for (int i=0; alof[i].codigoHex != NULL; i++){
                if(stringcomp(text+source_index, (char*) alof[i].secuencia,alof[i].longitud)){
                    for (int j=0; alof[i].codigoHex[j]!=0xff;j++){
                        out[dest_index] = alof[i].codigoHex[j];
                        dest_index++;
                    }
                    source_index+=alof[i].longitud;
                    found = true;
                    Serial.println(source_index);
                    break;
                }
            }
            if (!found) {
                Serial.print("Unknown char :"); Serial.println(text[source_index]);
                source_index++;
            }
        } else if (isDigit(text[source_index])){             
            uint64_t value = atoll(&text[source_index]); 
            Serial.print("NUMBER "); Serial.print(value);Serial.println(" detected.");
            while (isDigit(text[source_index])) source_index++;
            uint64_t unidad = value % 10;
            uint64_t decena = (value % 100)/10;
            uint64_t centena = (value % 1000)/100;
            uint64_t millares = (value % 1000000)/1000;
            uint64_t millones = (value % 1000000000) /1000000;
            uint64_t billones = value / 1000000000;
            procesa_billon(billones,out);
            procesa_millon(millones,out);
            procesa_millar(millares,out);
            procesa_centena(centena,out);
            procesa_and(value, out);
            procesa_decena_y_unidad(value,out);
        }
        else {
            bool found = false;
            for (int i=0; words[i].codigoHex != NULL; i++){
                if(stringcomp(text+source_index, (char*) words[i].secuencia,words[i].longitud)){
                    for (int j=0; words[i].codigoHex[j]!=0xff;j++){
                        out[dest_index] = words[i].codigoHex[j];
                        dest_index++;
                    }
                    source_index+=words[i].longitud;
                    found = true;
                    Serial.println(source_index);
                    break;
                }
            }
            if (!found) {
                Serial.print("Unknown char :"); Serial.println(text[source_index]);
                source_index++;
            }
        }
    }
    out[dest_index]=0xff; //terminador
    Serial.println("end conversion....");
}

// Función para decodificar un alófono hexadecimal de la cadena
static int get_next_alophone_id() {
    if (current_alophone_sequence == NULL ||current_alophone_sequence[current_alophone_index_in_sequence] == 0xff) {
        return -1; // Fin de la secuencia
    }

    long alophone_id = current_alophone_sequence[current_alophone_index_in_sequence];
    current_alophone_index_in_sequence ++; // Avanzar dos caracteres por alófono
    if (alophone_id < 0 || alophone_id >= 64) {
        Serial.print("Error: Alophone ID fuera de rango: ");
        Serial.println(alophone_id,HEX);
        return -1; // ID inválido
    }
    return (int)alophone_id;
}

// // Función para decodificar un alófono hexadecimal de la cadena
// static int get_next_alophone_id_hex() {
//     if (current_alophone_sequence_hex == NULL || current_alophone_sequence_hex[current_alophone_index_in_sequence] == '\0') {
//         return -1; // Fin de la secuencia
//     }

//     char hex_pair[3];
//     hex_pair[0] = current_alophone_sequence_hex[current_alophone_index_in_sequence];
//     hex_pair[1] = current_alophone_sequence_hex[current_alophone_index_in_sequence + 1];
//     hex_pair[2] = '\0';

//     current_alophone_index_in_sequence += 2; // Avanzar dos caracteres por alófono

//     long alophone_id = strtol(hex_pair, NULL, 16); // Convertir hexadecimal a entero
//     if (alophone_id < 0 || alophone_id >= 64) {
//         Serial.print("Error: Alophone ID fuera de rango: ");
//         Serial.println(alophone_id);
//         return -1; // ID inválido
//     }
//     return (int)alophone_id;
// }

// Inicializa el siguiente alófono para reproducción
static void load_next_alophone() {
    current_alophone_id = get_next_alophone_id();
    current_alophone_data_index = 0;

    if (current_alophone_id == -1) {
        voice_synthesizer_stop(); // No hay más alófonos, detener la reproducción
        Serial.println("Voice synthesizer finished sequence.");
    } else {
        Serial.print("Playing alophone ID: ");
        Serial.println(current_alophone_id,HEX);
    }
}

// // --- Funciones públicas ---

// void Speech_hex(const char* alophone_sequence_hex) {
//     if (alophone_sequence_hex == NULL || strlen(alophone_sequence_hex) == 0) {
//         voice_synthesizer_stop();
//         return;
//     }

//     current_alophone_sequence_hex = alophone_sequence_hex;
//     current_alophone_index_in_sequence = 0;
//     voice_sample_counter = 0; // Resetear el contador de muestras
//     voice_is_playing = true;
//     Serial.print("Starting speech: ");
//     Serial.println(alophone_sequence_hex);

//     // Calcular los períodos en nanosegundos para la interpolación
//     voice_sample_period_ns = 1000000000UL / SAMPLE_RATE;
//     sp0256_sample_period_ns = 1000000000UL / SP0256_SAMPLE_RATE;

//     load_next_alophone(); // Cargar el primer alófono
// }
// --- Funciones públicas ---

// función principal para iniciar la reproducción de voz
void Speech(const uint8_t* alophone_sequence) {
    if (alophone_sequence == NULL || alophone_sequence[0] == 0xff) {
        voice_synthesizer_stop();
        return;
    }

    current_alophone_sequence = alophone_sequence;
    current_alophone_index_in_sequence = 0;
    voice_sample_counter = 0; // Resetear el contador de muestras
    voice_is_playing = true;
    //Serial.println("Starting speech: ");
    //Serial.println(alophone_sequence, HEX);

    // Calcular los períodos en nanosegundos para la interpolación
    voice_sample_period_ns = 1000000000UL / SAMPLE_RATE;
    sp0256_sample_period_ns = 1000000000UL / SP0256_SAMPLE_RATE;

    load_next_alophone(); // Cargar el primer alófono
}

// variable que alberga la conversion de texto a binario
uint8_t alophone_sequence[1000];
// función para iniciar la reproducción de voz desde una cadena de texto
void Speech(const char* alophone_sequence_str) {
    Serial.print("Starting speech: ");
    Serial.println(alophone_sequence_str);
    text_to_speech(alophone_sequence_str, alophone_sequence);
    Speech(alophone_sequence);
}

// Esta función es llamada por la ISR. Devuelve la muestra actual del alófono.
int16_t voice_synthesizer_get_sample() {
    if (!voice_is_playing || current_alophone_id == -1) {
        return 0; // No hay voz reproduciéndose
    }

    // Interpolación para adaptar la frecuencia de muestreo del alófono (11025 Hz)
    // a la frecuencia de muestreo del DAC (44100 Hz).
    // Esto es un simple upsampling por repetición de muestra, ya que 44100 es múltiplo de 11025 (4x).
    // Para una interpolación más suave (lineal, etc.), se necesitaría más lógica.

    // A cada SAMPLE_RATE/SP0256_SAMPLE_RATE muestras del DAC, avanzamos una muestra del alófono.
    // SAMPLE_RATE / SP0256_SAMPLE_RATE = 44100 / 11025 = 4
    if (voice_sample_counter % (SAMPLE_RATE / SP0256_SAMPLE_RATE) == 0) {
        // Asegurarse de no exceder el tamaño del alófono
        if (current_alophone_data_index < allophone_sizes[current_alophone_id]) {
            // Los alófonos son de 8 bits, centrados alrededor de 0 (ej: -128 a 127).
            // Leer el valor y normalizarlo para el rango de salida.
            // La escala es de 0-255, así que restamos 128 para centrarlo alrededor de 0.
            int8_t sample_8bit = (int8_t)allophone_data_pointers[current_alophone_id][current_alophone_data_index]-0x80;
            current_alophone_data_index++;
            // Escalar para el DAC de 12 bits. Rango de salida del AY es hasta ~1600.
            // sample_8bit * VOICE_NORMALIZATION_FACTOR será el valor escalado.
            return (int16_t)(sample_8bit * VOICE_NORMALIZATION_FACTOR);
        } else {
            // Alófono actual terminado, cargar el siguiente
            load_next_alophone();
            // Si no hay más alófonos, se detendrá, de lo contrario, la primera muestra del nuevo alófono.
            if (!voice_is_playing) {
                return 0;
            }
            // Recursivamente llamar para obtener la primera muestra del nuevo alófono
            return voice_synthesizer_get_sample();
        }
    }
    // Si no es el momento de avanzar la muestra del alófono, devolver la muestra actual repetida.
    // Esto es si ya hemos avanzado en current_alophone_data_index en una llamada anterior
    // y no es el momento de cargar una nueva muestra del alófono (debido al upsampling).
    if (current_alophone_id != -1 && current_alophone_data_index > 0) {
        int8_t sample_8bit = (int8_t)allophone_data_pointers[current_alophone_id][current_alophone_data_index - 1];
        return (int16_t)(sample_8bit * VOICE_NORMALIZATION_FACTOR);
    }
    return 0;
}


void voice_synthesizer_stop() {
    voice_is_playing = false;
    current_alophone_sequence_hex = NULL;
    current_alophone_sequence = NULL;
    current_alophone_index_in_sequence = 0;
    current_alophone_data_index = 0;
    current_alophone_id = -1;
    voice_sample_counter = 0;
}

// Esta función es llamada en cada ISR para avanzar el contador de muestras de voz
void voice_synthesizer_update() {
    if (voice_is_playing) {
        voice_sample_counter++;
    }
}

// esta funcion es llamada por la rutina de interrupcion del mixer devolviendo el valor de salida del sintetizador de voz
uint16_t voice_synthesizer_isr(){
    if (voice_is_playing){
      voice_synthesizer_update();
      int16_t voice_sample = voice_synthesizer_get_sample();
      // Normalizar la muestra de voz (rango original +/-1524) a un rango más pequeño (e.g. +/- 1000)
      const float VOICE_OUTPUT_SCALING = 1000.0f / (VOICE_NORMALIZATION_FACTOR * 127.0f); // Scale voice to +/- 1000
      int16_t scaled_voice_sample = (int16_t)(voice_sample * VOICE_OUTPUT_SCALING);
    }
    else return 0;
}