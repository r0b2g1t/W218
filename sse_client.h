#pragma once

#include "esphome.h"
#include <WiFi.h>

namespace esphome {

class SSEClient : public Component {
 public:
  WiFiClient client;
  std::string buffer;
  uint32_t last_reconnect = 0;
  uint32_t last_heartbeat = 0;
  bool connected = false;

  // Pointers to the globals defined in YAML
  globals::GlobalsComponent<float> *ph_glob;
  globals::GlobalsComponent<float> *orp_glob;
  globals::GlobalsComponent<uint32_t> *time_glob;
  globals::GlobalsComponent<bool> *has_data_glob;
  globals::GlobalsComponent<bool> *show_dot_glob;

  SSEClient(globals::GlobalsComponent<float> *ph, 
            globals::GlobalsComponent<float> *orp,
            globals::GlobalsComponent<uint32_t> *time,
            globals::GlobalsComponent<bool> *has_data,
            globals::GlobalsComponent<bool> *show_dot) 
      : ph_glob(ph), orp_glob(orp), time_glob(time), has_data_glob(has_data), show_dot_glob(show_dot) {}

  void setup() override {
    ESP_LOGI("sse", "SSE Client Iniciado.");
  }

  void loop() override {
    uint32_t now = millis();
    
    // Heartbeat para log
    if (now - last_heartbeat > 10000) {
      last_heartbeat = now;
      ESP_LOGD("sse", "SSE Client ativo... Conectado: %s", connected ? "SIM" : "NÃO");
    }

    if (!WiFi.isConnected()) {
      connected = false;
      return;
    }

    if (!connected) {
      if (now - last_reconnect > 5000) {
        last_reconnect = now;
        connect_to_piscina();
      }
      return;
    }

    while (client.available()) {
      char c = client.read();
      if (c == '\n') {
        process_line(buffer);
        buffer = "";
      } else if (c != '\r') {
        buffer += c;
      }
    }

    if (!client.connected()) {
      ESP_LOGW("sse", "Conexão SSE perdida.");
      connected = false;
    }
  }

  void connect_to_piscina() {
    ESP_LOGI("sse", "Conectando ao stream da piscina (192.168.6.151)...");
    if (client.connect("192.168.6.151", 80)) {
      client.print("GET /events HTTP/1.1\r\n");
      client.print("Host: 192.168.6.151\r\n");
      client.print("Accept: text/event-stream\r\n");
      client.print("Connection: keep-alive\r\n\r\n");
      connected = true;
      ESP_LOGI("sse", "SSE CONECTADO COM SUCESSO!");
    } else {
      ESP_LOGW("sse", "Falha ao abrir stream na piscina.");
    }
  }

  void process_line(std::string line) {
    if (line.find("data: ") != std::string::npos) {
      ESP_LOGD("sse", "Linha: %s", line.c_str());
      
      if (line.find("sensor-ph_piscina") != std::string::npos) {
        size_t pos = line.find("\"value\":");
        if (pos != std::string::npos) {
          float val = atof(line.substr(pos + 8).c_str());
          ESP_LOGI("sse", "Sincronizado pH: %.2f", val);
          update_sensor("ph", val);
        }
      } else if (line.find("sensor-orp_piscina") != std::string::npos) {
        size_t pos = line.find("\"value\":");
        if (pos != std::string::npos) {
          float val = atof(line.substr(pos + 8).c_str());
          ESP_LOGI("sse", "Sincronizado ORP: %.0f", val);
          update_sensor("orp", val);
        }
      }
    }
  }

  void update_sensor(std::string type, float value) {
    if (type == "ph") {
        ph_glob->value() = value;
        time_glob->value() = millis();
        has_data_glob->value() = true;
        
        show_dot_glob->value() = true;
        auto *dot = show_dot_glob;
        App.scheduler.set_timeout(this, "hide_dot", 1000, [dot]() {
            dot->value() = false;
        });
    } else if (type == "orp") {
        orp_glob->value() = value;
        time_glob->value() = millis();
        has_data_glob->value() = true;
    }
  }
};

} // namespace esphome
