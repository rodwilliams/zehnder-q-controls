#include "gp8413_output.h"

#include "esphome/core/log.h"

namespace esphome {
namespace gp8413 {

static const char *const TAG = "gp8413.output";

static const uint8_t OUTPUT_REGISTER = 0x02;

void GP8413Output::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GP8413 Output:\n"
                "  Channel: %u",
                this->channel_);
}

void GP8413Output::write_state(float state) {
  // 15-bit DAC code (0-32767), left-justified into the 16-bit register (same convention as GP8403's
  // 12-bit-code-left-shifted-by-4, confirmed against DFRobot_GP8XXX's setDACOutVoltage: value << 1 for 15-bit)
  uint16_t value = ((uint16_t) (state * 32767)) << 1;
  i2c::ErrorCode err = this->parent_->write_register(OUTPUT_REGISTER + (2 * this->channel_), (uint8_t *) &value, 2);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error writing to GP8413, code %d", err);
  }
}

}  // namespace gp8413
}  // namespace esphome
