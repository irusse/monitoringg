#include "hal/rs485.h"
#include "access/pins.h"
#include <Arduino.h>
#ifndef ACCESS_STUB_NO_RADIO
#include <HardwareSerial.h>
#endif

uint32_t Rs485Hal::uartConfig() const {
  const bool two = cfg_.stop_bits >= 2;
  switch (cfg_.parity) {
    case Rs485Parity::Even:
      return two ? SERIAL_8E2 : SERIAL_8E1;
    case Rs485Parity::Odd:
      return two ? SERIAL_8O2 : SERIAL_8O1;
    case Rs485Parity::None:
    default:
      return two ? SERIAL_8N2 : SERIAL_8N1;
  }
}

void Rs485Hal::holdDeAfterTx() {
#ifndef ACCESS_STUB_NO_RADIO
  // One stop-bit time after UART flush so the last edge leaves the PHY.
  uint32_t us = 1000000UL / (cfg_.baud ? cfg_.baud : 9600);
  if (us < 50) us = 50;
  delayMicroseconds((uint16_t)(us * 2));
#endif
}

bool Rs485Hal::begin(uint32_t baud, Rs485Parity parity, uint8_t stop_bits) {
  Rs485Config c;
  c.baud = baud ? baud : 9600;
  c.parity = parity;
  c.stop_bits = (stop_bits == 2) ? 2 : 1;
  return begin(c);
}

bool Rs485Hal::begin(const Rs485Config& cfg) {
  cfg_ = cfg;
  if (cfg_.baud < 1200) cfg_.baud = 9600;
  if (cfg_.stop_bits != 2) cfg_.stop_bits = 1;
  rx_count_ = 0;
  tx_count_ = 0;

#ifdef ACCESS_STUB_NO_RADIO
  ok_ = true;
  return true;
#else
  pinMode(PIN_RS485_DE, OUTPUT);
  digitalWrite(PIN_RS485_DE, PIN_RS485_DE_RX);
  tx_dir_ = false;

  Serial1.setRxBufferSize(256);
  Serial1.setTxBufferSize(256);
  Serial1.end();
  Serial1.begin(cfg_.baud, uartConfig(), PIN_RS485_RX, PIN_RS485_TX);
  ok_ = true;
  Serial1.setTimeout(20);
  Serial.printf("RS485 UART1 %lu 8%c%u TX=%d RX=%d DE=%d\n",
                (unsigned long)cfg_.baud,
                cfg_.parity == Rs485Parity::Even ? 'E'
                    : (cfg_.parity == Rs485Parity::Odd ? 'O' : 'N'),
                (unsigned)cfg_.stop_bits, PIN_RS485_TX, PIN_RS485_RX, PIN_RS485_DE);
  return true;
#endif
}

void Rs485Hal::setDirection(bool tx) {
#ifdef ACCESS_STUB_NO_RADIO
  tx_dir_ = tx;
#else
  if (tx == tx_dir_) return;
  if (!tx) {
    Serial1.flush();
    holdDeAfterTx();
  }
  digitalWrite(PIN_RS485_DE, tx ? PIN_RS485_DE_TX : PIN_RS485_DE_RX);
  tx_dir_ = tx;
  if (tx) {
    delayMicroseconds(20);
  }
#endif
}

size_t Rs485Hal::writeFrame(const uint8_t* data, size_t len) {
  if (!ok_ || !data || len == 0) return 0;
#ifdef ACCESS_STUB_NO_RADIO
  tx_count_ += (uint32_t)len;
  return len;
#else
  setDirection(true);
  const size_t n = Serial1.write(data, len);
  Serial1.flush();
  holdDeAfterTx();
  setDirection(false);
  tx_count_ += (uint32_t)n;
  return n;
#endif
}

size_t Rs485Hal::readFrame(uint8_t* buf, size_t max, uint32_t timeout_ms) {
  if (!ok_ || !buf || max == 0) return 0;
#ifdef ACCESS_STUB_NO_RADIO
  (void)timeout_ms;
  return 0;
#else
  setDirection(false);
  if (timeout_ms > 2000) timeout_ms = 2000;
  const uint32_t start = millis();
  size_t n = 0;
  uint32_t last_rx = start;
  // ~3.5 character times (10-bit frames). RAW: stop on first gap after RX, else timeout_ms.
  uint32_t gap_ms = 35000UL / (cfg_.baud ? cfg_.baud : 9600);
  if (gap_ms < 5) gap_ms = 5;
  if (gap_ms > 50) gap_ms = 50;
  while (n < max && (millis() - start) < timeout_ms) {
    loop();
    if (Serial1.available()) {
      buf[n++] = (uint8_t)Serial1.read();
      rx_count_++;
      last_rx = millis();
    } else if (n > 0 && (millis() - last_rx) >= gap_ms) {
      break;
    } else {
      delay(1);
    }
  }
  return n;
#endif
}

void Rs485Hal::loop() {
#ifndef ACCESS_STUB_NO_RADIO
  if (!ok_ || tx_dir_) return;
  // UART FIFO is drained by available()/read(); nothing else to pump.
#endif
}

size_t Rs485Hal::available() const {
#ifdef ACCESS_STUB_NO_RADIO
  return 0;
#else
  if (!ok_) return 0;
  return (size_t)Serial1.available();
#endif
}

size_t Rs485Hal::read(uint8_t* buf, size_t max) {
  if (!ok_ || !buf || max == 0) return 0;
#ifdef ACCESS_STUB_NO_RADIO
  return 0;
#else
  size_t n = 0;
  while (n < max && Serial1.available()) {
    buf[n++] = (uint8_t)Serial1.read();
  }
  rx_count_ += (uint32_t)n;
  return n;
#endif
}
