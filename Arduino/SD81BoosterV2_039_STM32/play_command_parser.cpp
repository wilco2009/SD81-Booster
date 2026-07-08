#define debug
#include "play_command_parser.h"
#include <string.h> 
#include <stdlib.h> 
#include <math.h>   
#include <climits>  
#include "GLOBALS.h"

// --- Constantes para el parseo del comando PLAY ---
const float C0_FREQ = 16.351597f; // C0 ~16.35 Hz

const float NOTE_SEMITONE_FREQS[12] = {
    1.000000f, // C
    1.059463f, // C#
    1.122462f, // D
    1.189207f, // D#
    1.259921f, // E
    1.334840f, // F
    1.414214f, // F#
    1.498307f, // G
    1.587401f, // G#
    1.681793f, // A
    1.781797f, // A#
    1.887749f  // B
};

unsigned long quarter_note_duration_ms;

const uint8_t note_duration [10] = {
    2,              // 1: semicorchea = 2*(1/2 semicorchea)
    3,              // 2: semicorchea + puntillo = 3 * (1/2 semicorchea)
    4,              // 3: corchea = 1/2 negra = 4 * (1/2 semicorchea)
    6,              // 4: Corchea + 1/2 Corchea = 6*(1/2 semicorchea)
    8,              // 5: Negra = 2*corchea = 8*(1/2 semicorchea)
    12,             // 6: Negra + 1/2 negra = 12*(1/2 semicorchea)
    16,             // 7: Blanca = 2*negra = 16*(1/2 semicorchea)
    24,             // 8: Blanca + 1/2 blanca = 24*(1/2 semicorchea)
    32              // 9: 4 Negras = 32*(1/2 semicorchea)
};

// duracion de los tresillos = tres notas en el tiempo de dos
const uint8_t triplet_duration [3] = {
    4,              // 10: tresillo de semicorcheas = 2*semicorchea/3
    8,              // 11: tresillo de corcheas = 2*corchea/3
    16              // 12: tresillo de negras = 2*negra/3
};

// --- Estado global para el parser PLAY ---
static PlayParserState parser_state;

// --- Funciones auxiliares internas ---

// Silencia todos los canales AY (y desactiva envolventes si estaban activas)
static void mute_all_ay_channels() {
    ay_registers[8] = 0; // Volumen A = 0
    ay_registers[9] = 0; // Volumen B = 0
    ay_registers[10] = 0; // Volumen C = 0
    ay_registers[11] = 0; // Periodo envolvente bajo
    ay_registers[12] = 0; // Periodo envolvente alto
    ay_registers[13] = 0; // Tipo envolvente (desactivado)
    ay_registers[7] = 0b00111111; // Deshabilita todos los generadores de tono y ruido
    ay_emulator_apply_register_changes();
}

inline unsigned long calc_note_duration(uint8_t note_scale, uint8_t triplet_scale, uint8_t triplet_cnt){
unsigned long ms;
    // Serial.print("Triplet Scale: "); Serial.print(triplet_scale); Serial.print(" = "); Serial.println(triplet_duration[triplet_scale-1]);
    // Serial.print("Triplet counter: "); Serial.println(triplet_cnt);
    if (triplet_cnt<=0)      // ejecutando un tresillo
        ms = (quarter_note_duration_ms ) * note_duration[note_scale-1] / 8UL;
    else {
        ms = (quarter_note_duration_ms ) * (unsigned long) triplet_duration[triplet_scale-1] / 48UL; // Tresillos
        // Serial.print("Triplet Scale: "); Serial.print(triplet_scale); Serial.print(" = "); Serial.println(triplet_duration[triplet_scale-1]);
    }
    if (ms == 0) ms = 1;
    return ms;
}

