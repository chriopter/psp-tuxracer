// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace PspSaveFormat {
using Files = std::array<std::string, 3>;
constexpr size_t Capacity = 256 * 1024;
constexpr size_t HeaderSize = 28;
inline uint32_t crc(const uint8_t *p, size_t n, bool blankChecksum = false) {
  uint32_t value = ~0u;
  for (size_t at = 0; at < n; ++at) {
    value ^= blankChecksum && at >= 24 && at < 28 ? 0 : p[at];
    for (int i = 0; i < 8; ++i)
      value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1u)));
  }
  return ~value;
}
inline void put(std::vector<uint8_t> &b, size_t at, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b[at + i] = v >> (8 * i);
}
inline uint32_t get(const uint8_t *b, size_t at) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i)
    v |= uint32_t(b[at + i]) << (8 * i);
  return v;
}
inline bool encode(const Files &files, std::vector<uint8_t> &out) {
  size_t total = HeaderSize;
  for (const auto &s : files) {
    if (s.size() > Capacity - total || s.find('\0') != std::string::npos)
      return false;
    total += s.size();
  }
  out.assign(HeaderSize, 0);
  const char magic[] = "ETRPSP1";
  for (int i = 0; i < 8; ++i)
    out[i] = magic[i];
  put(out, 8, 1);
  for (size_t i = 0; i < files.size(); ++i) {
    put(out, 12 + 4 * i, files[i].size());
    out.insert(out.end(), files[i].begin(), files[i].end());
  }
  put(out, 24, crc(out.data(), out.size()));
  return true;
}
inline bool decode(const uint8_t *data, size_t size, Files &out) {
  if (!data || size < HeaderSize || size > Capacity)
    return false;
  const char magic[] = "ETRPSP1";
  for (int i = 0; i < 8; ++i)
    if (data[i] != uint8_t(magic[i]))
      return false;
  if (get(data, 8) != 1)
    return false;
  size_t total = HeaderSize;
  for (size_t i = 0; i < 3; ++i) {
    size_t n = get(data, 12 + 4 * i);
    if (n > size - total)
      return false;
    total += n;
  }
  if (total != size || get(data, 24) != crc(data, size, true))
    return false;
  Files decoded;
  size_t offset = HeaderSize;
  for (size_t i = 0; i < 3; ++i) {
    size_t n = get(data, 12 + 4 * i);
    decoded[i].assign(reinterpret_cast<const char *>(data + offset), n);
    if (decoded[i].find('\0') != std::string::npos)
      return false;
    offset += n;
  }
  out = decoded; // Invalid/corrupt input never changes the caller's data.
  return true;
}
} // namespace PspSaveFormat
