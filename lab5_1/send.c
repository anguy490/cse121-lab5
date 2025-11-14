// lab5_1/send.c
// Usage: ./send <repetitions> "message"
// Example: ./send 4 "hello ESP32"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wiringPi.h>

#define LED_PIN   17      // BCM GPIO pin connected to LED + resistor
#define DOT_MS    200     // dot duration in milliseconds

typedef struct {
    char ch;
    const char *morse;
} morse_t;

static const morse_t MORSE_TABLE[] = {
    {'A', ".-"},   {'B', "-..."},
    {'C', "-.-."}, {'D', "-.."},
    {'E', "."},    {'F', "..-."},
    {'G', "--."},  {'H', "...."},
    {'I', ".."},   {'J', ".---"},
    {'K', "-.-"},  {'L', ".-.."},
    {'M', "--"},   {'N', "-."},
    {'O', "---"},  {'P', ".--."},
    {'Q', "--.-"}, {'R', ".-."},
    {'S', "..."},  {'T', "-"},
    {'U', "..-"},  {'V', "...-"},
    {'W', ".--"},  {'X', "-..-"},
    {'Y', "-.--"}, {'Z', "--.."},

    {'0', "-----"}, {'1', ".----"},
    {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."},
    {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},
};

static const int MORSE_TABLE_LEN = sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]);

static void led_on(void)
{
    digitalWrite(LED_PIN, HIGH);
}

static void led_off(void)
{
    digitalWrite(LED_PIN, LOW);
}

// transmit a single dot: LED on for 1 dot, off for 1 dot
static void send_dot(void)
{
    led_on();
    delay(DOT_MS);
    led_off();
    delay(DOT_MS);
}

// transmit a single dash: LED on for 3 dots, off for 1 dot
static void send_dash(void)
{
    led_on();
    delay(3 * DOT_MS);
    led_off();
    delay(DOT_MS);
}

static const char *char_to_morse(char c)
{
    for (int i = 0; i < MORSE_TABLE_LEN; i++) {
        if (MORSE_TABLE[i].ch == c) {
            return MORSE_TABLE[i].morse;
        }
    }
    return NULL;
}

// send one character, including intra-letter spacing
static void send_char(char c)
{
    if (c == ' ') {
        // word gap: 7 dots total; we assume previous symbol already left us "off",
        // so just stay off for 7 * DOT_MS
        delay(7 * DOT_MS);
        return;
    }

    const char *m = char_to_morse(c);
    if (!m) {
        // unknown character, skip it
        return;
    }

    // For each symbol in Morse code for this character
    for (int i = 0; m[i] != '\0'; i++) {
        if (m[i] == '.') {
            send_dot();
        } else if (m[i] == '-') {
            send_dash();
        }
        // The dot/dash functions already include a 1-dot off period after the symbol.
    }

    // At end of letter, we need a total of 3-dot gap between letters.
    // We already had 1-dot off after the last symbol, so add 2 more dots worth.
    delay(2 * DOT_MS);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <repetitions> \"message\"\n", argv[0]);
        return 1;
    }

    int repetitions = atoi(argv[1]);
    if (repetitions <= 0) {
        fprintf(stderr, "Repetitions must be > 0\n");
        return 1;
    }

    const char *msg = argv[2];

    if (wiringPiSetupGpio() == -1) {
        fprintf(stderr, "Failed to init wiringPi\n");
        return 1;
    }

    pinMode(LED_PIN, OUTPUT);
    led_off();

    for (int r = 0; r < repetitions; r++) {
        const char *p = msg;
        while (*p) {
            char c = *p++;
            if (c >= 'a' && c <= 'z') {
                c = toupper((unsigned char)c);
            }
            send_char(c);
        }
        // gap between messages: 7-dot gap
        delay(7 * DOT_MS);
    }

    led_off();
    return 0;
}