// Procesa el siguiente comando significativo (nota/silencio/control) para un canal.
// Retorna true si se procesó un comando y el canal AUN TIENE ALGO PENDIENTE DE HACER (más comandos o esperando tiempo),
// false si el canal ha TERMINADO COMPLETAMENTE.
static bool process_channel_event(uint8_t channel_idx) {
    PlayChannelState* channel = &parser_state.channels[channel_idx];
    
    // Si el canal ya terminó de procesar su string y no hay bucles pendientes, entonces ha terminado
    if (channel->command_string[channel->current_index] == '\0' && channel->loop_stack_ptr == 0) {
        return false; 
    }

    char cmd_char;
    int value;
    bool sharp = false;
    bool flat = false;

    quarter_note_duration_ms = (60000UL / parser_state.current_tempo_bpm);
    unsigned long event_duration_ms = calc_note_duration(channel->current_note_duration_scale,channel->current_triplet_duration_scale,channel->current_triplet_cnt);
    

   // El bucle interno procesa comandos de configuración y saltos de bucle sin demora.
    // Sale cuando encuentra una nota/silencio (que tiene duración), o cuando el canal ha terminado.
    while (true) { 
        quarter_note_duration_ms = (60000UL / parser_state.current_tempo_bpm);
        event_duration_ms = calc_note_duration(channel->current_note_duration_scale,channel->current_triplet_duration_scale,channel->current_triplet_cnt);
#ifdef debug
    Serial.print("  [CH"); Serial.print((char)('A' + channel_idx));
    Serial.print("] Calc: QND: "); Serial.print(quarter_note_duration_ms);
    Serial.print("ms, Scale: "); Serial.print(channel->current_note_duration_scale);
    Serial.print(", Event Duration: "); Serial.print(event_duration_ms); Serial.println("ms");
#endif
        // Si el índice llegó al final de la cadena, y hay un bucle activo, manejamos el bucle implícito.
        if (channel->command_string[channel->current_index] == '\0') {
             if (channel->loop_stack_ptr > 0) {
                 // Accedemos al loop actual en la cima de la pila
                 LoopState* current_loop = &channel->loop_stack[channel->loop_stack_ptr - 1];

                 if (current_loop->repeat_count == -1) { // Bucle indefinido
                     channel->current_index = current_loop->start_index;
                     Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Loop implicit (infinite) repeat.");
                     channel->next_event_time_ms = millis(); // Asegura procesamiento inmediato al volver al inicio
                 } else { // Bucle con repeticiones finitas
                     current_loop->repeat_count--; 
                     if (current_loop->repeat_count >= 0) { // Si quedan repeticiones (incluida la última)
                         channel->current_index = current_loop->start_index;
#ifdef debug
                         Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Loop implicit repeat. Remaining: "); Serial.println(current_loop->repeat_count + 1);
#endif
                         channel->next_event_time_ms = millis(); // Asegura procesamiento inmediato al volver al inicio
                     } else { // El bucle ha terminado (contador es -1)
#ifdef debug
                         Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Loop implicit end.");
#endif
                         channel->loop_stack_ptr--; // Sacar de la pila (solo cuando ha terminado)
                     }
                 }
                 continue; 
             } else {
                 return false; // No hay más comandos ni bucles pendientes, el canal ha terminado
             }
        }
        
        cmd_char = channel->command_string[channel->current_index];
        
        if (cmd_char == ' ' || cmd_char == '\t') {
            channel->current_index++; 
            continue;
        }

        // --- Comandos que requieren una duración de espera (notas y silencios) ---
        if (strchr("CcDdEeFfGgAaBb&", cmd_char) != NULL) {
            channel->current_index++; 
            uint8_t current_ay_volume_reg_val = 0; // Valor a escribir en R8/9/10

            // Si el canal usa la envolvente de hardware
            if (channel->use_hardware_envelope) {
                current_ay_volume_reg_val = 0x10; // Bit 4 a 1, bits 0-3 a 0. Volumen controlado por envolvente.
            } else {
                current_ay_volume_reg_val = channel->current_play_volume; // Volumen controlado por V-command.
            }

            switch (cmd_char) {
                case 'C': case 'c':
                case 'D': case 'd':
                case 'E': case 'e':
                case 'F': case 'f':
                case 'G': case 'g':
                case 'A': case 'a':
                case 'B': case 'b': { 
                    int semitone_index;
                    switch (cmd_char) {
                        case 'C': case 'c': semitone_index = 0; break;
                        case 'D': case 'd': semitone_index = 2; break;
                        case 'E': case 'e': semitone_index = 4; break;
                        case 'F': case 'f': semitone_index = 5; break;
                        case 'G': case 'g': semitone_index = 7; break;
                        case 'A': case 'a': semitone_index = 9; break;
                        case 'B': case 'b': semitone_index = 11; break;
                        default: semitone_index = 0; break; 
                    }

                    // bool sharp = false;
                    // bool flat = false;

                    // if (channel->command_string[channel->current_index] == '#') {
                    //     sharp = true;
                    //     semitone_index++;
                    //     channel->current_index++;
                    // } else if (channel->command_string[channel->current_index] == '$') {
                    //     flat = true;
                    //     semitone_index--;
                    //     channel->current_index++;
                    // }

                    if (sharp) semitone_index++;
                    if (flat) semitone_index--;
                    flat = false; sharp = false;

                    if (semitone_index < 0) semitone_index = 0; 
                    if (semitone_index > 11) semitone_index = 11; 

                    if (channel->current_triplet_cnt > 0) channel->current_triplet_cnt--;

                    
                    uint8_t effective_octave = channel->current_play_octave;
                    if (islower(cmd_char)) effective_octave--;
                    // Ajuste de octava si la nota es C, D, E, F, G, A, B y la octava actual no es la más alta (8)
                    // Este ajuste es común en algunos formatos para notas que "cruzan" la octava inferior del teclado
                    // pero en el contexto MIDI y música, C es el inicio de la octava.
                    // Para un comportamiento más "directo", podrías eliminar este if.
                    // Lo mantengo si es la intención original.
                    if (cmd_char >= 'A' && cmd_char <= 'G' && channel->current_play_octave < 8) { 
                        // Esta lógica puede ser confusa. Generalmente, C4 a B4 están en octava 4.
                        // Si 'A' en O3 es más alto que 'C' en O4, esto puede ser un problema.
                        // Si se refiere a que la nota se eleva a la siguiente octava si se especifica un 'C' pero la melodía está subiendo,
                        // podría ser un comportamiento deseado. Para una interpretación estándar, usualmente 'O' define la octava.
                        // Por ahora, lo dejamos como está en el original.
                    }

                    float note_freq = C0_FREQ * pow(2, effective_octave) * NOTE_SEMITONE_FREQS[semitone_index];
                    uint16_t ay_period = ay_emulator_get_period_from_freq(note_freq);

                    uint8_t ay_register_offset = channel_idx * 2; // R0/R1 para Ch A, R2/R3 para Ch B, R4/R5 para Ch C
                    ay_registers[ay_register_offset] = ay_period & 0xFF;
                    ay_registers[ay_register_offset + 1] = (ay_period >> 8) & 0x0F;
                    ay_registers[8 + channel_idx] = current_ay_volume_reg_val; // *** APLICA VOLUMEN O MODO ENVOLVENTE ***
                    ay_registers[7] &= ~(1 << channel_idx); // Habilita tono para este canal
                    ay_registers[7] |= (1 << (channel_idx + 3)); // Deshabilita ruido para este canal
                    
                    channel->is_playing_note = true;
#ifdef debug
                    Serial.print("  [CH"); Serial.print((char)('A' + channel_idx));
                    Serial.print("] Playing Note: "); Serial.print(cmd_char);
#endif
                    if (sharp) Serial.print("#"); 
                    if (flat) Serial.print("$");
#ifdef debug
                    Serial.print(", Octave: "); Serial.print(effective_octave); 
                    Serial.print(", Freq: "); Serial.print(note_freq); Serial.println(" Hz");
#endif
                    break;
                }
                case '&': // Silencio
                    ay_registers[8 + channel_idx] = 0; // Volumen a 0 (silencio total, incluso si envolvente activa)
                    ay_registers[7] |= (1 << channel_idx); // Deshabilita tono
                    ay_registers[7] |= (1 << (channel_idx + 3)); // Deshabilita ruido
                    channel->is_playing_note = false;
#ifdef debug
                    Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Rest.");
#endif
                    break;
            }
            channel->next_event_time_ms = millis() + event_duration_ms;
            return true; // Se procesó un evento con duración, esperar
        }

        // --- Comandos de configuración (no tienen duración) ---
        switch (cmd_char) {
            case '#':{
                sharp = true;
                channel->current_index++; 
#ifdef debug
                Serial.println("sharp");
#endif
                break;
             }
            case '$':{
                flat = true;
                channel->current_index++; 
#ifdef debug
                Serial.println("flat");
#endif
                break;
             }
            case 'V': case 'v': {
                channel->current_index++; 
                value = atoi(&channel->command_string[channel->current_index]);
                if (value >= 0 && value <= 15) { channel->current_play_volume = value; }
                while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Set Volume to: "); Serial.println(channel->current_play_volume);
#endif
                
                // Si el canal NO está en modo envolvente, aplicar el nuevo volumen inmediatamente
                if (!channel->use_hardware_envelope) {
                    ay_registers[8 + channel_idx] = channel->current_play_volume;
                }
                break;
            }
            case 'O': case 'o':{
                channel->current_index++; 
                value = atoi(&channel->command_string[channel->current_index]);
                if (value >= 0 && value <= 8) { channel->current_play_octave = value; }
                while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Set Octave to: "); Serial.println(channel->current_play_octave);
#endif
                break;
            }
            // Duración de la nota 
            case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
                value = atoi(&channel->command_string[channel->current_index]); 
                // La duración base es una redonda (1), 4 es negra, 8 corchea, etc.
                // Asegurarse que el valor sea un divisor válido de la duración de la redonda.
                // Por ejemplo, 1 (entera), 2 (blanca), 4 (negra), 8 (corchea), 16 (semicorchea), etc.
                // Limitar a valores razonables (ej. 1 a 64)
                if (value >=1 && value < 10) { channel->current_note_duration_scale = value; channel->current_triplet_duration_scale = 0; channel->current_triplet_cnt = 0; } 
                else if (value >=10 && value <= 12) { channel->current_triplet_duration_scale=value-9; channel->current_triplet_cnt = 3; } // tresillos
                while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Set Note Length Scale: "); Serial.println(channel->current_note_duration_scale); 
                Serial.print("Triplet Scale: "); Serial.print(channel->current_triplet_duration_scale); Serial.print(" = "); Serial.println(triplet_duration[channel->current_triplet_duration_scale-1]);
                Serial.print("Triplet counter: "); Serial.println(channel->current_triplet_cnt);
#endif
                break;
            }
            case 'T': case 't':{
                channel->current_index++; 
                value = atoi(&channel->command_string[channel->current_index]);
                if (value >= 60 && value <= 240) { parser_state.current_tempo_bpm = value; }
                while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Set GLOBAL Tempo to: "); Serial.println(parser_state.current_tempo_bpm);
#endif
                break;
            }
            case '!': { // Comentarios de una línea (hasta el siguiente '!' o fin de línea)
                channel->current_index++; 
                while (channel->command_string[channel->current_index] != '\0' && channel->command_string[channel->current_index] != '!') { channel->current_index++; }
                if (channel->command_string[channel->current_index] == '!') channel->current_index++; 
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Comment skipped.");
#endif
                break;
            }
            case 'H': case 'h':{ // Halt (detener la reproducción global)
                channel->current_index++; 
                play_parser_stop();
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Halt command. Global playback stopped.");
#endif
                return false; 
            }
            case 'N': case 'n': { // Separador numérico, ignorar
                channel->current_index++; 
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Numeric separator 'N' skipped.");
#endif
                break;
            }
            case '(': { // Inicio de bucle
                channel->current_index++; 
                if (channel->loop_stack_ptr >= MAX_PLAY_LOOP_DEPTH) {
#ifdef debug
                    Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] ERROR: Loop stack overflow! Ignoring loop.");
#endif
                    // Intentar avanzar hasta el ')' o el final para evitar un bucle roto.
                    // Esto es una simplificación; un parser más robusto buscaría el ')' emparejado.
                    while (channel->command_string[channel->current_index] != '\0' && channel->command_string[channel->current_index] != ')') channel->current_index++;
                    if (channel->command_string[channel->current_index] == ')') channel->current_index++;
                    break;
                }

                int repetitions = 2;
                // int repetitions = 0;
                // // Leer el número de repeticiones si existe
                // if (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') {
                //     repetitions = atoi(&channel->command_string[channel->current_index]);
                //     while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
                // }
                // if (repetitions == 0) repetitions = 2; // Default 2 repeticiones si no se especifica número

                channel->loop_stack[channel->loop_stack_ptr].start_index = channel->current_index; 
                channel->loop_stack[channel->loop_stack_ptr].repeat_count = repetitions - 1; // N-1 repeticiones después de la primera pasada
                
                // Si repetitions es 0 y se convierte en 2, entonces repeat_count es 1.
                // Si repetitions es 1 (solo una pasada), repeat_count es 0 (no más repeticiones).
                // Si queremos que '0' repeticiones signifique infinito (como en algunos formatos), se podría ajustar.
                // Actualmente, repetitions = 0 -> 2 repeticiones.

                channel->loop_stack_ptr++; // Empujar el nuevo bucle a la pila

#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Loop start ("); Serial.print(repetitions); Serial.println(" repetitions).");
#endif
                break; 
            }
            case ')': { // Fin de bucle
                channel->current_index++; 

                if (channel->loop_stack_ptr <= 0) { // Si la pila de bucles está vacía (no hay un '(' coincidente)
                    // NUEVA REGLA: Repetir indefinidamente desde el inicio de la secuencia
#ifdef debug
                    Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Found ')' without matching '('. Looping indefinitely from start.");
#endif
                    channel->current_index = 0; // Reiniciar el puntero al inicio de la cadena (posición 0)
                    // Podemos añadir un estado de "bucle infinito" si el `loop_stack` no está en uso
                    // Para evitar confusiones con la pila de bucles normal, simplemente reiniciamos el índice.
                    // El `play_parser_update` gestionará el flujo continuo.
                    // No es necesario añadir a la pila, solo reiniciar el puntero.
                    // Si se desea un bucle infinito REAL con un ')' suelto, la lógica de 'process_channel_event' al final del string ya se encargaría.
                    // Si el usuario quiere esto como un comando explicito:
                    channel->loop_stack[channel->loop_stack_ptr].start_index = 0; // Marca el inicio del string como el punto de repetición
                    channel->loop_stack[channel->loop_stack_ptr].repeat_count = -1; // -1 para indefinido
                    channel->loop_stack_ptr++; // Empujar a la pila como un bucle "especial"
                    
                    // Asegúrate de que este push no cause un overflow si ya hay bucles anidados válidos.
                    // Si ya estamos en el límite de la pila, esto podría ser un problema.
                    // Una alternativa es no usar la pila para este caso especial de ')' sin '('.
                    // Pero para consistencia con la gestión de bucles, es mejor.
                    if (channel->loop_stack_ptr >= MAX_PLAY_LOOP_DEPTH) { // Si se desborda, no se activa el bucle infinito
#ifdef debug
                         Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] ERROR: Loop stack overflow on infinite loop ')'! Playback will stop.");
#endif
                         return false;
                    }

                } else { // Hay un bucle activo en la pila
                    LoopState* current_loop = &channel->loop_stack[channel->loop_stack_ptr - 1];

                    if (current_loop->repeat_count == -1) { // Si el bucle actual es indefinido (de un '(...)' de -1 repeticiones)
                        channel->current_index = current_loop->start_index;
#ifdef debug
                        Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Loop (infinite) repeat."); 
#endif
                    } else { // Bucle con repeticiones finitas
                        current_loop->repeat_count--; 
                        if (current_loop->repeat_count >= 0) { 
                            channel->current_index = current_loop->start_index;
#ifdef debug
                            Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Loop repeat. Remaining: "); Serial.println(current_loop->repeat_count + 1); 
#endif
                        } else { // El bucle ha terminado (contador es -1)
#ifdef debug
                            Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Loop end.");
#endif
                            channel->loop_stack_ptr--; // Sacar de la pila
                        }
                    }
                }
                channel->next_event_time_ms = millis(); // Asegura procesamiento inmediato al volver al inicio del bucle/cadena
                break; 
            }
            
            // --- NUEVOS COMANDOS DE ENVOLVENTE (W, U, X) ---
            case 'W': case 'w': { // Activar envolvente de hardware para este canal
                channel->current_index++;
                value = atoi(&channel->command_string[channel->current_index]);
                if (value >= 0 && value <= 15) {
                    parser_state.ay_envelope_shape = value;
                    ay_registers[13] = parser_state.ay_envelope_shape; // Aplica la forma de envolvente global
                }
                while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
                
                channel->use_hardware_envelope = true;
                ay_registers[8 + channel_idx] = 0x10; // Set bit 4 to 1, use hardware envelope for this channel
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Use Hardware Envelope. Shape: "); Serial.println(parser_state.ay_envelope_shape);
#endif
                break;
            }
            case 'U': case  'u': { // Desactivar envolvente de hardware para este canal
                channel->current_index++;
                channel->use_hardware_envelope = false;
                ay_registers[8 + channel_idx] = channel->current_play_volume; // Back to software volume control
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.println("] Disable Hardware Envelope. Back to V-volume.");
#endif
                break;
            }
            case 'X': case 'x': { // Setear periodo de envolvente (GLOBAL)
                channel->current_index++;
                value = atoi(&channel->command_string[channel->current_index]);
                if (value >= 0 && value <= 65535) { // Periodo de 16 bits
                    parser_state.ay_envelope_period = value;
                    ay_registers[11] = parser_state.ay_envelope_period & 0xFF;
                    ay_registers[12] = (parser_state.ay_envelope_period >> 8) & 0xFF;
                }
                while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
#ifdef debug
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Set Global Envelope Period to: "); Serial.println(parser_state.ay_envelope_period);
#endif
                break;
            }
            // --- FIN NUEVOS COMANDOS ---

            // Comandos no implementados o ignorados, pero que avanzan el índice
            case 'M': case'm': 
            case '_':
            case 'Y': case 'y': case 'Z': case 'z':
                channel->current_index++; 
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Command '"); Serial.print(cmd_char); Serial.println("' skipped.");
                if (strchr("MWUXY", cmd_char) != NULL) { 
                    while (channel->command_string[channel->current_index] >= '0' && channel->command_string[channel->current_index] <= '9') channel->current_index++;
                }
                break;
            default:
                Serial.print("  [CH"); Serial.print((char)('A' + channel_idx)); Serial.print("] Unknown/unhandled command: '"); Serial.print(cmd_char); Serial.println("'");
                channel->current_index++; 
                break;
        } // Fin del switch
        // Si no se procesó un comando con duración, continuar el bucle para procesar el siguiente comando sin demora
    } // Fin del while(true)
    return false; // Si llegamos aquí, el canal ha terminado (o hubo un error)
}


