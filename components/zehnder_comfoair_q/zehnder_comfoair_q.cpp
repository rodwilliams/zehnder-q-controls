#include "zehnder_comfoair_q.h"

namespace zehnder_comfoair_q
{
    void ZehnderComfoAirQ::setup()
    {
        // do a first request of all PDOs some time after starting (helpful for long intervals)
        this->set_timeout(1000 * 10, [this]()
                          { this->update(); });
    }

    void ZehnderComfoAirQ::update()
    {
        if (request_ids_.size() > 0)
            request_all_pdos();
    }

    void ZehnderComfoAirQ::request_all_pdos()
    {
        ESP_LOGD(TAG, "Requesting all registered PDOs (count: %d)", request_ids_.size());

        if (request_next_pdo_pos_ == 0)
            request_next_pdo_();
        else
            ESP_LOGW(TAG, "Skipping PDO request cycle as last cycle is still in progress.");
    }

    void ZehnderComfoAirQ::request_next_pdo_()
    {
        if (request_next_pdo_pos_ >= request_ids_.size())
            return;

        this->request_pdo(request_ids_[request_next_pdo_pos_]);
        request_next_pdo_pos_++;
        if (request_next_pdo_pos_ < request_ids_.size())
        {
            this->set_timeout(this->request_delay_, [this]()
                              { this->request_next_pdo_(); });
        }
        else
        {
            request_next_pdo_pos_ = 0;
        }
    }

    void ZehnderComfoAirQ::request_pdo(uint16_t pdo_id)
    {
        // ZehnderComfoAirQ does not care for the data length to be set corectly on transmission requests, so there is no need to send any data
        this->send_can_message_((pdo_id << 14) + 0x40 + this->local_node_id_, true);
    }

    const char *const ZehnderComfoAirQ::IMPERSONATION_TICK_NAME = "comfoair_impersonation_tick";

    void ZehnderComfoAirQ::send_pdo_broadcast_(uint16_t pdo_id, uint8_t src_node_id, const std::vector<uint8_t> &data)
    {
        send_can_message_((pdo_id << 14) + 0x40 + src_node_id, false, data);
    }

    void ZehnderComfoAirQ::send_heartbeat_(uint8_t src_node_id)
    {
        send_can_message_(0x10000000 + src_node_id, false, {});
    }

    void ZehnderComfoAirQ::send_join_sequence_()
    {
        ESP_LOGI(TAG, "Impersonating node %d: sending join sequence", impersonated_node_id_);
        // mirrors what the real Option Box does on connect: RTR-request these PDOs from the Q450,
        // then announce itself with a 4-byte frame on its heartbeat can_id, before settling into
        // the plain empty-frame heartbeat.
        send_can_message_((17 << 14) + 0x40 + impersonated_node_id_, true);
        send_can_message_((385 << 14) + 0x40 + impersonated_node_id_, true);
        send_can_message_((401 << 14) + 0x40 + impersonated_node_id_, true);
        send_can_message_(0x10000000 + impersonated_node_id_, false, {0x04, 0x01, 0x00, 0x00});
    }

    void ZehnderComfoAirQ::start_impersonation()
    {
        if (impersonation_active_)
        {
            ESP_LOGW(TAG, "Impersonation already active, ignoring start request.");
            return;
        }
        impersonation_active_ = true;
        send_join_sequence_();
        // real Option Box's heartbeat/PDO cadence is roughly every 2s per channel
        this->set_interval(IMPERSONATION_TICK_NAME, 2000, [this]()
                           { this->impersonation_tick_(); });
    }

    void ZehnderComfoAirQ::stop_impersonation()
    {
        if (!impersonation_active_)
            return;
        impersonation_active_ = false;
        this->cancel_interval(IMPERSONATION_TICK_NAME);
        ESP_LOGI(TAG, "Impersonation stopped - heartbeat no longer sent, Q450 will raise a fault after ~30s.");
    }

    void ZehnderComfoAirQ::impersonation_tick_()
    {
        send_heartbeat_(impersonated_node_id_);
        for (uint8_t channel = 1; channel <= 4; channel++)
        {
            uint16_t raw = analog_raw_[channel - 1];
            send_pdo_broadcast_(512 + channel, impersonated_node_id_, {(uint8_t)(raw & 0xff), (uint8_t)(raw >> 8)});
        }
    }

    void ZehnderComfoAirQ::set_analog_input_raw(uint8_t channel, uint16_t raw_value)
    {
        if (channel < 1 || channel > 4)
        {
            ESP_LOGW(TAG, "set_analog_input_raw: invalid channel %d (must be 1-4)", channel);
            return;
        }
        uint16_t pdo_id = 512 + channel;
        analog_raw_[channel - 1] = raw_value;
        ESP_LOGI(TAG, "Set V%d raw = %u", channel, raw_value);
        if (impersonation_active_)
        {
            // publish immediately rather than waiting for the next tick, so changes are responsive
            send_pdo_broadcast_(pdo_id, impersonated_node_id_, {(uint8_t)(raw_value & 0xff), (uint8_t)(raw_value >> 8)});
        }
    }

