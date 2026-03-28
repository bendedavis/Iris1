#pragma once

#include <cstdint>

namespace iris_common {

class EepromM95080 {
public:
    static constexpr std::uint16_t kSizeBytes = 1024;
    static constexpr std::uint8_t kPageSize = 32;

    EepromM95080();

    // Nonblocking operations. Return false if busy.
    bool begin_read(std::uint16_t addr, std::uint8_t* dst, std::uint16_t len);
    bool begin_write(std::uint16_t addr, const std::uint8_t* src, std::uint16_t len);

    // Call from main loop to advance state machine.
    void tick();

    bool busy() const;
    bool done() const;
    std::uint8_t last_status() const;

private:
    enum class State : std::uint8_t {
        Idle,
        Wren,
        WrenPollStart,
        WrenPollCheck,
        WriteCmdWait,
        WriteCmdData,
        WriteDataWait,
        WritePollStart,
        WritePollWait,
        WritePollCheck,
        ReadCmdData,
        ReadDataWait,
        Done,
        Error,
    };

    void start_wren();
    void start_write_cmd_data(std::uint16_t chunk);
    void start_read_cmd_data(std::uint16_t chunk);
    void start_read_status();

    std::uint16_t page_remaining() const;

    State state_ = State::Idle;
    bool done_ = false;

    std::uint16_t addr_ = 0;
    std::uint16_t remaining_ = 0;
    const std::uint8_t* src_ = nullptr;
    std::uint8_t* dst_ = nullptr;
    std::uint32_t next_poll_tick_ = 0;

    // Small command buffers
    std::uint8_t cmd_[3] = {0};
    std::uint8_t status_rx_[2] = {0};
    std::uint8_t last_status_ = 0;
    std::uint8_t dummy_tx_[2] = {0xFF, 0xFF};

    static constexpr std::uint16_t kIoChunk = 32;
    std::uint8_t tx_buf_[3 + kIoChunk] = {0};
    std::uint8_t rx_buf_[3 + kIoChunk] = {0};

    // Shared bus guard: only one logical EEPROM transaction at a time
    // across all EepromM95080 instances.
    static EepromM95080* active_owner_;
};

} // namespace iris_common
