#pragma once

#include <cstdint>

void decode_hex_byte(char *buffer, uint8_t byte);
void decode_hex_word(char *buffer, uint16_t word);
void decode_ascii(char *buffer, uint8_t byte);