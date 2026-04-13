#include "esphome.h"

class W218Driver {
 public:
  esphome::sensor::Sensor *ph_sensor = nullptr;
  esphome::sensor::Sensor *orp_sensor = nullptr;
  esphome::sensor::Sensor *temp_sensor = nullptr;
  esphome::sensor::Sensor *tds_sensor = nullptr;
  esphome::sensor::Sensor *ec_sensor = nullptr;
  esphome::sensor::Sensor *salinity_sensor = nullptr;
  esphome::sensor::Sensor *cf_sensor = nullptr;
  esphome::sensor::Sensor *rh_sensor = nullptr;
  esphome::time::RealTimeClock *time_source = nullptr;

  W218Driver(esphome::uart::UARTComponent *uart) : uart_(uart) {}

  void tick() {
    while (uart_->available()) {
      uint8_t c;
      uart_->read_byte(&c);
      parse_tuya_byte(c);
    }
    
    uint32_t now = millis();
    // Heartbeat proativo a cada 10 segundos
    if (now - last_heartbeat_ > 10000) {
        send_command(0x00, {0x00});
        last_heartbeat_ = now;
    }

    // Query data a cada 15 segundos se houver inatividade
    if (now - last_data_received_ > 15000 && now - last_query_sent_ > 5000) {
        ESP_LOGD("w218", "Inatividade detectada, solicitando dados...");
        send_command(0x08, {});
        last_query_sent_ = now;
    }
  }

 protected:
  esphome::uart::UARTComponent *uart_;
  std::vector<uint8_t> rx_buffer_;
  int state_ = 0;
  uint8_t mcu_version_ = 0x03; 
  uint8_t command_ = 0;
  uint16_t data_len_ = 0;
  uint32_t last_handshake_response_ = 0;
  uint32_t last_data_received_ = 0;
  uint32_t last_query_sent_ = 0;

  void parse_tuya_byte(uint8_t c) {
    switch (state_) {
      case 0: if (c == 0x55) state_ = 1; break;
      case 1: if (c == 0xAA) state_ = 2; else state_ = 0; break;
      case 2: mcu_version_ = c; state_ = 3; break;
      case 3: command_ = c; state_ = 4; break;
      case 4: data_len_ = c << 8; state_ = 5; break;
      case 5: 
        data_len_ |= c; 
        rx_buffer_.clear(); 
        if (data_len_ > 512) { state_ = 0; break; } 
        state_ = (data_len_ == 0) ? 7 : 6; 
        break;
      case 6: 
        rx_buffer_.push_back(c);
        if (rx_buffer_.size() >= data_len_) state_ = 7;
        break;
      case 7: {
        uint16_t sum = mcu_version_ + command_ + (data_len_ >> 8) + (data_len_ & 0xFF);
        for (auto b : rx_buffer_) sum += b;
        uint8_t cs = (uint8_t)(sum - 1);
        if (cs == c) {
          handle_packet(command_, rx_buffer_);
        }
        state_ = 0;
        break;
      }
    }
  }

  void handle_packet(uint8_t cmd, std::vector<uint8_t> &data) {
    uint32_t now = millis();
    switch (cmd) {
      case 0x00: // Heartbeat response
        break;
      case 0x01: {
        std::string pid = "{\"p\":\"layxxij0sdbrfmrf\",\"v\":\"1.0.0\",\"m\":2}";
        std::vector<uint8_t> p(pid.begin(), pid.end());
        send_command(0x01, p);
        break;
      }
      case 0x02: send_command(0x02, {0x01}); break;
      case 0x03: // WiFi Status (Legacy)
        send_command(0x03, {0x04}); 
        break;
      case 0x2B: // WiFi Status (New)
        if (now - last_handshake_response_ > 2000) {
            send_command(0x2B, {0x04}); 
            send_command(0x08, {}); 
            last_handshake_response_ = now;
        }
        break; 
      case 0x07: 
        last_data_received_ = now;
        parse_dp_data(data); 
        break;        
      case 0x1C:
      case 0x24:
        send_time_sync();
        break;
    }
  }

  void send_time_sync() {
    if (time_source == nullptr) return;
    auto now = time_source->now();
    if (!now.is_valid()) return;
    std::vector<uint8_t> p = { 0x01, (uint8_t)(now.year - 2000), (uint8_t)now.month, (uint8_t)now.day_of_month, (uint8_t)now.hour, (uint8_t)now.minute, (uint8_t)now.second };
    send_command(0x24, p);
  }

  void parse_dp_data(std::vector<uint8_t> &data) {
    size_t offset = 0;
    while (offset + 4 <= data.size()) {
        uint8_t dp_id = data[offset];
        uint16_t len = (data[offset + 2] << 8) | data[offset + 3];
        if (offset + 4 + len > data.size()) break;
        uint32_t val = 0;
        if (len == 4)      val = ((uint32_t)data[offset+4]<<24)|((uint32_t)data[offset+5]<<16)|((uint32_t)data[offset+6]<<8)|(uint32_t)data[offset+7];
        else if (len == 2) val = (data[offset+4] << 8) | data[offset+5];
        else if (len == 1) val = data[offset+4];

        ESP_LOGD("w218", "Got DP %d = %u", dp_id, val);

        if (dp_id == 106 && ph_sensor) ph_sensor->publish_state(val * 0.01f);
        else if (dp_id == 131 && orp_sensor) orp_sensor->publish_state(val);
        else if ((dp_id == 8 || dp_id == 108) && temp_sensor) temp_sensor->publish_state(val * 0.1f);
        else if (dp_id == 126 && tds_sensor) tds_sensor->publish_state(val);
        else if (dp_id == 116 && ec_sensor) ec_sensor->publish_state(val * 0.01f);
        else if (dp_id == 121 && salinity_sensor) salinity_sensor->publish_state(val);
        else if (dp_id == 136 && cf_sensor) cf_sensor->publish_state(val * 0.1f);
        else if (dp_id == 141 && rh_sensor) rh_sensor->publish_state(val);
        offset += (4 + len);
    }
  }

  void send_command(uint8_t cmd, std::vector<uint8_t> payload) {
    uint16_t len = payload.size();
    uint8_t ver = 0x00; 
    uint16_t sum = ver + cmd + (len >> 8) + (len & 0xFF);
    for (auto b : payload) sum += b;
    uint8_t cs = (uint8_t)(sum - 1);
    
    uart_->write_byte(0x55); uart_->write_byte(0xAA);
    uart_->write_byte(ver); uart_->write_byte(cmd);
    uart_->write_byte(len >> 8); uart_->write_byte(len & 0xFF);
    for (auto b : payload) uart_->write_byte(b);
    uart_->write_byte(cs);
    
    ESP_LOGD("w218", "TX -> CMD %02X", cmd);
  }

 private:
  uint32_t last_heartbeat_ = 0;
};
