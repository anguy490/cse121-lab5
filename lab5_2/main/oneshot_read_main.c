#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#define TAG "LAB5_2"

// ADC config: ADC1 channel 4 = GPIO4
#define ADC_UNIT_ID   ADC_UNIT_1
#define ADC_CHANNEL   ADC_CHANNEL_4   // GPIO4
#define SAMPLE_MS     10              

// Morse timing
#define UNIT_MS        200
#define DOT_MAX        (UNIT_MS * 2)   // 200 ms
#define LETTER_GAP_MIN (UNIT_MS * 2)   // 200 ms
#define LETTER_GAP_MAX (UNIT_MS * 5)   // 500 ms
#define WORD_GAP_MIN   (UNIT_MS * 5)   // 500 ms



typedef struct {
    const char *morse;
    char letter;
} morse_map_t;

static const morse_map_t morse_table[] = {
    {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},
    {".", 'E'},    {"..-.", 'F'}, {"--.", 'G'},  {"....", 'H'},
    {"..", 'I'},   {".---", 'J'}, {"-.-", 'K'},  {".-..", 'L'},
    {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},  {".--.", 'P'},
    {"--.-", 'Q'}, {".-.", 'R'},  {"...", 'S'},  {"-", 'T'},
    {"..-", 'U'},  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
    {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'},
    {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'},
    {"---..", '8'}, {"----.", '9'},
};

static char morse_to_char(const char *code)
{
    for (size_t i = 0; i < sizeof(morse_table) / sizeof(morse_table[0]); i++) {
        if (strcmp(morse_table[i].morse, code) == 0) {
            return morse_table[i].letter;
        }
    }
    return '?';
}

void app_main(void)
{
    // -----------------------------
    // ADC oneshot setup
    // -----------------------------
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_ID,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,   // use 12 dB on ESP32-C3 style chips
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg));

    // -----------------------------
    // Ambient calibration
    // -----------------------------
    ESP_LOGI(TAG, "not ready...");
    int ambient_sum = 0;
    const int ambient_samples = 64;

    for (int i = 0; i < ambient_samples; i++) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw));
        ambient_sum += raw;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    int ambient = ambient_sum / ambient_samples;
    int threshold = ambient + 200;

    ESP_LOGI(TAG, "Ambient ~ %d, threshold ~ %d", ambient, threshold);
    ESP_LOGI(TAG, "ready");

    // -----------------------------
    // Morse decode state
    // -----------------------------
    bool prev_on = false;
    int state_duration_ms = 0; 
    int gap_ms = 0;            

    char morse_buf[16];
    int morse_len = 0;

    while (1) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ADC read error: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
            continue;
        }

        bool on = (raw > threshold);

        // debugger if needed:
        // ESP_LOGI(TAG, "raw=%d on=%d", raw, (int)on);

        if (on == prev_on) {
            state_duration_ms += SAMPLE_MS;
        } else {
            if (prev_on) {
                // was ON, now OFF: dot or dash
                if (state_duration_ms < DOT_MAX) {
                    if (morse_len < (int)sizeof(morse_buf) - 1) {
                        morse_buf[morse_len++] = '.';
                        morse_buf[morse_len] = '\0';
                    }
                    ESP_LOGD(TAG, "dot . (dur=%dms)", state_duration_ms);
                } else {
                    if (morse_len < (int)sizeof(morse_buf) - 1) {
                        morse_buf[morse_len++] = '-';
                        morse_buf[morse_len] = '\0';
                    }
                    ESP_LOGD(TAG, "dash - (dur=%dms)", state_duration_ms);
                }
                gap_ms = 0;
            } else {
                // was OFF, now ON: check gap for letter/word boundary
                ESP_LOGD(TAG, "gap=%d ms", gap_ms);

                if (gap_ms >= LETTER_GAP_MIN && gap_ms < LETTER_GAP_MAX) {
                    if (morse_len > 0) {
                        char c = morse_to_char(morse_buf);
                        printf("%c", c);
                        fflush(stdout);
                        ESP_LOGI(TAG, "Letter: %s -> %c", morse_buf, c);
                        morse_len = 0;
                    }
                } else if (gap_ms >= WORD_GAP_MIN) {
                    if (morse_len > 0) {
                        char c = morse_to_char(morse_buf);
                        printf("%c", c);
                        morse_len = 0;
                    }
                    printf(" ");
                    fflush(stdout);
                    ESP_LOGI(TAG, "Word gap");
                }
            }

            prev_on = on;
            state_duration_ms = SAMPLE_MS;
        }

        if (!on) {
            gap_ms += SAMPLE_MS;
        }

        // Extra flush if it's been OFF forever with leftover bits
        if (!on && gap_ms > WORD_GAP_MIN * 2 && morse_len > 0) {
            char c = morse_to_char(morse_buf);
            printf("%c ", c);
            fflush(stdout);
            ESP_LOGI(TAG, "Flush letter at long OFF: %s -> %c", morse_buf, c);
            morse_len = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
}
