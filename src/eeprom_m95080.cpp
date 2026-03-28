#include "eeprom_m95080.h"
#include "hal.h"

#include <cstring>

namespace iris_common {

EepromM95080* EepromM95080::active_owner_ = nullptr;

namespace {
constexpr std::uint8_t CMD_WREN  = 0x06;
constexpr std::uint8_t CMD_RDSR  = 0x05;
constexpr std::uint8_t CMD_READ  = 0x03;
constexpr std::uint8_t CMD_WRITE = 0x02;
constexpr std::uint8_t SR_WIP    = 0x01;
constexpr std::uint8_t SR_WEL    = 0x02;
constexpr std::uint32_t kWritePollIntervalMs = 5;
}

EepromM95080::EepromM95080() = default;

bool EepromM95080::begin_read(std::uint16_t addr, std::uint8_t* dst, std::uint16_t len) {
    if (state_ != State::Idle) return false;
    if (!dst || len == 0 || addr + len > kSizeBytes) return false;
    if (active_owner_ != nullptr && active_owner_ != this) return false;

    addr_ = addr;
    dst_ = dst;
    remaining_ = len;
    done_ = false;
    active_owner_ = this;

    state_ = State::ReadCmdData;
    return true;
}

bool EepromM95080::begin_write(std::uint16_t addr, const std::uint8_t* src, std::uint16_t len) {
    if (state_ != State::Idle) return false;
    if (!src || len == 0 || addr + len > kSizeBytes) return false;
    if (active_owner_ != nullptr && active_owner_ != this) return false;

    addr_ = addr;
    src_ = src;
    remaining_ = len;
    done_ = false;
    active_owner_ = this;

    state_ = State::Wren;
    return true;
}

void EepromM95080::tick() {
    // Progress SPI polling engine.
    iris_common::hal::spi1_poll_tick();

    switch (state_) {
    case State::Idle:
        break;
    case State::Wren:
        if (!iris_common::hal::spi1_poll_busy()) {
            start_wren();
            next_poll_tick_ = iris_common::hal::tick_ms() + 1u; // 1ms spacing
            state_ = State::WrenPollStart;
        }
        break;
    case State::WrenPollStart:
        if (!iris_common::hal::spi1_poll_busy()) {
            start_read_status();
            state_ = State::WrenPollCheck;
        }
        break;
    case State::WrenPollCheck:
        if (!iris_common::hal::spi1_poll_busy()) {
            const std::uint8_t sr = status_rx_[1];
            last_status_ = sr;
            if ((sr & SR_WEL) == 0) {
                state_ = State::Error;
            } else {
                state_ = State::WriteCmdWait;
            }
        }
        break;
    case State::WriteCmdWait:
        if (static_cast<std::int32_t>(iris_common::hal::tick_ms() - next_poll_tick_) >= 0) {
            state_ = State::WriteCmdData;
        }
        break;
    case State::WriteCmdData:
        if (!iris_common::hal::spi1_poll_busy()) {
            const std::uint16_t chunk = page_remaining();
            start_write_cmd_data(chunk);
            remaining_ -= chunk;
            addr_ += chunk;
            src_ += chunk;
            state_ = State::WriteDataWait;
        }
        break;
    case State::WriteDataWait:
        if (!iris_common::hal::spi1_poll_busy()) {
            next_poll_tick_ = iris_common::hal::tick_ms() + kWritePollIntervalMs; // allow tWC start
            state_ = State::WritePollStart;
        }
        break;
    case State::WritePollStart:
        if (!iris_common::hal::spi1_poll_busy()) {
            next_poll_tick_ = iris_common::hal::tick_ms() + kWritePollIntervalMs; // poll interval
            state_ = State::WritePollWait;
        }
        break;
    case State::WritePollWait:
        if (static_cast<std::int32_t>(iris_common::hal::tick_ms() - next_poll_tick_) >= 0) {
            start_read_status();
            state_ = State::WritePollCheck;
        }
        break;
    case State::WritePollCheck:
        if (!iris_common::hal::spi1_poll_busy()) {
            const std::uint8_t sr = status_rx_[1];
            last_status_ = sr;
            if ((sr & SR_WIP) == 0) {
                if (remaining_ == 0) {
                    state_ = State::Done;
                } else {
                    state_ = State::Wren;
                }
            } else {
                state_ = State::WritePollStart;
            }
        }
        break;
    case State::ReadCmdData:
        if (!iris_common::hal::spi1_poll_busy()) {
            const std::uint16_t chunk = (remaining_ < kIoChunk) ? remaining_ : kIoChunk;
            start_read_cmd_data(chunk);
            state_ = State::ReadDataWait;
        }
        break;
    case State::ReadDataWait:
        if (!iris_common::hal::spi1_poll_busy()) {
            const std::uint16_t chunk = (remaining_ < kIoChunk) ? remaining_ : kIoChunk;
            std::memcpy(dst_, rx_buf_ + 3, chunk);
            remaining_ -= chunk;
            addr_ += chunk;
            dst_ += chunk;
            if (remaining_ == 0) {
                state_ = State::Done;
            } else {
                state_ = State::ReadCmdData;
            }
        }
        break;
    case State::Done:
        done_ = true;
        if (active_owner_ == this) {
            active_owner_ = nullptr;
        }
        state_ = State::Idle;
        break;
    case State::Error:
        if (active_owner_ == this) {
            active_owner_ = nullptr;
        }
        state_ = State::Idle;
        break;
    }
}

bool EepromM95080::busy() const {
    return state_ != State::Idle;
}

bool EepromM95080::done() const {
    return done_;
}

std::uint8_t EepromM95080::last_status() const {
    return last_status_;
}

void EepromM95080::start_wren() {
    cmd_[0] = CMD_WREN;
    iris_common::hal::spi1_poll_start(cmd_, nullptr, 1);
}

void EepromM95080::start_write_cmd_data(std::uint16_t chunk) {
    tx_buf_[0] = CMD_WRITE;
    tx_buf_[1] = static_cast<std::uint8_t>((addr_ >> 8) & 0x03);
    tx_buf_[2] = static_cast<std::uint8_t>(addr_ & 0xFF);
    std::memcpy(&tx_buf_[3], src_, chunk);
    iris_common::hal::spi1_poll_start(tx_buf_, nullptr, static_cast<std::uint16_t>(3 + chunk));
}

void EepromM95080::start_read_cmd_data(std::uint16_t chunk) {
    tx_buf_[0] = CMD_READ;
    tx_buf_[1] = static_cast<std::uint8_t>((addr_ >> 8) & 0x03);
    tx_buf_[2] = static_cast<std::uint8_t>(addr_ & 0xFF);
    std::memset(&tx_buf_[3], 0xFF, chunk);
    iris_common::hal::spi1_poll_start(tx_buf_, rx_buf_, static_cast<std::uint16_t>(3 + chunk));
}

void EepromM95080::start_read_status() {
    dummy_tx_[0] = CMD_RDSR;
    dummy_tx_[1] = 0xFF;
    iris_common::hal::spi1_poll_start(dummy_tx_, status_rx_, 2);
}

std::uint16_t EepromM95080::page_remaining() const {
    const std::uint16_t offset = static_cast<std::uint16_t>(addr_ % kPageSize);
    const std::uint16_t rem = static_cast<std::uint16_t>(kPageSize - offset);
    return (remaining_ < rem) ? remaining_ : rem;
}

} // namespace iris_common