// Inicializa el parser con las cadenas de comando para cada canal
uint8_t play_parser_start(const char* command_A, const char* command_B, const char* command_C) {
    Serial.printf("Starting PLAY command parsing (3 voices)...\n\r");
    Serial.printf("A=%s, B=%s, C=%s \n\r",command_A,command_B,command_C);

    parser_state.is_playing = true;
    parser_state.current_tempo_bpm = 120; // Tempo global por defecto
    parser_state.ay_envelope_period = 0; // Inicializar periodo de envolvente
    parser_state.ay_envelope_shape = 0;  // Inicializar forma de envolvente (desactivada)


    // Inicializar el estado de cada canal
    for (int i = 0; i < 3; ++i) {
        parser_state.channels[i].current_index = 0;
        parser_state.channels[i].current_play_octave = 4;
        parser_state.channels[i].current_play_volume = 15;
        parser_state.channels[i].current_note_duration_scale = 4;
        parser_state.channels[i].current_triplet_duration_scale = 0;
        parser_state.channels[i].is_playing_note = false; 
        parser_state.channels[i].next_event_time_ms = millis(); 
        parser_state.channels[i].loop_stack_ptr = 0; 
        parser_state.channels[i].use_hardware_envelope = false; // No usar envolvente por defecto

        // Asignar las cadenas de comando y guardar el puntero inicial absoluto
        if (i == 0) parser_state.channels[i].command_string = command_A;
        else if (i == 1) parser_state.channels[i].command_string = command_B;
        else parser_state.channels[i].command_string = command_C;
        
        // Es crucial que el 'initial_string_ptr' sea el puntero al inicio original de la cadena.
        // Esto es necesario para el bucle infinito con ')' sin '('
        parser_state.channels[i].initial_string_ptr = parser_state.channels[i].command_string;

        Serial.printf("  Channel %c",(char)('A' + i));
        Serial.printf(" command: '%s'",parser_state.channels[i].command_string);
    }

    // Asegurar que todos los canales están en silencio al inicio
    mute_all_ay_channels();
    return 0;       // no error handle here
}

