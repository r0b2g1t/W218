#pragma once

#include "esphome.h"
#include <WiFiUdp.h>

namespace esphome {

class UDPReceiver : public Component {
 public:
  WiFiUDP udp;
  char buffer[64];
  
  globals::GlobalsComponent<float> *ph_glob;
  globals::GlobalsComponent<float> *orp_glob;
  globals::GlobalsComponent<uint32_t> *time_glob;
  globals::GlobalsComponent<bool> *has_data_glob;
  globals::GlobalsComponent<bool> *show_dot_glob;

  UDPReceiver(globals::GlobalsComponent<float> *ph, 
              globals::GlobalsComponent<float> *orp,
              globals::GlobalsComponent<uint32_t> *time,
              globals::GlobalsComponent<bool> *has_data,
              globals::GlobalsComponent<bool> *show_dot) 
      : ph_glob(ph), orp_glob(orp), time_glob(time), has_data_glob(has_data), show_dot_glob(show_dot) {}

  void setup() override {
    udp.begin(12345);
    ESP_LOGI("udp", "Ouvindo na porta 12345...");
  }

  void loop() override {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      int len = udp.read(buffer, 63);
      if (len > 0) {
        buffer[len] = 0;
        process_data(buffer);
      }
    }
  }

  void process_data(const char* data) {
    ESP_LOGD("udp", "Recebido: %s", data);
    
    // Formato esperado: P:7.75|O:720
    float ph = 0, orp = 0;
    if (sscanf(data, "P:%f|O:%f", &ph, &orp) == 2) {
        ph_glob->value() = ph;
        orp_glob->value() = orp;
        time_glob->value() = millis();
        has_data_glob->value() = true;
        
        show_dot_glob->value() = true;
        auto *dot = show_dot_glob;
        App.scheduler.set_timeout(this, "hide_dot", 500, [dot]() {
            dot->value() = false;
        });
    }
  }
};

} // namespace esphome
