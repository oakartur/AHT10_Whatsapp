#ifndef WHATSAPP_H
#define WHATSAPP_H

#include <stdbool.h>

// Function prototype for sending a message to WhatsApp
void send_to_whatsapp(const char *phone_number, const char *api_key, const char *message);
void url_encode(const char *src, char *dest, size_t dest_size);

#endif // WHATSAPP_H