#pragma once

#define FOUR_CC(a, b, c, d) \
  (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

#define SWIZZLE_BYTES(a) \
  ((((u8 *)&a)[0] << 24) | (((u8 *)&a)[1] << 16) | (((u8 *)&a)[2] << 8) | (((u8 *)&a)[3] << 0))

u32 convert_from_big_endian(u32 a) {
  // @Incomplete: don't do this if the machine is big-endian
  return SWIZZLE_BYTES(a);
}

#pragma pack(push, 1)
struct PNG_Chunk_Header {
  u32 length; // big-endian
  u32 type;
};

struct PNG_Chunk_Footer {
  u32 crc;
};

struct PNG_IHDR_Chunk {
  u32 width;  // big-endian
  u32 height; // big-endian

  // NOTE(nick): number of bits per sample / palette index (not per pixel)
  u8  bit_depth;  // 1, 2, 4, 8, or 16 

  // 0 = greyscale
  // 2 = truecolor
  // 3 = index-color
  // 4 = greyscale with alpha
  // 6 = truecolor with alpha
  u8  color_type; // 0, 2, 3, 4, or 6

  u8  compression_method; // must be 0
  u8  filter_method; // must be 0

  // 0 = no interlace
  // 1 = Adam7 interlace
  u8  interlace_method; // 0, 1
};

struct PNG_IDAT_Header {
  u8 zlib_method_flags;
  u8 additional_flags;
};

struct PNG_IDAT_Footer {
  u32 check;
};

struct Huffman_Decode_Buffer {
  u32 count;
  u8 *data;

  u32 bit_count;
  u32 bit_buffer;
};
#pragma pack(pop)

void flush_byte(Huffman_Decode_Buffer *buf) {
  buf->bit_count = 0;
  buf->bit_buffer = 0;
}

u32 consume_bits(Huffman_Decode_Buffer *buf, u32 num_bits) {
  assert(num_bits <= 16);

  u32 result = 0;

  while (buf->bit_count < num_bits)
  {
    u8 byte = *buf->data;
    buf->data += 1;
    buf->count -= 1;

    buf->bit_buffer |= (byte << buf->bit_count);
    buf->bit_count += 8;
  }

  if (buf->bit_count >= num_bits)
  {
    buf->bit_count -= num_bits;
    result = buf->bit_buffer & ((1 << num_bits) - 1);
    buf->bit_buffer >>= num_bits;
  }

  return result;
}

bool png_parse(Arena *arena, String raw) {
  u8 *at = (u8 *)raw.data;

  if (((u32 *)at)[0] != FOUR_CC(137, 'P', 'N', 'G') || ((u32 *)at)[1] != FOUR_CC(13, 10, 26, 10)) {
    print("[png] Invalid header!");
    return false;
  }

  at += 2 * sizeof(u32);

  // NOTE(nick): IHDR must be the first chunk
  PNG_Chunk_Header *ihdr_header = (PNG_Chunk_Header *)at;
  at += sizeof(PNG_Chunk_Header);

  if (ihdr_header->type != FOUR_CC('I', 'H', 'D', 'R')) {
    print("[png] IHDR must be first chunk!");
    return false;
  }

  PNG_IHDR_Chunk *ihdr = (PNG_IHDR_Chunk *)at;
  at += sizeof(PNG_IHDR_Chunk);

  u32 width = convert_from_big_endian(ihdr->width);
  u32 height = convert_from_big_endian(ihdr->height);

  //u8 *output_buffer = arena_alloc(arena, width * height * sizeof(u32));

  if (width == 0 || height == 0) {
    print("[png] width and height cannot be zero!");
    return false;
  }

  at += sizeof(PNG_Chunk_Footer);

  print("bit_depth: %d\n", ihdr->bit_depth);
  print("color_type: %d\n", ihdr->color_type);
  print("compression_method: %d\n", ihdr->compression_method);
  print("filter_method: %d\n", ihdr->filter_method);
  print("interlace_method: %d\n", ihdr->interlace_method);

  u8 *at_end = (u8 *)raw.data + raw.count;

  while (at < at_end) {
    PNG_Chunk_Header *header = (PNG_Chunk_Header *)at;
    at += sizeof(PNG_Chunk_Header);

    u32 chunk_length = convert_from_big_endian(header->length);
    u32 chunk_type = header->type;

    u8 *chunk_data = at;
    at += chunk_length;

    switch (chunk_type) {
      case FOUR_CC('P','L','T','E'): {
      } break;

      case FOUR_CC('t','R','N','S'): {
      } break;

      case FOUR_CC('I','D','A','T'): {
        PNG_IDAT_Header *header = (PNG_IDAT_Header *)chunk_data;
        chunk_data += sizeof(PNG_IDAT_Header);

        // @Incomplete: there may be multiple IDAT chunks (if so, consecutive)

        u8 cm = header->zlib_method_flags & 0xF; // According to the spec, this will always be 8
        u8 cinfo = (header->zlib_method_flags >> 4);
        u8 fcheck = header->additional_flags & 0x1F;
        u8 fdict = (header->additional_flags >> 5) & 0x1;
        u8 flevel = (header->additional_flags >> 6);

        if (cm == 8 && fdict == 0) {
          Huffman_Decode_Buffer buf = {};
          buf.count = chunk_length;
          buf.data = chunk_data;

          print("cm: %d\n", cm);
          print("cinfo: %d\n", cinfo);
          print("fcheck: %d\n", fcheck);
          print("fdict: %d\n", fdict);
          print("flevel: %d\n", flevel);
          print("\n");

          u8 bfinal = 0;
          u8 btype = 0;

          do
          {
            bfinal = consume_bits(&buf, 1);
            btype = consume_bits(&buf, 2);

            print("bfinal: %d\n", bfinal);
            print("btype: %d\n", btype);

            if (btype == 0) {
              flush_byte(&buf);

              u8 len = consume_bits(&buf, 16);
              u8 nlen = consume_bits(&buf, 16);

              if (len != ~nlen) {
                // error
              }

              print("len: %d\n", len);
            }
            else if (btype == 1 || btype == 2) {

              if (btype == 2) {
                u32 hlit = 257 + consume_bits(&buf, 5);
                u32 hdist = 1 + consume_bits(&buf, 5);
                u32 hclen = 4 + consume_bits(&buf, 4);

                print("hlit: %d\n", hlit);
                print("hdist: %d\n", hdist);
                print("hclen: %d\n", hclen);

                u8 lenlens[19] = {0};
                u32 hclen_swizzle[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

                for (u32 i = 0; i < hclen; i ++) {
                  lenlens[hclen_swizzle[i]] = (u8)consume_bits(&buf, 3);
                }
              }

              for (;;) {
                u32 value = 0; //huffman_decode();
                break;

                if (value < 256) {
                  // copy value (literal byte) to output stream
                } else {

                  if (value == 256) { // End of block
                    break;
                  }

                  // value = 257..285
                  // decode distance from input stream
                  // move backwards distance bytes in the output
                  // stream and copy length bytes from this position to the output stream
                }
              }

            }
            else {
              // btype 3, which is invalid
            }


          } while (!bfinal);

        }

      } break;

      case FOUR_CC('I','E','N','D'): {
      } break;

      default: {
      } break;
    }

    at += sizeof(PNG_Chunk_Footer);
  }

  // print bytes remaining
  print("%S", string_slice(raw, (i64)(at - raw.data)));

  return false;
}