// Actualiza el estado del parser, se debe llamar repetidamente en loop()
bool play_parser_update() {
    if (!parser_state.is_playing) {
        return false;
    }

    unsigned long current_time_ms = millis();
    bool any_channel_still_active_or_playing = false; 
    unsigned long min_next_event_time = ULONG_MAX; 

    for (int i = 0; i < 3; ++i) {
        PlayChannelState* channel = &parser_state.channels[i];
        
        // Lógica para procesar el siguiente comando si es el momento
        if (current_time_ms >= channel->next_event_time_ms) {
            // Un canal con cadena vacía o que ya terminó de procesar se considera inactivo
            if (channel->command_string[channel->current_index] == '\0' && channel->loop_stack_ptr == 0) {
                 // Canal realmente terminado, silenciarlo si no lo está
                 ay_registers[8 + i] = 0;
                 ay_registers[7] |= (1 << i);       
                 ay_registers[7] |= (1 << (i + 3)); 
                 channel->use_hardware_envelope = false; // Desactivar la bandera de envolvente
            } else {
                bool channel_still_has_work = process_channel_event(i);

                // Si process_channel_event retorna false, el canal ha terminado su cadena y bucles.
                // Asegurarse de silenciarlo.
                if (!channel_still_has_work) {
                    ay_registers[8 + i] = 0;
                    ay_registers[7] |= (1 << i);       
                    ay_registers[7] |= (1 << (i + 3)); 
                    channel->use_hardware_envelope = false; // Desactivar la bandera de envolvente
                }
            }
        }

        // Determinar si este canal contribuye a mantener el parser global activo
        // Un canal está activo si:
        // 1. Todavía tiene comandos por procesar en su cadena.
        // 2. Está esperando por el tiempo de la nota actual (next_event_time_ms > current_time_ms).
        // 3. Tiene bucles pendientes en su stack (loop_stack_ptr > 0).
        // 4. Está en un estado de bucle indefinido (manejado por el ')' sin '(' que reinicia current_index a 0)
        
        // Si el canal tiene más por hacer O si está esperando una duración de nota
        if (channel->command_string[channel->current_index] != '\0' || 
            current_time_ms < channel->next_event_time_ms ||
            channel->loop_stack_ptr > 0) { 
            
            any_channel_still_active_or_playing = true;
            if (channel->next_event_time_ms < min_next_event_time) {
                min_next_event_time = channel->next_event_time_ms;
            }
        }
    }

    // Si NINGÚN canal está activo, detener la reproducción global
    if (!any_channel_still_active_or_playing) {
        Serial.println("All PLAY commands finished and all notes ended for all voices. Global playback stopped.");
        play_parser_stop();
        return false;
    } else {
        parser_state.next_global_event_time_ms = min_next_event_time;
        ay_emulator_apply_register_changes(); // Aplica los cambios a los registros AY
        return true;
    }
}

// Detiene cualquier reproducción en curso y silencia los canales
void play_parser_stop() {
    parser_state.is_playing = false;
    mute_all_ay_channels(); // Mutea y resetea todos los registros AY, incluyendo envolventes.
}