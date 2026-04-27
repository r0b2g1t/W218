#include <WiFiUDP.h>
#include "esphome.h"

void broadcast_pool_data(float ph, float orp) {
    static WiFiUDP udp;
    // Enviando direto para o IP do quiosque para evitar bloqueios de broadcast no roteador
    if (udp.beginPacket("192.168.6.161", 12345)) {
        udp.printf("P:%.2f|O:%.0f", ph, orp);
        udp.endPacket();
        ESP_LOGD("udp", "Enviado para Quiosque: P:%.2f|O:%.0f", ph, orp);
    } else {
        ESP_LOGW("udp", "Erro de socket UDP - Tentando reiniciar...");
        udp.stop(); 
    }
}
