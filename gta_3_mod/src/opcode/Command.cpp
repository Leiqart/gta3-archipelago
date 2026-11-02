#include "Command.h"

#include <cmath>
#include <stdexcept>

namespace {
    void Word(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }
}

std::vector<std::uint8_t> Opcode::Encode(const Command& command) {
    std::vector<std::uint8_t> bytes;
    Word(bytes, static_cast<std::uint16_t>(command.id));
    std::uint16_t local = 0;
    for (const auto& parameter : command.parameters) {
        if (const auto* value = std::get_if<int>(&parameter)) {
            bytes.push_back(1); // SCRIPTPARAM_STATIC_INT_32BITS
            const auto bits = static_cast<std::uint32_t>(*value);
            Word(bytes, static_cast<std::uint16_t>(bits));
            Word(bytes, static_cast<std::uint16_t>(bits >> 16));
        } else if (const auto* floatValue = std::get_if<float>(&parameter)) {
            // GTA III's type 6 is a signed 16-bit value divided by 16, not IEEE float.
            const float scaled = *floatValue * 16.0f;
            if (!std::isfinite(scaled) || scaled < -32768.0f || scaled > 32767.0f)
                throw std::invalid_argument("GTA III SCM float is out of range");
            bytes.push_back(6);
            Word(bytes, static_cast<std::uint16_t>(static_cast<std::int16_t>(scaled)));
        } else if (const auto* text = std::get_if<std::string>(&parameter)) {
            if (text->size() > 8 || text->find('\0') != std::string::npos)
                throw std::invalid_argument("GTA III SCM text exceeds eight bytes");
            for (unsigned char letter : *text)
                if (letter > 127) throw std::invalid_argument("GTA III SCM text must be ASCII");
            // GTA III text labels have no parameter-type byte.
            bytes.insert(bytes.end(), text->begin(), text->end());
            bytes.insert(bytes.end(), 8 - text->size(), 0);
        } else {
            if (!std::get<int*>(parameter) || local >= 16)
                throw std::invalid_argument("Invalid GTA III SCM output variable");
            bytes.push_back(3); // SCRIPTPARAM_LOCAL_NUMBER_VARIABLE
            Word(bytes, local++);
        }
    }
    return bytes;
}
