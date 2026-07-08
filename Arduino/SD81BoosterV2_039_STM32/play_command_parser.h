#ifndef PLAY_COMMAND_PARSER_H
#define PLAY_COMMAND_PARSER_H

#include <stdint.h> // Para uint8_t, uint16_t
#include <Arduino.h> // Para millis() y Serial, si no se usan directamente en el .h, no es estrictamente necesario aquí.

// Define la profundidad máxima de anidamiento de bucles.
// Un valor de 4 significa que puedes tener hasta 4 bucles anidados: (((X)))
#define MAX_PLAY_LOOP_DEPTH 4 

// Estructura para almacenar el estado de un bucle (loop)
typedef struct {
    uint16_t start_index; // Índice en la cadena donde comienza el contenido del bucle
    int repeat_count;     // Cuántas veces más debe repetirse el bucle (0 para 1 pasada, -1 para infinito)
} LoopState;

// Estructura para almacenar el estado de reproducción de cada canal (A, B, C)
typedef struct {
    const char* command_string; // Puntero a la cadena de comandos PLAY de este canal
    uint16_t current_index;     // Índice actual de lectura en command_string

    // Este puntero es crucial para la nueva funcionalidad de bucle infinito ')' sin '('
    // Apunta al inicio absoluto de la cadena command_string.
    const char* initial_string_ptr; 

    uint8_t current_play_octave; // Octava actual para las notas (0-8)
    uint8_t current_play_volume; // Volumen actual para las notas (0-15)
    uint8_t current_note_duration_scale; // Escala de duración de la nota (e.g., 3=corchea, 5=negra, 7=blanca)
    uint8_t current_triplet_duration_scale; // Escala de duración de la nota (e.g., 3=corchea, 5=negra, 7=blanca)
    uint8_t current_triplet_cnt = 0; // Escala de duración de la nota (e.g., 3=corchea, 5=negra, 7=blanca)

    bool is_playing_note;       // True si el canal está sonando una nota, false si está en silencio
    unsigned long next_event_time_ms; // Marca de tiempo (millis()) para el próximo evento de este canal

    // Pila para manejar bucles anidados
    LoopState loop_stack[MAX_PLAY_LOOP_DEPTH];
    uint8_t loop_stack_ptr;     // Puntero a la cima de la pila de bucles (0 si la pila está vacía)

    // Control de la envolvente de hardware del AY
    bool use_hardware_envelope; // True si este canal está configurado para usar la envolvente de hardware
} PlayChannelState;

// Estructura para almacenar el estado global del parser PLAY
typedef struct {
    bool is_playing;             // True si la reproducción está activa, false si está detenida
    uint8_t current_tempo_bpm;   // Tempo global en BPM (beats por minuto)
    unsigned long next_global_event_time_ms; // El momento más temprano en que cualquier canal necesita una actualización

    // Estado global de la envolvente de hardware del chip AY
    uint16_t ay_envelope_period; // Periodo de la envolvente (para registros AY R11/R12)
    uint8_t ay_envelope_shape;   // Forma de la envolvente (para registro AY R13)

    PlayChannelState channels[3]; // Array de estados para los 3 canales de audio (A, B, C)
} PlayParserState;

// --- Declaraciones de funciones públicas del parser ---

// Inicializa el parser con las cadenas de comandos para cada uno de los 3 canales.
// Debe llamarse una vez al inicio para comenzar la reproducción.
uint8_t play_parser_start(const char* command_A, const char* command_B, const char* command_C);

// Actualiza el estado del parser y los registros del emulador AY.
// Debe llamarse repetidamente y tan a menudo como sea posible dentro de la función loop() de Arduino.
// Retorna true si la música aún se está reproduciendo, false si ha terminado o ha sido detenida.
bool play_parser_update();

// Detiene inmediatamente cualquier reproducción en curso y silencia todos los canales.
void play_parser_stop();

// --- Declaraciones de funciones y variables externas (del módulo ay_emulator) ---
// Estas son necesarias para que el parser pueda interactuar con el emulador AY.
// Se asume que ay_emulator.h las define o son globales.

// Array de registros del chip AY que el parser actualiza
extern uint8_t ay_registers[16]; 

// Función para aplicar los cambios en ay_registers al hardware emulado
extern void ay_emulator_apply_register_changes(); 

// Función para calcular el período del AY a partir de una frecuencia en Hz
extern uint16_t ay_emulator_get_period_from_freq(float freq_hz); 

#endif // PLAY_COMMAND_PARSER_H