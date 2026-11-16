#pragma once

#include <stddef.h>
#include <stdint.h>

enum class Rs485Parity : uint8_t { None = 0, Even = 1, Odd = 2 };

struct Rs485Config {
  uint32_t baud = 9600;
  Rs485Parity parity = Rs485Parity::None;
  uint8_t stop_bits = 1;
};

class Rs485Hal {
public:
  bool begin(const Rs485Config& cfg = Rs485Config());
  bool begin(uint32_t baud, Rs485Parity parity, uint8_t stop_bits);

  // DE/RE: true = transmit (DE high), false = receive (DE low, fail-safe).
  void setDirection(bool tx);

  size_t writeFrame(const uint8_t* data, size_t len);
  size_t readFrame(uint8_t* buf, size_t max, uint32_t timeout_ms);

  void loop();
  size_t available() const;
  size_t read(uint8_t* buf, size_t max);

  uint32_t rxCount() const { return rx_count_; }
  uint32_t txCount() const { return tx_count_; }
  uint32_t baud() const { return cfg_.baud; }
  uint8_t stopBits() const { return cfg_.stop_bits; }
  Rs485Parity parity() const { return cfg_.parity; }
  char parityChar() const {
    if (cfg_.parity == Rs485Parity::Even) return 'E';
    if (cfg_.parity == Rs485Parity::Odd) return 'O';
    return 'N';
  }
  bool ok() const { return ok_; }
  bool txDirection() const { return tx_dir_; }

private:
  Rs485Config cfg_{};
  bool ok_ = false;
  bool tx_dir_ = false;
  uint32_t rx_count_ = 0;
  uint32_t tx_count_ = 0;

  uint32_t uartConfig() const;
  void holdDeAfterTx();
};
