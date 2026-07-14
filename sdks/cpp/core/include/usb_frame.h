#pragma once

#include "usb_protocol.h"

#include <cstdint>

#pragma pack(push, 1)
struct RocketBxHeader {
  uint8_t magic[8];
  uint64_t file_size;
  uint8_t frame_kind;
  uint8_t filename[15];
};
#pragma pack(pop)

static_assert(sizeof(RocketBxHeader) == usb_protocol::kHeaderSize, "ROCKETBX header is 32 bytes");

inline void fill_rocketbx_header(RocketBxHeader* h, uint64_t size, uint8_t kind,
                                 const char* name = nullptr) {
  for (int i = 0; i < 8; ++i) h->magic[i] = static_cast<uint8_t>(usb_protocol::kHeaderMagic[i]);
  h->file_size = size;
  h->frame_kind = kind;
  for (int i = 0; i < 15; ++i) h->filename[i] = 0;
  if (!name) return;
  for (int i = 0; i < usb_protocol::kFrameFilenameMax && name[i]; ++i) {
    h->filename[i] = static_cast<uint8_t>(name[i]);
  }
}