    void ZehnderComfoAirQ::set_level(uint8_t level)
    {
        send_command_set_timer(true, 0x01, 0x01, level);
    }

    void ZehnderComfoAirQ::set_level_float(float state)
    {
        uint8_t level;
        if (state >= 1)
            level = 3;
        else if (state >= 0.5)
            level = 2;
        else if (state > 0)
            level = 1;
        else
            level = 0;
        ESP_LOGD(TAG, "send_cmd_level_float: state: %f, level: %d", state, level);

        set_level(level);
    }

    void ZehnderComfoAirQ::send_command_set_timer(bool enable, uint8_t subunit_id, uint8_t property_id, uint8_t property_value, uint32_t duration_secs)
    {
        std::vector<uint8_t> command = {(uint8_t)(enable ? 0x84 : 0x85), 0x15 /* SCHEDULE */, subunit_id, property_id};
        if (enable)
            command.insert(command.end(), {0x00, 0x00, 0x00, 0x00, (uint8_t)(duration_secs), (uint8_t)(duration_secs >> 8),
                                           (uint8_t)(duration_secs >> 16), (uint8_t)(duration_secs >> 24), property_value});
        send_command(command);
    }

    void ZehnderComfoAirQ::send_command_set_property(uint8_t unit_id, uint8_t subunit_id, uint8_t property_id, uint8_t property_value)
    {
        send_command({0x03, unit_id, subunit_id, property_id, property_value});
    }

    void ZehnderComfoAirQ::send_command(const std::vector<uint8_t> &command)
    {
        auto is_multi_message_command = command.size() > 8;
        auto can_id = get_command_next_can_id_(this->local_node_id_, 0x01, 0, is_multi_message_command, false, true);

        if (is_multi_message_command)
        {
            std::vector<uint8_t> message_buffer;
            int message_counter = 0;
            for (auto command_pos = command.begin(); command_pos < command.end(); command_pos += 7)
            {
                auto is_last_message = command.end() - command_pos <= 7;
                message_buffer.clear();
                message_buffer.push_back(message_counter | (is_last_message ? 0x80 : 0));
                message_buffer.insert(message_buffer.end(), command_pos, std::min(command_pos + 7, command.end()));
                send_can_message_(can_id, false, message_buffer);

                message_counter++;
            }
        }
        else
        {
            send_can_message_(can_id, false, command);
        }
    }

    uint32_t ZehnderComfoAirQ::get_command_next_can_id_(uint8_t src_node_id, uint8_t dst_node_id, uint8_t unknown_counter,
                                                        bool is_multi_message_command, bool response_error_occurred,
                                                        bool is_request)
    {
        return get_command_can_id_(src_node_id, dst_node_id, unknown_counter, is_multi_message_command,
                                   response_error_occurred, is_request, get_command_next_sequence_number_());
    }

    uint32_t ZehnderComfoAirQ::get_command_can_id_(uint8_t src_node_id, uint8_t dst_node_id, uint8_t unknown_counter,
                                                   bool is_multi_message_command, bool response_error_occurred,
                                                   bool is_request, uint8_t sequence_number)
    {
        return 0x1f << 24 |
               sequence_number << 17 |
               (is_request ? 1 : 0) << 16 |
               (response_error_occurred ? 1 : 0) << 15 |
               (is_multi_message_command ? 1 : 0) << 14 |
               unknown_counter << 12 |
               dst_node_id << 6 |
               src_node_id << 0;
    }

    uint8_t ZehnderComfoAirQ::get_command_next_sequence_number_()
    {
        this->command_sequence_number_ = (this->command_sequence_number_ + 1) & 0x3;
        return this->command_sequence_number_;
    }

    void ZehnderComfoAirQ::send_can_message_(uint32_t can_id, bool remote_transmission_request, const std::vector<uint8_t> &data)
    {
        ESP_LOGD(TAG, "Send can message: id: 0x%08lx (pdo_id: %ld), rtr: %d, size: %d, content: %s", can_id, can_id >> 14,
                 remote_transmission_request, data.size(), format_hex_pretty(data).c_str());

        if (parent_ == nullptr)
        {
            ESP_LOGE(TAG, "Parent not set, exiting send_can_message_.");
            return;
        }

        // We could have also called send_data directly, but using CanbusSendAction seem to be a bit more future-proof
        // id(parent_).send_data(can_id, true, true, {});

        CanbusSendAction<> canbus_send_action;
        canbus_send_action.set_parent(parent_);
        canbus_send_action.set_can_id(can_id);
        // use_extended_id as already been set globally on parent
        if (remote_transmission_request)
            canbus_send_action.set_remote_transmission_request(true);
        canbus_send_action.set_data_static(data);
        canbus_send_action.play();
    }
}
