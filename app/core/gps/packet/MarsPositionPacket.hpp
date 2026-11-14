#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "core/gps/Position.hpp"

namespace mars {

	class PositionPacket {
	public:
		static constexpr std::size_t kSize = 16;
		using Buffer = std::array<std::uint8_t, kSize>;

		[[nodiscard]] static constexpr Buffer encode(t_position position, std::uint16_t sequenceCount) noexcept
		{
			Buffer packet{};
			write(packet, 0, kIdentification, 2);
			write(packet, 2, kUnsegmented | (sequenceCount & kSequenceMask), 2);
			write(packet, 4, kDataLength, 2);
			write(packet, 6, static_cast<std::uint32_t>(position.latitudeE7), 4);
			write(packet, 10, position.longitudeEastE7, 4);
			write(packet, 14, crc16(std::span{packet}.first<14>()), 2);
			return packet;
		}

		[[nodiscard]] static constexpr std::optional<t_position> decode(std::span<const std::uint8_t, kSize> packet) noexcept
		{
			if (read(packet, 14, 2) != crc16(packet.first<14>())
				|| read(packet, 0, 2) != kIdentification
				|| read(packet, 4, 2) != kDataLength) {
				return std::nullopt;
			}

			const t_position position{
				static_cast<std::int32_t>(read(packet, 6, 4)),
				read(packet, 10, 4),
			};
			return position.isValid() ? std::optional{position} : std::nullopt;
		}

	private:
		static constexpr std::uint32_t kApid = 0x1A2;
		static constexpr std::uint32_t kIdentification = kApid;
		static constexpr std::uint32_t kUnsegmented = 0xC000;
		static constexpr std::uint32_t kSequenceMask = 0x3FFF;
		static constexpr std::uint32_t kDataLength = kSize - 6 - 1;

		static constexpr void write(Buffer& packet, std::size_t offset, std::uint32_t value, std::size_t byteCount) noexcept
		{
			for (std::size_t index = 0; index < byteCount; ++index) {
				packet[offset + index] = static_cast<std::uint8_t>(value >> (8 * (byteCount - 1 - index)));
			}
		}

		[[nodiscard]] static constexpr std::uint32_t read(
			std::span<const std::uint8_t, kSize> packet, std::size_t offset, std::size_t byteCount) noexcept
		{
			std::uint32_t value = 0;
			for (std::size_t index = 0; index < byteCount; ++index) {
				value = (value << 8) | packet[offset + index];
			}
			return value;
		}

		[[nodiscard]] static constexpr std::uint16_t crc16(std::span<const std::uint8_t> data) noexcept
		{
			std::uint16_t crc = 0xFFFF;
			for (const auto byte : data) {
				crc ^= static_cast<std::uint16_t>(byte << 8);
				for (int bit = 0; bit < 8; ++bit) {
					crc = static_cast<std::uint16_t>((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
				}
			}
			return crc;
		}
	};

	static_assert(PositionPacket::decode(PositionPacket::encode({184'446'000, 774'509'000}, 0))->longitudeEastE7 == 774'509'000);

}
