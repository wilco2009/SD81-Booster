#include "VBAT.h"
#include "stm32f4xx_hal.h"
#include "GLOBALS.h"

// Declaración de la estructura de manejo del ADC
ADC_HandleTypeDef hadc1; // Usaremos el ADC1

// Define el voltaje de referencia del ADC en voltios (Vcc del STM32)
// Para el STM32F407, la referencia interna del ADC es VDD, que suele ser 3.3V.
const float ADC_REFERENCE_VOLTAGE = 3.3;

// Constante para el valor máximo (escala completa) de la lectura RAW del ADC de 12 bits.
// Un ADC de 12 bits tiene 2^12 = 4096 valores posibles (de 0 a 4095).
// el valor del voltaje debe estar entre 1.8V y 3.6V
// probaremos a poner 0%=2V y 100%>=3.3V
const float ADC_MAX_RAW_VALUE = 4096.0;

// Si estás leyendo una batería real con un divisor de voltaje,
// este factor debe ser el inverso del factor de división de tu divisor.
// Por ejemplo, si tu divisor reduce el voltaje de la batería a la mitad,
// este factor sería 2.0. Si no usas divisor y el voltaje es directamente
// medible por el ADC (<= 3.3V), este factor sería 1.0.
// AJUSTA ESTE VALOR SEGÚN TU DIVISOR DE VOLTAJE EXTERNO
const float EXTERNAL_VOLTAGE_DIVIDER_FACTOR = 1.0; // Cambia esto si usas un divisor

bool vbat_init(void){
  log_3("ℹ️ Starting external voltage reading....");

  // Desinicializar el ADC para asegurar un estado limpio antes de la configuración
  HAL_ADC_DeInit(&hadc1);

  // 1. Habilitar el reloj del ADC1
  __HAL_RCC_ADC1_CLK_ENABLE();

  // 2. Habilitar el reloj del puerto GPIOC (para PC1)
  __HAL_RCC_GPIOC_CLK_ENABLE(); // ¡NUEVO! Habilitamos el reloj del puerto C

  // 3. Configurar el pin PC1 como entrada analógica
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_1; // Usando el pin PC1
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL; // Es crucial para entradas analógicas
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct); // Inicializar el GPIOC

  // 4. Configurar la estructura de manejo del ADC
  hadc1.Instance = ADC1;
  // El prescaler del reloj del ADC debe ser tal que el reloj de entrada al ADC sea <= 36MHz.
  // Con PCLK2 (APB2) a 90MHz (común en F407), DIV4 da 22.5MHz, que es válido.
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B; // Resolución de 12 bits
  hadc1.Init.ScanConvMode = DISABLE; // Solo un canal
  hadc1.Init.ContinuousConvMode = DISABLE; // Conversión simple
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE; // Sin trigger externo
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT; // Alineación de datos a la derecha
  hadc1.Init.NbrOfConversion = 1; // Solo una conversión
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV; // Evento de fin de conversión para una sola conversión

  if (HAL_ADC_Init(&hadc1) != HAL_OK) {
    log_0("❌ ERROR: Failed to initialize ADC.");
    return false;
  }
  log_3("✅ ADC1 initialized correctly.");

  // 5. Configurar el canal ADC_CHANNEL_11 para PC1
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_CHANNEL_11; // ¡CAMBIO CLAVE! Canal correspondiente a PC1
  sConfig.Rank = 1; // Es la primera (y única) conversión en la secuencia
  // Usar un tiempo de muestreo alto (como 480 ciclos) mejora la precisión para canales de alta impedancia.
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
    log_0("❌ ERROR: Failed to configure external ADC channel (PC1).");
    return false;
  }
  log_3("✅ External ADC channel (PC1) configured.");

  // ¡IMPORTANTE! Para un pin externo, NO uses ADC->CCR |= ADC_CCR_TSVREFE;
  // Ese bit es solo para habilitar los canales internos (sensor de temperatura, VREFINT, VBAT).
  // Para PC1, no es necesario ni apropiado.

  return true;
}

float get_battery_level(void){
  uint32_t adc_raw_value;
  float battery_voltage;
  HAL_StatusTypeDef poll_status; // Variable para capturar el estado del poll

  // 1. Iniciar la conversión ADC
  HAL_ADC_Start(&hadc1);

  // 2. Esperar a que la conversión termine, capturando el estado
  poll_status = HAL_ADC_PollForConversion(&hadc1, 100); // Espera hasta 100ms

  // 3. Leer el valor digital de la conversión y manejar errores
  if (poll_status == HAL_OK) { // Si la conversión fue exitosa
    adc_raw_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1); // Detener el ADC después de leer
  } else {
    // Si hubo un error o timeout
    log_0("❌ Error: ADC conversion not completed. Poll status: ");
    if (poll_status == HAL_TIMEOUT) {
      log_0("❌ Timeout (HAL_TIMEOUT).");
    } else if (poll_status == HAL_ERROR) {
      log_0("❌ General error (HAL_ERROR). Code: %d \n\r",hadc1.ErrorCode);
   } else {
      log_0("❌ Unknown status.\n\r");
    }
    HAL_ADC_Stop(&hadc1); // Asegúrate de detener el ADC incluso si hay un error
    delay(1000); // Pequeña pausa antes de reintentar
    return 0; // Salir del loop para evitar cálculos con datos erróneos
  }

  // 4. Convertir el valor ADC a voltaje real de la batería
  // Fórmula: V_BATERIA = (Valor_ADC_Crudo / ADC_MAX_RAW_VALUE) * Voltaje_Referencia_ADC * Factor_Divisor_Externo
  battery_voltage = (float)adc_raw_value * (ADC_REFERENCE_VOLTAGE / ADC_MAX_RAW_VALUE) * EXTERNAL_VOLTAGE_DIVIDER_FACTOR;

  //log_3("Raw ADC Reading (PC1): %d  -> Battery Voltage: %f V\n\r",adc_raw_value,battery_voltage);
  return battery_voltage;

}

uint8_t get_battery_byte(void){
  float bat = (get_battery_level()*255.0/3.3);
  return (uint8_t) bat;
}
