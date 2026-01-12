#include "ap_client_lib.h"
#include <chrono>
#include <iostream>

namespace APClientLib {

APClient::APClient()
    : ipc_client_(std::make_unique<APIPCClient>()) {
}

APClient::~APClient() {
    // APIPCClient destructor will handle disconnection
}

VoidResult APClient::init(const std::string& mod_id, const std::string& pipe_name) {
    mod_id_ = mod_id;

    // Connect to framework IPC server
    auto result = ipc_client_->connect(pipe_name, std::chrono::milliseconds(5000));
    if (!result.success) {
        return VoidResult::failure("Failed to connect to framework: " + result.error_message);
    }

    // Set up message callback to handle incoming messages
    ipc_client_->set_message_callback([this](const IPCMessage& msg) {
        handle_incoming_message(msg);
    });

    return VoidResult::success_result();
}

VoidResult APClient::register_with_framework(const nlohmann::json& capabilities, bool is_priority) {
    nlohmann::json reg_data;
    reg_data["mod_id"] = mod_id_;
    reg_data["capabilities"] = capabilities;
    reg_data["is_priority"] = is_priority;

    return ipc_client_->register_mod(mod_id_, reg_data);
}

VoidResult APClient::send_location_check(int64_t location_id) {
    return ipc_client_->send_location_check(location_id);
}

VoidResult APClient::send_location_checks(const std::vector<int64_t>& location_ids) {
    return ipc_client_->send_location_checks(location_ids);
}

VoidResult APClient::send_message(const IPCMessage& message) {
    return ipc_client_->send(message);
}

VoidResult APClient::send_command(const std::string& cmd, const nlohmann::json& data) {
    IPCMessage msg;
    msg.type = "command";
    msg.from_mod_id = mod_id_;
    msg.to_mod_id = "framework";
    msg.data = {
        {"cmd", cmd},
        {"data", data}
    };

    return ipc_client_->send(msg);
}

void APClient::poll() {
    // Get all pending messages and process them
    auto messages = ipc_client_->get_messages();

    // DEBUG: Log message count
    if (!messages.empty()) {
        std::cout << "[APClient::poll] Processing " << messages.size() << " messages" << std::endl;
    }

    for (const auto& msg : messages) {
        std::cout << "[APClient::poll] Handling message type: " << msg.type << std::endl;
        handle_incoming_message(msg);
        std::cout << "[APClient::poll] Message handled" << std::endl;
    }
}

std::vector<IPCMessage> APClient::get_messages() {
    return ipc_client_->get_messages();
}

void APClient::on_received_items(ReceivedItemsCallback callback) {
    received_items_callback_ = callback;
}

void APClient::on_location_info(LocationInfoCallback callback) {
    location_info_callback_ = callback;
}

void APClient::on_lifecycle_change(GenericMessageCallback callback) {
    lifecycle_callback_ = callback;
}

void APClient::on_message(const std::string& message_type, GenericMessageCallback callback) {
    message_callbacks_[message_type] = callback;
}

bool APClient::is_connected() const {
    return ipc_client_->is_connected();
}

void APClient::handle_incoming_message(const IPCMessage& msg) {
    // Handle specific message types
    if (msg.type == "ap_message") {
        // AP server message routed from framework
        std::string ap_type = msg.data.value("ap_type", "");

        if (ap_type == "ReceivedItems" && received_items_callback_) {
            received_items_callback_(msg.data);
        } else if (ap_type == "LocationInfo" && location_info_callback_) {
            location_info_callback_(msg.data);
        }
    } else if (msg.type == "lifecycle" && lifecycle_callback_) {
        lifecycle_callback_(msg);
    }

    // Check for generic message callbacks
    auto it = message_callbacks_.find(msg.type);
    if (it != message_callbacks_.end() && it->second) {
        it->second(msg);
    }
}

} // namespace APClientLib