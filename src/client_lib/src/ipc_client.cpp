#include "ipc_client.h"
#include <nlohmann/json.hpp>
#include <iostream>

namespace APClientLib {

using json = nlohmann::json;

IPCClient::IPCClient(const std::string& pipe_name)
    : pipe_name_(pipe_name), pipe_handle_(INVALID_HANDLE_VALUE) {
}

IPCClient::~IPCClient() {
    disconnect();
}

bool IPCClient::connect() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        return true; // Already connected
    }

    // Try to open the named pipe
    std::string full_pipe_name = "\\\\.\\pipe\\" + pipe_name_;

    pipe_handle_ = CreateFileA(
        full_pipe_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_PIPE_BUSY) {
            // Wait for pipe to become available (up to 5 seconds)
            if (WaitNamedPipeA(full_pipe_name.c_str(), 5000)) {
                pipe_handle_ = CreateFileA(
                    full_pipe_name.c_str(),
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr
                );
            }
        }
    }

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        set_error("Failed to connect to pipe: " + pipe_name_);
        return false;
    }

    // Set pipe to message mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe_handle_, &mode, nullptr, nullptr)) {
        set_error("Failed to set pipe mode");
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    return true;
}

void IPCClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pipe_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_handle_);
        pipe_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool IPCClient::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pipe_handle_ != INVALID_HANDLE_VALUE;
}

bool IPCClient::send_message(const std::string& type, const std::string& mod_id, const std::string& data_json) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        set_error("Not connected to pipe");
        return false;
    }

    // Construct JSON message
    json message;
    message["type"] = type;
    message["mod_id"] = mod_id;

    // Parse data_json if it's valid JSON, otherwise treat as string
    try {
        message["data"] = json::parse(data_json);
    } catch (...) {
        message["data"] = data_json;
    }

    std::string message_str = message.dump();

    // Add newline delimiter
    message_str += "\n";

    DWORD bytes_written = 0;
    if (!WriteFile(pipe_handle_, message_str.c_str(), static_cast<DWORD>(message_str.size()), &bytes_written, nullptr)) {
        set_error("Failed to write to pipe");
        return false;
    }

    return true;
}

bool IPCClient::poll_messages(std::vector<IPCMessage>& out_messages) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pipe_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Check if data is available (non-blocking)
    DWORD bytes_available = 0;
    if (!PeekNamedPipe(pipe_handle_, nullptr, 0, nullptr, &bytes_available, nullptr)) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE) {
            set_error("Pipe connection broken");
            CloseHandle(pipe_handle_);
            pipe_handle_ = INVALID_HANDLE_VALUE;
        }
        return false;
    }

    if (bytes_available == 0) {
        return true; // No messages, but not an error
    }

    // Read all available messages
    while (bytes_available > 0) {
        IPCMessage message;
        if (read_message(message)) {
            out_messages.push_back(std::move(message));
        } else {
            break;
        }

        // Check for more messages
        if (!PeekNamedPipe(pipe_handle_, nullptr, 0, nullptr, &bytes_available, nullptr)) {
            break;
        }
    }

    return true;
}

bool IPCClient::read_message(IPCMessage& out_message) {
    // Read until newline delimiter
    std::string buffer;
    char ch;
    DWORD bytes_read = 0;

    while (true) {
        if (!ReadFile(pipe_handle_, &ch, 1, &bytes_read, nullptr)) {
            set_error("Failed to read from pipe");
            return false;
        }

        if (bytes_read == 0) {
            break;
        }

        if (ch == '\n') {
            break; // End of message
        }

        buffer += ch;
    }

    if (buffer.empty()) {
        return false;
    }

    // Parse JSON message
    try {
        json message = json::parse(buffer);
        out_message.type = message.value("type", "");
        out_message.mod_id = message.value("mod_id", "");

        // Serialize data back to JSON string
        if (message.contains("data")) {
            out_message.data = message["data"].dump();
        } else {
            out_message.data = "{}";
        }

        return true;
    } catch (const std::exception& e) {
        set_error(std::string("Failed to parse message JSON: ") + e.what());
        return false;
    }
}

std::string IPCClient::get_last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

void IPCClient::set_error(const std::string& error) {
    last_error_ = error;
}

} // namespace APClientLib
