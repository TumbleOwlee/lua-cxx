#include "modbus.h"
#include <iostream>

modbus::Address::Address(uint16_t address)
    : address(address)
{
}

modbus::Modbus::Modbus(Host host)
    : host(std::move(host))
{
}

auto modbus::Modbus::read(Read read, Address address, size_t count) -> std::expected<RegisterValues, Error>
{
    std::cerr << "Read: " << (int)read << ", Address: " << address.address << ", Count: " << count << std::endl;
    return { { (uint16_t)read, (uint16_t)address.address, (uint16_t)count } };
}

auto modbus::Modbus::write(modbus::Write write, modbus::Address address, RegisterValues&& values) -> bool
{
    std::cerr << "Write: " << (int)write << ", Address: " << address.address << ", Values: ";
    for (auto const& v : values) {
        std::cerr << v << ", ";
    }
    std::cerr << std::endl;
    return address.address > 100;
}
