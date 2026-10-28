#pragma once
// Faux UART pour les tests sur PC. NE DOIT JAMAIS etre inclus dans le firmware.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "uart.hpp"

class MockUart final : public Uart {
  public:
    // Ce que le code teste "envoie" : on le garde pour le verifier.
    void putc(uint8_t b) override { tx_.push_back(b); }

    // Ce que le code teste "recoit" : on le prepare avec feed().
    bool getc(uint8_t &b) override {
        if (rxPos_ >= rx_.size()) {
            return false;
        }
        b = rx_[rxPos_++];
        return true;
    }

    void feed(const uint8_t *data, std::size_t len) { rx_.insert(rx_.end(), data, data + len); }
    const std::vector<uint8_t> &sent() const { return tx_; }
    void clearSent() { tx_.clear(); }

  private:
    std::vector<uint8_t> tx_;
    std::vector<uint8_t> rx_;
    std::size_t rxPos_ = 0;
};