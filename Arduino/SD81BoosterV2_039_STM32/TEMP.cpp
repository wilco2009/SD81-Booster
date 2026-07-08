#include "TEMP.h"
#include "Arduino.h"
#include "stdint.h"

// El sensor de temperatura interno está conectado al Canal 16 del ADC1
//#define TEMP_SENSOR_CHANNEL ADC_CHANNEL_TEMPSENSOR
#define ADC_TEMP_CHANNEL 16
#define ADC_MAX_VALUE 4095.0f // El ADC del STM32F4 es de 12 bits (2^12 - 1)

uint16_t getInternalTemperatureADCValue() {
    ADC_TypeDef* ADCx = ADC1;

    // --- 1. FORZAR ACTIVACIÓN DEL RELOJ DEL ADC1 (CRÍTICO) ---
    // El bit 8 del RCC->APB2ENR habilita el reloj para ADC1
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; 
    
    // 2. Activar el sensor de temperatura y VREFINT (registro común)
    ADC->CCR |= ADC_CCR_TSVREFE; 
    delay(1); 

    // 3. Apagar el ADC para asegurar un estado limpio
    ADCx->CR2 &= ~ADC_CR2_ADON; 
    
    // 4. Limpiar la bandera de Status
    ADCx->SR = 0; 
    
    // 5. Configurar el tiempo de muestreo (480 ciclos)
    ADCx->SMPR1 |= (7U << 18U); 

    // 6. Configurar la secuencia para el Canal 16
    ADCx->SQR1 &= ~ADC_SQR1_L;           
    ADCx->SQR3 = ADC_TEMP_CHANNEL;       
    
    // 7. Encender el ADC (ADON)
    ADCx->CR2 |= ADC_CR2_ADON;
    delay(1); 

    // 8. Iniciar la conversión (Software Start)
    ADCx->CR2 |= ADC_CR2_SWSTART;

    // 9. Esperar a que la conversión termine con un timeout de seguridad
    uint32_t timeout = 500000; 
    while (!(ADCx->SR & ADC_SR_EOC)) {
        if (timeout-- == 0) {
            ADCx->CR2 &= ~ADC_CR2_ADON;
            return 9999; 
        }
    }

    // 10. Devolver el resultado
    return ADCx->DR;
}

// --- Constantes de Calibración del STM32F4 (Obtenidas del datasheet) ---
// Estos valores están guardados en la ROM del chip para la calibración:

// Valor de la lectura del ADC a 30°C
#define TS_CAL1_ADDR ((uint16_t*)0x1FFF7A2C)
float TS_CAL1_VALUE = 0.0f; // Se leerá en setup()

// Valor de la lectura del ADC a 110°C
#define TS_CAL2_ADDR ((uint16_t*)0x1FFF7A2E)
float TS_CAL2_VALUE = 0.0f; // Se leerá en setup()

// Frecuencia de la fuente de voltaje de referencia (referencia interna VREFINT)
#define VREFINT_CAL_ADDR ((uint16_t*)0x1FFF7A2A)
float VREFINT_CAL_VALUE = 0.0f; // Se leerá en setup()

// Voltaje de referencia interna, valor fijo a 3.0V
#define VREF_VOLTAGE 3.0f

#define ADC_TEMP_CHANNEL_NUM 16

void showTemp(){
// Leer los valores de calibración de la ROM del microcontrolador
  // Son necesarios para la fórmula de linealización de la temperatura.
  TS_CAL1_VALUE = (float)*TS_CAL1_ADDR;
  TS_CAL2_VALUE = (float)*TS_CAL2_ADDR;
  VREFINT_CAL_VALUE = (float)*VREFINT_CAL_ADDR;

  // Inicializar el ADC y configurar el canal del sensor de temperatura
  analogReadResolution(12);
  analogReference(AR_DEFAULT);
  
  // Imprimir los valores de calibración (opcional, para depuración)
  // Serial.println("--- Inicializacion del Sensor de Temperatura ---");
  // Serial.print("TS_CAL1 (30C): "); Serial.println(TS_CAL1_VALUE);
  // Serial.print("TS_CAL2 (110C): "); Serial.println(TS_CAL2_VALUE);
  // Serial.print("VREFINT_CAL: "); Serial.println(VREFINT_CAL_VALUE);
  // Serial.println("----------------------------------------------");

uint16_t temp_adc_reading_raw = getInternalTemperatureADCValue();
    float temp_adc_reading = (float)temp_adc_reading_raw;
    
    // El cálculo del Vdda se omite para simplificar, 
    // asumiendo que Vdda es estable, ya que no se necesita VREFINT para la fórmula lineal.

    // 1. Aplicar la fórmula de calibración (Linealización)
    // T = 30 + ( (Lectura - TS_CAL1) * 80 ) / (TS_CAL2 - TS_CAL1)
    float temperature_celsius = 
        30.0f + ((temp_adc_reading - TS_CAL1_VALUE) * 80.0f) / (TS_CAL2_VALUE - TS_CAL1_VALUE);

    // 2. Imprimir los resultados
    // Serial.print("Lectura cruda ADC (0-4095): ");
    // Serial.print(temp_adc_reading_raw);
    // Serial.print("  -> ");
    Serial.print("ℹ️ Temperatura Interna: ");
    Serial.print(temperature_celsius, 2); 
    Serial.println(" °C");
    
}
