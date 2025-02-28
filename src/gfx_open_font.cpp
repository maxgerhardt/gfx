#include <gfx_core.hpp>
#define STB_RECT_PACK_IMPLEMENTATION
#define STBRP_STATIC
#include "stb_rect_pack.h"
#define STBTT_STATIC
#include "stb_truetype_htcw.h"

#include <gfx_open_font.hpp>
// e.g. #define your own STBTT_ifloor/STBTT_iceil() to avoid math.h
#ifndef STBTT_ifloor
#include <math.h>
#define STBTT_ifloor(x) ((int)floor(x))
#define STBTT_iceil(x) ((int)ceil(x))
#endif

#ifndef STBTT_sqrt
#include <math.h>
#define STBTT_sqrt(x) sqrt(x)
#define STBTT_pow(x, y) pow(x, y)
#endif

#ifndef STBTT_fmod
#include <math.h>
#define STBTT_fmod(x, y) fmod(x, y)
#endif

#ifndef STBTT_cos
#include <math.h>
#define STBTT_cos(x) cos(x)
#define STBTT_acos(x) acos(x)
#endif

#ifndef STBTT_fabs
#include <math.h>
#define STBTT_fabs(x) fabs(x)
#endif

// #define your own functions "STBTT_malloc" / "STBTT_free" to avoid malloc.h
#ifndef STBTT_malloc
#include <stdlib.h>
#define STBTT_malloc(x, u) ((void)(u), malloc(x))
#define STBTT_free(x, u) ((void)(u), free(x))
#endif

#ifndef STBTT_assert
#include <assert.h>
#define STBTT_assert(x) assert(x)
#endif

#ifndef STBTT_strlen
#include <string.h>
#define STBTT_strlen(x) strlen(x)
#endif

#ifndef STBTT_memcpy
#include <string.h>
#define STBTT_memcpy memcpy
#define STBTT_memset memset
#endif

namespace stbtt {


#ifndef STBTT_MAX_OVERSAMPLE
#define STBTT_MAX_OVERSAMPLE 8
#endif

#if STBTT_MAX_OVERSAMPLE > 255
#error "STBTT_MAX_OVERSAMPLE cannot be > 255"
#endif

   typedef int stbtt__test_oversample_pow2[(STBTT_MAX_OVERSAMPLE & (STBTT_MAX_OVERSAMPLE - 1)) == 0 ? 1 : -1];

#ifndef STBTT_RASTERIZER_VERSION
#define STBTT_RASTERIZER_VERSION 2
#endif

#ifdef _MSC_VER
#define STBTT__NOTUSED(v) (void)(v)
#else
#define STBTT__NOTUSED(v) (void)sizeof(v)
#endif

   static stbtt::stbtt_uint8 read_uint8(io::stream *stream)
   {
      uint8_t result;
      stream->read(&result, 1);
      return result;
   }
   static stbtt::stbtt_int8 read_int8(io::stream *stream)
   {
      int8_t result;
      stream->read((uint8_t *)&result, 1);
      return result;
   }
   static stbtt::stbtt_uint16 read_uint16(io::stream *stream)
   {
      uint8_t result[2];
      stream->read(result, 2);
      return result[0] * 256 + result[1];
   }
   static stbtt::stbtt_int16 read_int16(io::stream *stream)
   {
      uint8_t result[2];
      stream->read(result, 2);
      return result[0] * 256 + result[1];
   }
   static stbtt::stbtt_uint32 read_uint32(io::stream *stream)
   {
      uint8_t result[4];
      stream->read(result, 4);
      return (result[0] << 24) + (result[1] << 16) + (result[2] << 8) + result[3];
   }
   /*static stbtt_int32 read_int32(io::stream *stream)
   {
      uint8_t result[4];
      stream->read(result, 4);
      return (result[0] << 24) + (result[1] << 16) + (result[2] << 8) + result[3];
   }*/

   //////////////////////////////////////////////////////////////////////////
   //
   // stbtt::stbtt__buf helpers to parse data from file
   //

   static stbtt::stbtt_uint8 stbtt__buf_get8(stbtt::stbtt__buf *b)
   {
      if (b->cursor >= b->size)
         return 0;
      b->stream->seek(b->cursor + b->offset);
      ++b->cursor;
      return read_uint8(b->stream);
   }

   static stbtt::stbtt_uint8 stbtt__buf_peek8(stbtt::stbtt__buf *b)
   {
      if (b->cursor >= b->size)
         return 0;
      b->stream->seek(b->cursor + b->offset);
      return read_uint8(b->stream);
   }

   static void stbtt__buf_seek(stbtt::stbtt__buf *b, int o)
   {
      STBTT_assert(!(o > b->size || o < 0));
      b->cursor = (o > b->size || o < 0) ? b->size : o;
   }

   static void stbtt__buf_skip(stbtt::stbtt__buf *b, int o)
   {
      stbtt__buf_seek(b, b->cursor + o);
   }

   static stbtt::stbtt_uint32 stbtt__buf_get(stbtt::stbtt__buf *b, int n)
   {
      stbtt::stbtt_uint32 v = 0;
      int i;
      STBTT_assert(n >= 1 && n <= 4);
      for (i = 0; i < n; i++)
         v = (v << 8) | stbtt__buf_get8(b);
      return v;
   }

   static stbtt::stbtt__buf stbtt__new_buf(io::stream *stream, const stbtt_uint32 offset, size_t size)
   {
      stbtt::stbtt__buf r;
      STBTT_assert(size < 0x40000000);
      r.stream = stream;
      r.offset = offset;
      r.size = (int)size;
      r.cursor = 0;
      return r;
   }

#define stbtt__buf_get16(b) stbtt__buf_get((b), 2)
#define stbtt__buf_get32(b) stbtt__buf_get((b), 4)

   static stbtt::stbtt__buf stbtt__buf_range(const stbtt::stbtt__buf *b, int o, int s)
   {
      stbtt::stbtt__buf r = stbtt__new_buf(NULL, 0, 0);
      if (o < 0 || s < 0 || o > b->size || s > b->size - o)
         return r;
      r.stream = b->stream;
      r.offset = b->offset+o;
      r.size = s;
      return r;
   }

   static stbtt::stbtt__buf stbtt__cff_get_index(stbtt::stbtt__buf *b)
   {
      int count, start, offsize;
      start = b->cursor;
      count = stbtt__buf_get16(b);
      if (count)
      {
         offsize = stbtt__buf_get8(b);
         STBTT_assert(offsize >= 1 && offsize <= 4);
         stbtt__buf_skip(b, offsize * count);
         stbtt__buf_skip(b, stbtt__buf_get(b, offsize) - 1);
      }
      return stbtt__buf_range(b, start, b->cursor - start);
   }

   static stbtt_uint32 stbtt__cff_int(stbtt::stbtt__buf *b)
   {
      int b0 = stbtt__buf_get8(b);
      if (b0 >= 32 && b0 <= 246)
         return b0 - 139;
      else if (b0 >= 247 && b0 <= 250)
         return (b0 - 247) * 256 + stbtt__buf_get8(b) + 108;
      else if (b0 >= 251 && b0 <= 254)
         return -(b0 - 251) * 256 - stbtt__buf_get8(b) - 108;
      else if (b0 == 28)
         return stbtt__buf_get16(b);
      else if (b0 == 29)
         return stbtt__buf_get32(b);
      STBTT_assert(0);
      return 0;
   }

   static void stbtt__cff_skip_operand(stbtt::stbtt__buf *b)
   {
      int v, b0 = stbtt__buf_peek8(b);
      STBTT_assert(b0 >= 28);
      if (b0 == 30)
      {
         stbtt__buf_skip(b, 1);
         while (b->cursor < b->size)
         {
            v = stbtt__buf_get8(b);
            if ((v & 0xF) == 0xF || (v >> 4) == 0xF)
               break;
         }
      }
      else
      {
         stbtt__cff_int(b);
      }
   }

   static stbtt::stbtt__buf stbtt__dict_get(stbtt::stbtt__buf *b, int key)
   {
      stbtt__buf_seek(b, 0);
      while (b->cursor < b->size)
      {
         int start = b->cursor, end, op;
         while (stbtt__buf_peek8(b) >= 28)
            stbtt__cff_skip_operand(b);
         end = b->cursor;
         op = stbtt__buf_get8(b);
         if (op == 12)
            op = stbtt__buf_get8(b) | 0x100;
         if (op == key)
            return stbtt__buf_range(b, start, end - start);
      }
      return stbtt__buf_range(b, 0, 0);
   }

   static void stbtt__dict_get_ints(stbtt::stbtt__buf *b, int key, int outcount, stbtt_uint32 *out)
   {
      int i;
      stbtt::stbtt__buf operands = stbtt__dict_get(b, key);
      for (i = 0; i < outcount && operands.cursor < operands.size; i++)
         out[i] = stbtt__cff_int(&operands);
   }

   static int stbtt__cff_index_count(stbtt::stbtt__buf *b)
   {
      stbtt__buf_seek(b, 0);
      return stbtt__buf_get16(b);
   }

   static stbtt::stbtt__buf stbtt__cff_index_get(stbtt::stbtt__buf b, int i)
   {
      int count, offsize, start, end;
      stbtt__buf_seek(&b, 0);
      count = stbtt__buf_get16(&b);
      offsize = stbtt__buf_get8(&b);
      STBTT_assert(i >= 0 && i < count);
      STBTT_assert(offsize >= 1 && offsize <= 4);
      stbtt__buf_skip(&b, i * offsize);
      start = stbtt__buf_get(&b, offsize);
      end = stbtt__buf_get(&b, offsize);
      return stbtt__buf_range(&b, 2 + (count + 1) * offsize + start, end - start);
   }

   //////////////////////////////////////////////////////////////////////////
   //
   // accessors to parse data from file
   //

   // on platforms that don't allow misaligned reads, if we want to allow
   // truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE

#define ttBYTE(p) (*(stbtt_uint8 *)(p))
#define ttCHAR(p) (*(stbtt_int8 *)(p))
#define ttFixed(p) ttLONG(p)

   /*static stbtt_uint16 ttUSHORT(stbtt_uint8 *p)
   {
      return p[0] * 256 + p[1];
   }*/
   //static stbtt_int16 ttSHORT(stbtt_uint8 *p) { return p[0] * 256 + p[1]; }
   //static stbtt_uint32 ttULONG(stbtt_uint8 *p) { return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3]; }
   //static stbtt_int32 ttLONG(stbtt_uint8 *p) { return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3]; }

#define stbtt_tag4(p, c0, c1, c2, c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define stbtt_tag(p, str) stbtt_tag4(p, str[0], str[1], str[2], str[3])

   /*static int stbtt__isfont(stbtt_uint8 *font)
   {
      // check the version number
      if (stbtt_tag4(font, '1', 0, 0, 0))
         return 1; // TrueType 1
      if (stbtt_tag(font, "typ1"))
         return 1; // TrueType with type 1 font -- we don't support this!
      if (stbtt_tag(font, "OTTO"))
         return 1; // OpenType with CFF
      if (stbtt_tag4(font, 0, 1, 0, 0))
         return 1; // OpenType 1.0
      if (stbtt_tag(font, "true"))
         return 1; // Apple specification for TrueType fonts
      return 0;
   }*/

   // @OPTIMIZE: binary search
   /*static stbtt_uint32 stbtt__find_table(stbtt_uint8 *data, stbtt_uint32 fontstart, const char *tag)
{
   stbtt_int32 num_tables = ttUSHORT(data+fontstart+4);
   stbtt_uint32 tabledir = fontstart + 12;
   stbtt_int32 i;
   for (i=0; i < num_tables; ++i) {
      stbtt_uint32 loc = tabledir + 16*i;
      if (stbtt_tag(data+loc+0, tag))
         return ttULONG(data+loc+8);
   }
   return 0;
}*/

   static stbtt_uint32 stbtt__find_table(io::stream *stream, stbtt_uint32 fontstart, const char *tag)
   {
      stream->seek(fontstart + 4);
      stbtt_int32 num_tables = read_uint16(stream);
      stbtt_uint32 tabledir = fontstart + 12;
      stbtt_int32 i;
      uint8_t data[4];
      for (i = 0; i < num_tables; ++i)
      {
         stbtt_uint32 loc = tabledir + 16 * i;
         stream->seek(loc);
         stream->read(data, 4);
         if (stbtt_tag(data, tag))
         {
            stream->seek(loc + 8);
            return read_uint32(stream);
         }
      }
      return 0;
   }

   /*static int stbtt_GetFontOffsetForIndex_internal(unsigned char *font_collection, int index)
   {
      // if it's just a font, there's only one valid index
      if (stbtt__isfont(font_collection))
         return index == 0 ? 0 : -1;

      // check if it's a TTC
      if (stbtt_tag(font_collection, "ttcf"))
      {
         // version 1?
         if (ttULONG(font_collection + 4) == 0x00010000 || ttULONG(font_collection + 4) == 0x00020000)
         {
            stbtt_int32 n = ttLONG(font_collection + 8);
            if (index >= n)
               return -1;
            return ttULONG(font_collection + 12 + index * 4);
         }
      }
      return -1;
   }*/

   /*static int stbtt_GetNumberOfFonts_internal(unsigned char *font_collection)
   {
      // if it's just a font, there's only one valid font
      if (stbtt__isfont(font_collection))
         return 1;

      // check if it's a TTC
      if (stbtt_tag(font_collection, "ttcf"))
      {
         // version 1?
         if (ttULONG(font_collection + 4) == 0x00010000 || ttULONG(font_collection + 4) == 0x00020000)
         {
            return ttLONG(font_collection + 8);
         }
      }
      return 0;
   }*/

   static stbtt::stbtt__buf stbtt__get_subrs(stbtt::stbtt__buf cff, stbtt::stbtt__buf fontdict)
   {
      stbtt_uint32 subrsoff = 0, private_loc[2] = {0, 0};
      stbtt::stbtt__buf pdict;
      stbtt__dict_get_ints(&fontdict, 18, 2, private_loc);
      if (!private_loc[1] || !private_loc[0])
         return stbtt__new_buf(NULL, 0, 0);
      pdict = stbtt__buf_range(&cff, private_loc[1], private_loc[0]);
      stbtt__dict_get_ints(&pdict, 19, 1, &subrsoff);
      if (!subrsoff)
         return stbtt__new_buf(NULL, 0, 0);
      stbtt__buf_seek(&cff, private_loc[1] + subrsoff);
      return stbtt__cff_get_index(&cff);
   }

   // since most people won't use this, find this table the first time it's needed
   /*static int stbtt__get_svg(stbtt_fontinfo *info)
   {
      stbtt_uint32 t;
      if (info->svg < 0)
      {
         t = stbtt__find_table(info->stream, info->fontstart, "SVG ");
         if (t)
         {
            info->stream->seek(t + 2);
            stbtt_uint32 offset = read_uint32(info->stream);
            info->svg = t + offset;
         }
         else
         {
            info->svg = 0;
         }
      }
      return info->svg;
   }*/

   static int stbtt_InitFont_internal(stbtt_fontinfo *info, io::stream *stream, int fontstart)
   {
      stbtt_uint32 cmap, t;
      stbtt_int32 i, numTables;

      info->stream = stream;
      //info->data = data;
      info->fontstart = fontstart;
      info->cff = stbtt__new_buf(NULL, 0, 0);

      cmap = stbtt__find_table(stream, fontstart, "cmap");       // required
      info->loca = stbtt__find_table(stream, fontstart, "loca"); // required
      info->head = stbtt__find_table(stream, fontstart, "head"); // required
      info->glyf = stbtt__find_table(stream, fontstart, "glyf"); // required
      info->hhea = stbtt__find_table(stream, fontstart, "hhea"); // required
      info->hmtx = stbtt__find_table(stream, fontstart, "hmtx"); // required
      info->kern = stbtt__find_table(stream, fontstart, "kern"); // not required
      info->gpos = stbtt__find_table(stream, fontstart, "GPOS"); // not required

      if (!cmap || !info->head || !info->hhea || !info->hmtx)
         return 0;
      if (info->glyf)
      {
         // required for truetype
         if (!info->loca)
            return 0;
      }
      else
      {
         // initialization for CFF / Type2 fonts (OTF)
         stbtt::stbtt__buf b, topdict, topdictidx;
         stbtt_uint32 cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
         stbtt_uint32 cff;

         cff = stbtt__find_table(stream, fontstart, "CFF ");
         if (!cff)
            return 0;

         info->fontdicts = stbtt__new_buf(NULL, 0, 0);
         info->fdselect = stbtt__new_buf(NULL, 0, 0);

         // @TODO this should use size from table (not 512MB)
         info->cff = stbtt__new_buf(stream, cff, 512 * 1024 * 1024);
         b = info->cff;

         // read the header
         stbtt__buf_skip(&b, 2);
         stbtt__buf_seek(&b, stbtt__buf_get8(&b)); // hdrsize

         // @TODO the name INDEX could list multiple fonts,
         // but we just use the first one.
         stbtt__cff_get_index(&b); // name INDEX
         topdictidx = stbtt__cff_get_index(&b);
         topdict = stbtt__cff_index_get(topdictidx, 0);
         stbtt__cff_get_index(&b); // string INDEX
         info->gsubrs = stbtt__cff_get_index(&b);

         stbtt__dict_get_ints(&topdict, 17, 1, &charstrings);
         stbtt__dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
         stbtt__dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
         stbtt__dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
         info->subrs = stbtt__get_subrs(b, topdict);

         // we only support Type 2 charstrings
         if (cstype != 2)
            return 0;
         if (charstrings == 0)
            return 0;

         if (fdarrayoff)
         {
            // looks like a CID font
            if (!fdselectoff)
               return 0;
            stbtt__buf_seek(&b, fdarrayoff);
            info->fontdicts = stbtt__cff_get_index(&b);
            info->fdselect = stbtt__buf_range(&b, fdselectoff, b.size - fdselectoff);
         }

         stbtt__buf_seek(&b, charstrings);
         info->charstrings = stbtt__cff_get_index(&b);
      }

      t = stbtt__find_table(stream, fontstart, "maxp");
      if (t)
      {
         stream->seek(t + 4);
         info->numGlyphs = read_uint16(stream);
      }
      else
      {
         info->numGlyphs = 0xffff;
      }
      info->svg = -1;

      // find a cmap encoding table we understand *now* to avoid searching
      // later. (todo: could make this installable)
      // the same regardless of glyph.
      stream->seek(cmap + 2);
      numTables = read_uint16(stream);
      info->index_map = 0;
      for (i = 0; i < numTables; ++i)
      {
         stbtt_uint32 encoding_record = cmap + 4 + 8 * i;
         // find an encoding we understand:
         stream->seek(encoding_record);
         switch (read_uint16(stream))
         {
         case STBTT_PLATFORM_ID_MICROSOFT:
            switch (read_uint16(stream))
            {
            case STBTT_MS_EID_UNICODE_BMP:
            case STBTT_MS_EID_UNICODE_FULL:
               // MS/Unicode
               info->index_map = cmap + read_uint32(stream);
               break;
            }
            break;
         case STBTT_PLATFORM_ID_UNICODE:
            // Mac/iOS has these
            // all the encodingIDs are unicode, so we don't bother to check it
            stream->seek(encoding_record + 4);
            info->index_map = cmap + read_uint32(stream);
            break;
         }
      }
      if (info->index_map == 0)
         return 0;
      stream->seek(info->head + 50);
      info->indexToLocFormat = read_uint16(stream);
      return 1;
   }

   STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info, int unicode_codepoint)
   {
      stbtt_uint32 index_map = info->index_map;
      info->stream->seek(index_map);
      stbtt_uint16 format = read_uint16(info->stream);
      if (format == 0)
      { // apple byte encoding
         stbtt_int32 bytes = read_uint16(info->stream);
         if (unicode_codepoint < bytes - 6)
         {
            info->stream->seek(index_map + 6 + unicode_codepoint);
            return read_uint8(info->stream);
         }
         return 0;
      }
      else if (format == 6)
      {
         info->stream->seek(index_map + 6);
         stbtt_uint32 first = read_uint16(info->stream);
         stbtt_uint32 count = read_uint16(info->stream);
         if ((stbtt_uint32)unicode_codepoint >= first && (stbtt_uint32)unicode_codepoint < first + count)
         {
            info->stream->seek(index_map + 10 + (unicode_codepoint - first) * 2);
            return read_uint16(info->stream);
         }
         return 0;
      }
      else if (format == 2)
      {
         STBTT_assert(0); // @TODO: high-byte mapping for japanese/chinese/korean
         return 0;
      }
      else if (format == 4)
      { // standard mapping for windows fonts: binary search collection of ranges
         info->stream->seek(index_map + 6);
         stbtt_uint16 segcount = read_uint16(info->stream) >> 1;
         stbtt_uint16 searchRange = read_uint16(info->stream) >> 1;
         stbtt_uint16 entrySelector = read_uint16(info->stream);
         stbtt_uint16 rangeShift = read_uint16(info->stream) >> 1;

         // do a binary search of the segments
         stbtt_uint32 endCount = index_map + 14;
         stbtt_uint32 search = endCount;

         if (unicode_codepoint > 0xffff)
            return 0;

         // they lie from endCount .. endCount + segCount
         // but searchRange is the nearest power of two, so...
         info->stream->seek(search + rangeShift * 2);
         if (unicode_codepoint >= read_uint16(info->stream))
            search += rangeShift * 2;

         // now decrement to bias correctly to find smallest
         search -= 2;
         while (entrySelector)
         {
            stbtt_uint16 end;
            searchRange >>= 1;
            info->stream->seek(search + searchRange * 2);
            end = read_uint16(info->stream);
            if (unicode_codepoint > end)
               search += searchRange * 2;
            --entrySelector;
         }
         search += 2;

         {
            stbtt_uint16 offset, start;
            stbtt_uint16 item = (stbtt_uint16)((search - endCount) >> 1);

            // STBTT_assert(unicode_codepoint <= ttUSHORT(data + endCount + 2*item));
            info->stream->seek(index_map + 14 + segcount * 2 + 2 + 2 * item);
            start = read_uint16(info->stream);
            if (unicode_codepoint < start)
               return 0;
            info->stream->seek(index_map + 14 + segcount * 6 + 2 + 2 * item);
            offset = read_uint16(info->stream);
            if (offset == 0)
            {
               info->stream->seek(index_map + 14 + segcount * 4 + 2 + 2 * item);
               return (stbtt_uint16)(unicode_codepoint + read_int16(info->stream));
            }
            info->stream->seek(offset + (unicode_codepoint - start) * 2 + index_map + 14 + segcount * 6 + 2 + 2 * item);
            return read_uint16(info->stream);
         }
      }
      else if (format == 12 || format == 13)
      {
         info->stream->seek(index_map + 12);
         stbtt_uint32 ngroups = read_uint32(info->stream);
         stbtt_int32 low, high;
         low = 0;
         high = (stbtt_int32)ngroups;
         // Binary search the right group.
         while (low < high)
         {
            stbtt_int32 mid = low + ((high - low) >> 1); // rounds down, so low <= mid < high
            info->stream->seek(index_map + 15 + mid * 12);
            stbtt_uint32 start_char = read_uint32(info->stream);
            stbtt_uint32 end_char = read_uint32(info->stream);
            if ((stbtt_uint32)unicode_codepoint < start_char)
               high = mid;
            else if ((stbtt_uint32)unicode_codepoint > end_char)
               low = mid + 1;
            else
            {
               stbtt_uint32 start_glyph = read_uint32(info->stream);
               if (format == 12)
                  return start_glyph + unicode_codepoint - start_char;
               else // format == 13
                  return start_glyph;
            }
         }
         return 0; // not found
      }
      // @TODO
      STBTT_assert(0);
      return 0;
   }

   /*STBTT_DEF int stbtt_GetCodepointShape(const stbtt_fontinfo *info, int unicode_codepoint, stbtt_vertex **vertices)
   {
      return stbtt_GetGlyphShape(info, stbtt_FindGlyphIndex(info, unicode_codepoint), vertices);
   }*/

   static void stbtt_setvertex(stbtt_vertex *v, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy)
   {
      v->type = type;
      v->x = (stbtt_int16)x;
      v->y = (stbtt_int16)y;
      v->cx = (stbtt_int16)cx;
      v->cy = (stbtt_int16)cy;
   }

   static int stbtt__GetGlyfOffset(const stbtt_fontinfo *info, int glyph_index)
   {
      int g1, g2;

      STBTT_assert(!info->cff.size);

      if (glyph_index >= info->numGlyphs)
         return -1; // glyph index out of range
      if (info->indexToLocFormat >= 2)
         return -1; // unknown index->glyph map format

      if (info->indexToLocFormat == 0)
      {
         info->stream->seek(info->loca + glyph_index * 2);
         g1 = info->glyf + read_uint16(info->stream) * 2;
         g2 = info->glyf + read_uint16(info->stream) * 2;
      }
      else
      {
         info->stream->seek(info->loca + glyph_index * 4);
         g1 = info->glyf + read_uint32(info->stream);
         g2 = info->glyf + read_uint32(info->stream);
      }

      return g1 == g2 ? -1 : g1; // if length is 0, return -1
   }

   static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

   STBTT_DEF int stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
   {
      if (info->cff.size)
      {
         stbtt__GetGlyphInfoT2(info, glyph_index, x0, y0, x1, y1);
      }
      else
      {
         int g = stbtt__GetGlyfOffset(info, glyph_index);
         if (g < 0)
            return 0;
         info->stream->seek(g + 2);
         int xx0 = read_int16(info->stream), yy0 = read_int16(info->stream), xx1 = read_int16(info->stream), yy1 = read_int16(info->stream);
         if (x0)
            *x0 = xx0;
         if (y0)
            *y0 = yy0;
         if (x1)
            *x1 = xx1;
         if (y1)
            *y1 = yy1;
      }
      return 1;
   }

   /*STBTT_DEF int stbtt_GetCodepointBox(const stbtt_fontinfo *info, int codepoint, int *x0, int *y0, int *x1, int *y1)
   {
      return stbtt_GetGlyphBox(info, stbtt_FindGlyphIndex(info, codepoint), x0, y0, x1, y1);
   }*/

   /*STBTT_DEF int stbtt_IsGlyphEmpty(const stbtt_fontinfo *info, int glyph_index)
   {
      stbtt_int16 numberOfContours;
      int g;
      if (info->cff.size)
         return stbtt__GetGlyphInfoT2(info, glyph_index, NULL, NULL, NULL, NULL) == 0;
      g = stbtt__GetGlyfOffset(info, glyph_index);
      if (g < 0)
         return 1;
      info->stream->seek(g);
      numberOfContours = read_int16(info->stream);
      return numberOfContours == 0;
   }*/

   static int stbtt__close_shape(stbtt_vertex *vertices, int num_vertices, int was_off, int start_off,
                                 stbtt_int32 sx, stbtt_int32 sy, stbtt_int32 scx, stbtt_int32 scy, stbtt_int32 cx, stbtt_int32 cy)
   {
      if (start_off)
      {
         if (was_off)
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx + scx) >> 1, (cy + scy) >> 1, cx, cy);
         stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx, sy, scx, scy);
      }
      else
      {
         if (was_off)
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx, sy, cx, cy);
         else
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, sx, sy, 0, 0);
      }
      return num_vertices;
   }

   static int stbtt__GetGlyphShapeTT(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
   {
      stbtt_int16 numberOfContours;
      stbtt_uint32 endPtsOfContours;
      stbtt_vertex *vertices = 0;
      int num_vertices = 0;
      int g = stbtt__GetGlyfOffset(info, glyph_index);

      *pvertices = NULL;

      if (g < 0)
         return 0;
      info->stream->seek(g);
      numberOfContours = read_int16(info->stream);

      if (numberOfContours > 0)
      {
         stbtt_uint8 flags = 0, flagcount;
         stbtt_int32 ins, i, j = 0, m, n, next_move, was_off = 0, off, start_off = 0;
         stbtt_int32 x, y, cx, cy, sx, sy, scx, scy;
         //stbtt_uint8 *points;
         stbtt_uint32 points_offs;
         endPtsOfContours = (g + 10);
         info->stream->seek(g + 10 + numberOfContours * 2);
         ins = read_uint16(info->stream);
         points_offs = g + 10 + numberOfContours * 2 + 2 + ins;
         //points = data + points_offs;
         info->stream->seek(endPtsOfContours + numberOfContours * 2 - 2);

         n = 1 + read_uint16(info->stream);

         m = n + 2 * numberOfContours; // a loose bound on how many vertices we might need
         vertices = (stbtt_vertex *)STBTT_malloc(m * sizeof(vertices[0]), info->userdata);
         if (vertices == 0)
            return 0;

         next_move = 0;
         flagcount = 0;

         // in first pass, we load uninterpreted data into the allocated array
         // above, shifted to the end of the array so we won't overwrite it when
         // we create our final data starting from the front

         off = m - n; // starting offset for uninterpreted data, regardless of how m ends up being calculated

         // first load flags
         info->stream->seek(points_offs);
         for (i = 0; i < n; ++i)
         {
            if (flagcount == 0)
            {
               flags = read_uint8(info->stream);
               //++points;
               ++points_offs;
               if (flags & 8)
               {
                  flagcount = read_uint8(info->stream);
                  //++points;
                  ++points_offs;
               }
            }
            else
               --flagcount;
            vertices[off + i].type = flags;
         }
         // now load x coordinates
         x = 0;
         for (i = 0; i < n; ++i)
         {
            flags = vertices[off + i].type;
            if (flags & 2)
            {
               // TODO: Find out why we have to seek here
               info->stream->seek(points_offs);
               stbtt_int16 dx = read_uint8(info->stream);
               ++points_offs;
               x += (flags & 16) ? dx : -dx; // ???
            }
            else
            {
               if (!(flags & 16))
               {
                  // TODO: Find out why we have to seek here
                  info->stream->seek(points_offs);
                  x = x + read_int16(info->stream);
                  points_offs += 2;
               }
            }
            vertices[off + i].x = (stbtt_int16)x;
         }

         // now load y coordinates
         y = 0;
         for (i = 0; i < n; ++i)
         {
            flags = vertices[off + i].type;
            if (flags & 4)
            {
               // TODO: Find out why we have to seek here
               info->stream->seek(points_offs);
               stbtt_int16 dy = read_uint8(info->stream);
               ++points_offs;
               y += (flags & 32) ? dy : -dy; // ???
            }
            else
            {
               if (!(flags & 32))
               {
                  // TODO: Find out why we have to seek here
                  info->stream->seek(points_offs);
                  y = y + read_int16(info->stream);
                  points_offs += 2;
               }
            }
            vertices[off + i].y = (stbtt_int16)y;
         }

         // now convert them to our format
         num_vertices = 0;
         sx = sy = cx = cy = scx = scy = 0;
         for (i = 0; i < n; ++i)
         {
            flags = vertices[off + i].type;
            x = (stbtt_int16)vertices[off + i].x;
            y = (stbtt_int16)vertices[off + i].y;

            if (next_move == i)
            {
               if (i != 0)
                  num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx, sy, scx, scy, cx, cy);

               // now start the new one
               start_off = !(flags & 1);
               if (start_off)
               {
                  // if we start off with an off-curve point, then when we need to find a point on the curve
                  // where we can start, and we need to save some state for when we wraparound.
                  scx = x;
                  scy = y;
                  if (!(vertices[off + i + 1].type & 1))
                  {
                     // next point is also a curve point, so interpolate an on-point curve
                     sx = (x + (stbtt_int32)vertices[off + i + 1].x) >> 1;
                     sy = (y + (stbtt_int32)vertices[off + i + 1].y) >> 1;
                  }
                  else
                  {
                     // otherwise just use the next point as our start point
                     sx = (stbtt_int32)vertices[off + i + 1].x;
                     sy = (stbtt_int32)vertices[off + i + 1].y;
                     ++i; // we're using point i+1 as the starting point, so skip it
                  }
               }
               else
               {
                  sx = x;
                  sy = y;
               }
               stbtt_setvertex(&vertices[num_vertices++], STBTT_vmove, sx, sy, 0, 0);
               was_off = 0;
               info->stream->seek(endPtsOfContours + j * 2);

               next_move = 1 + read_uint16(info->stream);
               ++j;
            }
            else
            {
               if (!(flags & 1))
               {               // if it's a curve
                  if (was_off) // two off-curve control points in a row means interpolate an on-curve midpoint
                     stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx + x) >> 1, (cy + y) >> 1, cx, cy);
                  cx = x;
                  cy = y;
                  was_off = 1;
               }
               else
               {
                  if (was_off)
                     stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x, y, cx, cy);
                  else
                     stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, x, y, 0, 0);
                  was_off = 0;
               }
            }
         }
         num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx, sy, scx, scy, cx, cy);
      }
      else if (numberOfContours < 0)
      {
         // Compound shapes.
         int more = 1;
         stbtt_uint32 comp = g + 10;
         num_vertices = 0;
         vertices = 0;
         while (more)
         {
            stbtt_uint16 flags, gidx;
            int comp_num_verts = 0, i;
            stbtt_vertex *comp_verts = 0, *tmp = 0;
            float mtx[6] = {1, 0, 0, 1, 0, 0}, m, n;
            info->stream->seek(comp);
            flags = read_int16(info->stream);
            comp += 2;
            gidx = read_int16(info->stream);
            comp += 2;

            if (flags & 2)
            { // XY values
               info->stream->seek(comp);
               if (flags & 1)
               { // shorts
                  mtx[4] = read_int16(info->stream);
                  comp += 2;
                  mtx[5] = read_int16(info->stream);
                  comp += 2;
               }
               else
               {
                  mtx[4] = read_int8(info->stream);
                  comp += 1;
                  mtx[5] = read_int8(info->stream);
                  comp += 1;
               }
            }
            else
            {
               // @TODO handle matching point
               STBTT_assert(0);
            }
            info->stream->seek(comp);
            if (flags & (1 << 3))
            { // WE_HAVE_A_SCALE
               mtx[0] = mtx[3] = read_int16(info->stream) / 16384.0f;
               comp += 2;
               mtx[1] = mtx[2] = 0;
            }
            else if (flags & (1 << 6))
            { // WE_HAVE_AN_X_AND_YSCALE
               mtx[0] = read_int16(info->stream) / 16384.0f;
               comp += 2;
               mtx[1] = mtx[2] = 0;
               mtx[3] = read_int16(info->stream) / 16384.0f;
               comp += 2;
            }
            else if (flags & (1 << 7))
            { // WE_HAVE_A_TWO_BY_TWO
               mtx[0] = read_int16(info->stream) / 16384.0f;
               comp += 2;
               mtx[1] = read_int16(info->stream) / 16384.0f;
               comp += 2;
               mtx[2] = read_int16(info->stream) / 16384.0f;
               comp += 2;
               mtx[3] = read_int16(info->stream) / 16384.0f;
               comp += 2;
            }

            // Find transformation scales.
            m = (float)STBTT_sqrt(mtx[0] * mtx[0] + mtx[1] * mtx[1]);
            n = (float)STBTT_sqrt(mtx[2] * mtx[2] + mtx[3] * mtx[3]);

            // Get indexed glyph.
            comp_num_verts = stbtt_GetGlyphShape(info, gidx, &comp_verts);
            if (comp_num_verts > 0)
            {
               // Transform vertices.
               for (i = 0; i < comp_num_verts; ++i)
               {
                  stbtt_vertex *v = &comp_verts[i];
                  stbtt_vertex_type x, y;
                  x = v->x;
                  y = v->y;
                  v->x = (stbtt_vertex_type)(m * (mtx[0] * x + mtx[2] * y + mtx[4]));
                  v->y = (stbtt_vertex_type)(n * (mtx[1] * x + mtx[3] * y + mtx[5]));
                  x = v->cx;
                  y = v->cy;
                  v->cx = (stbtt_vertex_type)(m * (mtx[0] * x + mtx[2] * y + mtx[4]));
                  v->cy = (stbtt_vertex_type)(n * (mtx[1] * x + mtx[3] * y + mtx[5]));
               }
               // Append vertices.
               tmp = (stbtt_vertex *)STBTT_malloc((num_vertices + comp_num_verts) * sizeof(stbtt_vertex), info->userdata);
               if (!tmp)
               {
                  if (vertices)
                     STBTT_free(vertices, info->userdata);
                  if (comp_verts)
                     STBTT_free(comp_verts, info->userdata);
                  return 0;
               }
               if (num_vertices > 0)
                  STBTT_memcpy(tmp, vertices, num_vertices * sizeof(stbtt_vertex));
               STBTT_memcpy(tmp + num_vertices, comp_verts, comp_num_verts * sizeof(stbtt_vertex));
               if (vertices)
                  STBTT_free(vertices, info->userdata);
               vertices = tmp;
               STBTT_free(comp_verts, info->userdata);
               num_vertices += comp_num_verts;
            }
            // More components ?
            more = flags & (1 << 5);
         }
      }
      else
      {
         // numberOfCounters == 0, do nothing
      }

      *pvertices = vertices;
      return num_vertices;
   }

   typedef struct
   {
      int bounds;
      int started;
      float first_x, first_y;
      float x, y;
      stbtt_int32 min_x, max_x, min_y, max_y;

      stbtt_vertex *pvertices;
      int num_vertices;
   } stbtt__csctx;

#define STBTT__CSCTX_INIT(bounds)                \
   {                                             \
      bounds, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0 \
   }

   static void stbtt__track_vertex(stbtt__csctx *c, stbtt_int32 x, stbtt_int32 y)
   {
      if (x > c->max_x || !c->started)
         c->max_x = x;
      if (y > c->max_y || !c->started)
         c->max_y = y;
      if (x < c->min_x || !c->started)
         c->min_x = x;
      if (y < c->min_y || !c->started)
         c->min_y = y;
      c->started = 1;
   }

   static void stbtt__csctx_v(stbtt__csctx *c, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy, stbtt_int32 cx1, stbtt_int32 cy1)
   {
      if (c->bounds)
      {
         stbtt__track_vertex(c, x, y);
         if (type == STBTT_vcubic)
         {
            stbtt__track_vertex(c, cx, cy);
            stbtt__track_vertex(c, cx1, cy1);
         }
      }
      else
      {
         stbtt_setvertex(&c->pvertices[c->num_vertices], type, x, y, cx, cy);
         c->pvertices[c->num_vertices].cx1 = (stbtt_int16)cx1;
         c->pvertices[c->num_vertices].cy1 = (stbtt_int16)cy1;
      }
      c->num_vertices++;
   }

   static void stbtt__csctx_close_shape(stbtt__csctx *ctx)
   {
      if (ctx->first_x != ctx->x || ctx->first_y != ctx->y)
         stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->first_x, (int)ctx->first_y, 0, 0, 0, 0);
   }

   static void stbtt__csctx_rmove_to(stbtt__csctx *ctx, float dx, float dy)
   {
      stbtt__csctx_close_shape(ctx);
      ctx->first_x = ctx->x = ctx->x + dx;
      ctx->first_y = ctx->y = ctx->y + dy;
      stbtt__csctx_v(ctx, STBTT_vmove, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
   }

   static void stbtt__csctx_rline_to(stbtt__csctx *ctx, float dx, float dy)
   {
      ctx->x += dx;
      ctx->y += dy;
      stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
   }

   static void stbtt__csctx_rccurve_to(stbtt__csctx *ctx, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
   {
      float cx1 = ctx->x + dx1;
      float cy1 = ctx->y + dy1;
      float cx2 = cx1 + dx2;
      float cy2 = cy1 + dy2;
      ctx->x = cx2 + dx3;
      ctx->y = cy2 + dy3;
      stbtt__csctx_v(ctx, STBTT_vcubic, (int)ctx->x, (int)ctx->y, (int)cx1, (int)cy1, (int)cx2, (int)cy2);
   }

   static stbtt::stbtt__buf stbtt__get_subr(stbtt::stbtt__buf idx, int n)
   {
      int count = stbtt__cff_index_count(&idx);
      int bias = 107;
      if (count >= 33900)
         bias = 32768;
      else if (count >= 1240)
         bias = 1131;
      n += bias;
      if (n < 0 || n >= count)
         return stbtt__new_buf(NULL, 0, 0);
      return stbtt__cff_index_get(idx, n);
   }

   static stbtt::stbtt__buf stbtt__cid_get_glyph_subrs(const stbtt_fontinfo *info, int glyph_index)
   {
      stbtt::stbtt__buf fdselect = info->fdselect;
      int nranges, start, end, v, fmt, fdselector = -1, i;

      stbtt__buf_seek(&fdselect, 0);
      fmt = stbtt__buf_get8(&fdselect);
      if (fmt == 0)
      {
         // untested
         stbtt__buf_skip(&fdselect, glyph_index);
         fdselector = stbtt__buf_get8(&fdselect);
      }
      else if (fmt == 3)
      {
         nranges = stbtt__buf_get16(&fdselect);
         start = stbtt__buf_get16(&fdselect);
         for (i = 0; i < nranges; i++)
         {
            v = stbtt__buf_get8(&fdselect);
            end = stbtt__buf_get16(&fdselect);
            if (glyph_index >= start && glyph_index < end)
            {
               fdselector = v;
               break;
            }
            start = end;
         }
      }
      if (fdselector == -1)
         stbtt__new_buf(NULL, 0, 0);
      return stbtt__get_subrs(info->cff, stbtt__cff_index_get(info->fontdicts, fdselector));
   }

   static int stbtt__run_charstring(const stbtt_fontinfo *info, int glyph_index, stbtt__csctx *c)
   {
      int in_header = 1, maskbits = 0, subr_stack_height = 0, sp = 0, v, i, b0;
      int has_subrs = 0, clear_stack;
      float s[48];
      stbtt::stbtt__buf subr_stack[10], subrs = info->subrs, b;
      float f;

#define STBTT__CSERR(s) (0)

      // this currently ignores the initial width value, which isn't needed if we have hmtx
      b = stbtt__cff_index_get(info->charstrings, glyph_index);
      while (b.cursor < b.size)
      {
         i = 0;
         clear_stack = 1;
         b0 = stbtt__buf_get8(&b);
         switch (b0)
         {
         // @TODO implement hinting
         case 0x13: // hintmask
         case 0x14: // cntrmask
            if (in_header)
               maskbits += (sp / 2); // implicit "vstem"
            in_header = 0;
            stbtt__buf_skip(&b, (maskbits + 7) / 8);
            break;

         case 0x01: // hstem
         case 0x03: // vstem
         case 0x12: // hstemhm
         case 0x17: // vstemhm
            maskbits += (sp / 2);
            break;

         case 0x15: // rmoveto
            in_header = 0;
            if (sp < 2)
               return STBTT__CSERR("rmoveto stack");
            stbtt__csctx_rmove_to(c, s[sp - 2], s[sp - 1]);
            break;
         case 0x04: // vmoveto
            in_header = 0;
            if (sp < 1)
               return STBTT__CSERR("vmoveto stack");
            stbtt__csctx_rmove_to(c, 0, s[sp - 1]);
            break;
         case 0x16: // hmoveto
            in_header = 0;
            if (sp < 1)
               return STBTT__CSERR("hmoveto stack");
            stbtt__csctx_rmove_to(c, s[sp - 1], 0);
            break;

         case 0x05: // rlineto
            if (sp < 2)
               return STBTT__CSERR("rlineto stack");
            for (; i + 1 < sp; i += 2)
               stbtt__csctx_rline_to(c, s[i], s[i + 1]);
            break;

            // hlineto/vlineto and vhcurveto/hvcurveto alternate horizontal and vertical
            // starting from a different place.

         case 0x07: // vlineto
            if (sp < 1)
               return STBTT__CSERR("vlineto stack");
            goto vlineto;
         case 0x06: // hlineto
            if (sp < 1)
               return STBTT__CSERR("hlineto stack");
            for (;;)
            {
               if (i >= sp)
                  break;
               stbtt__csctx_rline_to(c, s[i], 0);
               i++;
            vlineto:
               if (i >= sp)
                  break;
               stbtt__csctx_rline_to(c, 0, s[i]);
               i++;
            }
            break;

         case 0x1F: // hvcurveto
            if (sp < 4)
               return STBTT__CSERR("hvcurveto stack");
            goto hvcurveto;
         case 0x1E: // vhcurveto
            if (sp < 4)
               return STBTT__CSERR("vhcurveto stack");
            for (;;)
            {
               if (i + 3 >= sp)
                  break;
               stbtt__csctx_rccurve_to(c, 0, s[i], s[i + 1], s[i + 2], s[i + 3], (sp - i == 5) ? s[i + 4] : 0.0f);
               i += 4;
            hvcurveto:
               if (i + 3 >= sp)
                  break;
               stbtt__csctx_rccurve_to(c, s[i], 0, s[i + 1], s[i + 2], (sp - i == 5) ? s[i + 4] : 0.0f, s[i + 3]);
               i += 4;
            }
            break;

         case 0x08: // rrcurveto
            if (sp < 6)
               return STBTT__CSERR("rcurveline stack");
            for (; i + 5 < sp; i += 6)
               stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            break;

         case 0x18: // rcurveline
            if (sp < 8)
               return STBTT__CSERR("rcurveline stack");
            for (; i + 5 < sp - 2; i += 6)
               stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            if (i + 1 >= sp)
               return STBTT__CSERR("rcurveline stack");
            stbtt__csctx_rline_to(c, s[i], s[i + 1]);
            break;

         case 0x19: // rlinecurve
            if (sp < 8)
               return STBTT__CSERR("rlinecurve stack");
            for (; i + 1 < sp - 6; i += 2)
               stbtt__csctx_rline_to(c, s[i], s[i + 1]);
            if (i + 5 >= sp)
               return STBTT__CSERR("rlinecurve stack");
            stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            break;

         case 0x1A: // vvcurveto
         case 0x1B: // hhcurveto
            if (sp < 4)
               return STBTT__CSERR("(vv|hh)curveto stack");
            f = 0.0;
            if (sp & 1)
            {
               f = s[i];
               i++;
            }
            for (; i + 3 < sp; i += 4)
            {
               if (b0 == 0x1B)
                  stbtt__csctx_rccurve_to(c, s[i], f, s[i + 1], s[i + 2], s[i + 3], 0.0);
               else
                  stbtt__csctx_rccurve_to(c, f, s[i], s[i + 1], s[i + 2], 0.0, s[i + 3]);
               f = 0.0;
            }
            break;

         case 0x0A: // callsubr
            if (!has_subrs)
            {
               if (info->fdselect.size)
                  subrs = stbtt__cid_get_glyph_subrs(info, glyph_index);
               has_subrs = 1;
            }
            // fallthrough
         case 0x1D: // callgsubr
            if (sp < 1)
               return STBTT__CSERR("call(g|)subr stack");
            v = (int)s[--sp];
            if (subr_stack_height >= 10)
               return STBTT__CSERR("recursion limit");
            subr_stack[subr_stack_height++] = b;
            b = stbtt__get_subr(b0 == 0x0A ? subrs : info->gsubrs, v);
            if (b.size == 0)
               return STBTT__CSERR("subr not found");
            b.cursor = 0;
            clear_stack = 0;
            break;

         case 0x0B: // return
            if (subr_stack_height <= 0)
               return STBTT__CSERR("return outside subr");
            b = subr_stack[--subr_stack_height];
            clear_stack = 0;
            break;

         case 0x0E: // endchar
            stbtt__csctx_close_shape(c);
            return 1;

         case 0x0C:
         { // two-byte escape
            float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
            float dx, dy;
            int b1 = stbtt__buf_get8(&b);
            switch (b1)
            {
            // @TODO These "flex" implementations ignore the flex-depth and resolution,
            // and always draw beziers.
            case 0x22: // hflex
               if (sp < 7)
                  return STBTT__CSERR("hflex stack");
               dx1 = s[0];
               dx2 = s[1];
               dy2 = s[2];
               dx3 = s[3];
               dx4 = s[4];
               dx5 = s[5];
               dx6 = s[6];
               stbtt__csctx_rccurve_to(c, dx1, 0, dx2, dy2, dx3, 0);
               stbtt__csctx_rccurve_to(c, dx4, 0, dx5, -dy2, dx6, 0);
               break;

            case 0x23: // flex
               if (sp < 13)
                  return STBTT__CSERR("flex stack");
               dx1 = s[0];
               dy1 = s[1];
               dx2 = s[2];
               dy2 = s[3];
               dx3 = s[4];
               dy3 = s[5];
               dx4 = s[6];
               dy4 = s[7];
               dx5 = s[8];
               dy5 = s[9];
               dx6 = s[10];
               dy6 = s[11];
               //fd is s[12]
               stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
               stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
               break;

            case 0x24: // hflex1
               if (sp < 9)
                  return STBTT__CSERR("hflex1 stack");
               dx1 = s[0];
               dy1 = s[1];
               dx2 = s[2];
               dy2 = s[3];
               dx3 = s[4];
               dx4 = s[5];
               dx5 = s[6];
               dy5 = s[7];
               dx6 = s[8];
               stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, 0);
               stbtt__csctx_rccurve_to(c, dx4, 0, dx5, dy5, dx6, -(dy1 + dy2 + dy5));
               break;

            case 0x25: // flex1
               if (sp < 11)
                  return STBTT__CSERR("flex1 stack");
               dx1 = s[0];
               dy1 = s[1];
               dx2 = s[2];
               dy2 = s[3];
               dx3 = s[4];
               dy3 = s[5];
               dx4 = s[6];
               dy4 = s[7];
               dx5 = s[8];
               dy5 = s[9];
               dx6 = dy6 = s[10];
               dx = dx1 + dx2 + dx3 + dx4 + dx5;
               dy = dy1 + dy2 + dy3 + dy4 + dy5;
               if (STBTT_fabs(dx) > STBTT_fabs(dy))
                  dy6 = -dy;
               else
                  dx6 = -dx;
               stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
               stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
               break;

            default:
               return STBTT__CSERR("unimplemented");
            }
         }
         break;

         default:
            if (b0 != 255 && b0 != 28 && (b0 < 32 || b0 > 254))
               return STBTT__CSERR("reserved operator");

            // push immediate
            if (b0 == 255)
            {
               f = (float)(stbtt_int32)stbtt__buf_get32(&b) / 0x10000;
            }
            else
            {
               stbtt__buf_skip(&b, -1);
               f = (float)(stbtt_int16)stbtt__cff_int(&b);
            }
            if (sp >= 48)
               return STBTT__CSERR("push stack overflow");
            s[sp++] = f;
            clear_stack = 0;
            break;
         }
         if (clear_stack)
            sp = 0;
      }
      return STBTT__CSERR("no endchar");

#undef STBTT__CSERR
   }

   static int stbtt__GetGlyphShapeT2(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
   {
      // runs the charstring twice, once to count and once to output (to avoid realloc)
      stbtt__csctx count_ctx = STBTT__CSCTX_INIT(1);
      stbtt__csctx output_ctx = STBTT__CSCTX_INIT(0);
      if (stbtt__run_charstring(info, glyph_index, &count_ctx))
      {
         *pvertices = (stbtt_vertex *)STBTT_malloc(count_ctx.num_vertices * sizeof(stbtt_vertex), info->userdata);
         output_ctx.pvertices = *pvertices;
         if (stbtt__run_charstring(info, glyph_index, &output_ctx))
         {
            STBTT_assert(output_ctx.num_vertices == count_ctx.num_vertices);
            return output_ctx.num_vertices;
         }
      }
      *pvertices = NULL;
      return 0;
   }

   static int stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
   {
      stbtt__csctx c = STBTT__CSCTX_INIT(1);
      int r = stbtt__run_charstring(info, glyph_index, &c);
      if (x0)
         *x0 = r ? c.min_x : 0;
      if (y0)
         *y0 = r ? c.min_y : 0;
      if (x1)
         *x1 = r ? c.max_x : 0;
      if (y1)
         *y1 = r ? c.max_y : 0;
      return r ? c.num_vertices : 0;
   }

   STBTT_DEF int stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
   {
      if (!info->cff.size)
         return stbtt__GetGlyphShapeTT(info, glyph_index, pvertices);
      else
         return stbtt__GetGlyphShapeT2(info, glyph_index, pvertices);
   }

   STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing)
   {
      info->stream->seek(info->hhea + 34);
      stbtt_uint16 numOfLongHorMetrics = read_uint16(info->stream);
      if (glyph_index < numOfLongHorMetrics)
      {
         if (advanceWidth)
         {
            info->stream->seek(info->hmtx + 4 * glyph_index);
            *advanceWidth = read_int16(info->stream);
         }
         if (leftSideBearing)
         {
            info->stream->seek(info->hmtx + 4 * glyph_index + 2);
            *leftSideBearing = read_int16(info->stream);
         }
      }
      else
      {
         if (advanceWidth)
         {
            info->stream->seek(info->hmtx + 4 * (numOfLongHorMetrics - 1));
            *advanceWidth = read_int16(info->stream);
         }
         if (leftSideBearing)
         {
            info->stream->seek(info->hmtx + 4 * numOfLongHorMetrics + 2 * (glyph_index - numOfLongHorMetrics));
            *leftSideBearing = read_int16(info->stream);
         }
      }
   }

   /*STBTT_DEF int stbtt_GetKerningTableLength(const stbtt_fontinfo *info)
   {
      // we only look at the first table. it must be 'horizontal' and format 0.
      if (!info->kern)
         return 0;
      info->stream->seek(info->kern + 2);

      if (read_uint16(info->stream) < 1) // number of tables, need at least 1
         return 0;
      info->stream->seek(info->kern + 8);
      if (read_uint16(info->stream) != 1) // horizontal flag must be set in format
         return 0;

      return read_uint16(info->stream);
   }*/

   /*STBTT_DEF int stbtt_GetKerningTable(const stbtt_fontinfo *info, stbtt_kerningentry *table, int table_length)
   {
      int k, length;

      // we only look at the first table. it must be 'horizontal' and format 0.
      if (!info->kern)
         return 0;
      info->stream->seek(info->kern + 2);
      if (read_uint16(info->stream) < 1) // number of tables, need at least 1
         return 0;
      info->stream->seek(info->kern + 8);
      if (read_uint16(info->stream) != 1) // horizontal flag must be set in format
         return 0;
      length = read_uint16(info->stream);
      if (table_length < length)
         length = table_length;

      for (k = 0; k < length; k++)
      {
         info->stream->seek(18 + (k * 6));
         table[k].glyph1 = read_uint16(info->stream);
         table[k].glyph2 = read_uint16(info->stream);
         table[k].advance = read_uint16(info->stream);
      }

      return length;
   }*/

   static int stbtt__GetGlyphKernInfoAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2)
   {
      stbtt_uint32 needle, straw;
      int l, r, m;

      // we only look at the first table. it must be 'horizontal' and format 0.
      if (!info->kern)
         return 0;
      info->stream->seek(info->kern + 2);
      if (read_uint16(info->stream) < 1) // number of tables, need at least 1
         return 0;
      info->stream->seek(info->kern + 8);
      if (read_uint16(info->stream) != 1) // horizontal flag must be set in format
         return 0;
      l = 0;
      r = read_uint16(info->stream) - 1;
      needle = glyph1 << 16 | glyph2;
      while (l <= r)
      {
         m = (l + r) >> 1;
         info->stream->seek(info->kern + 18 + (m * 6));
         straw = read_uint32(info->stream);
         if (needle < straw)
            r = m - 1;
         else if (needle > straw)
            l = m + 1;
         else
         {
            info->stream->seek(info->kern + 22 + (m * 6));
            return read_int16(info->stream);
         }
      }
      return 0;
   }

   /*static stbtt_int32 stbtt__GetCoverageIndex(stbtt_uint8 *coverageTable, int glyph)
   {
      stbtt_uint16 coverageFormat = ttUSHORT(coverageTable);
      switch (coverageFormat)
      {
      case 1:
      {
         stbtt_uint16 glyphCount = ttUSHORT(coverageTable + 2);

         // Binary search.
         stbtt_int32 l = 0, r = glyphCount - 1, m;
         int straw, needle = glyph;
         while (l <= r)
         {
            stbtt_uint8 *glyphArray = coverageTable + 4;
            stbtt_uint16 glyphID;
            m = (l + r) >> 1;
            glyphID = ttUSHORT(glyphArray + 2 * m);
            straw = glyphID;
            if (needle < straw)
               r = m - 1;
            else if (needle > straw)
               l = m + 1;
            else
            {
               return m;
            }
         }
      }
      break;

      case 2:
      {
         stbtt_uint16 rangeCount = ttUSHORT(coverageTable + 2);
         stbtt_uint8 *rangeArray = coverageTable + 4;

         // Binary search.
         stbtt_int32 l = 0, r = rangeCount - 1, m;
         int strawStart, strawEnd, needle = glyph;
         while (l <= r)
         {
            stbtt_uint8 *rangeRecord;
            m = (l + r) >> 1;
            rangeRecord = rangeArray + 6 * m;
            strawStart = ttUSHORT(rangeRecord);
            strawEnd = ttUSHORT(rangeRecord + 2);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else
            {
               stbtt_uint16 startCoverageIndex = ttUSHORT(rangeRecord + 4);
               return startCoverageIndex + glyph - strawStart;
            }
         }
      }
      break;

      default:
      {
         // There are no other cases.
         STBTT_assert(0);
      }
      break;
      }

      return -1;
   }*/
   static stbtt_int32 stbtt__GetCoverageIndex(const stbtt_fontinfo *info, stbtt_uint32 coverageTable, int glyph)
   {
      info->stream->seek(coverageTable);
      stbtt_uint16 coverageFormat = read_uint16(info->stream);
      switch (coverageFormat)
      {
      case 1:
      {
         info->stream->seek(coverageTable+2);
         stbtt_uint16 glyphCount = read_uint16(info->stream);

         // Binary search.
         stbtt_int32 l = 0, r = glyphCount - 1, m;
         int straw, needle = glyph;
         while (l <= r)
         {
            stbtt_uint32 glyphArray = coverageTable + 4;
            stbtt_uint16 glyphID;
            m = (l + r) >> 1;
            info->stream->seek(glyphArray + 2 * m);
            glyphID = read_uint16(info->stream);
            straw = glyphID;
            if (needle < straw)
               r = m - 1;
            else if (needle > straw)
               l = m + 1;
            else
            {
               return m;
            }
         }
      }
      break;

      case 2:
      {
         info->stream->seek(coverageTable+2);
         stbtt_uint16 rangeCount = read_uint16(info->stream);
         stbtt_uint32 rangeArray = coverageTable + 4;

         // Binary search.
         stbtt_int32 l = 0, r = rangeCount - 1, m;
         int strawStart, strawEnd, needle = glyph;
         stbtt_uint16 startCoverageIndex;
         while (l <= r)
         {
            stbtt_uint32 rangeRecord;
            m = (l + r) >> 1;
            rangeRecord = rangeArray + 6 * m;
            info->stream->seek(rangeRecord);
            strawStart = read_uint16(info->stream);
            strawEnd = read_uint16(info->stream);
            //long pos = info->stream->seek(0,io::seek_origin::current);
            startCoverageIndex = read_uint16(info->stream);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else
            {
               //info->stream->seek(rangeRecord+6); // ???
               //stbtt_uint16 startCoverageIndex = read_uint16(info->stream);
               return startCoverageIndex + glyph - strawStart;
            }
         }
      }
      break;

      default:
      {
         // There are no other cases.
         STBTT_assert(0);
      }
      break;
      }

      return -1;
   }
   /*
static stbtt_int32 stbtt__GetGlyphClass(stbtt_uint8 *classDefTable, int glyph)
{
   stbtt_uint16 classDefFormat = ttUSHORT(classDefTable);
   switch (classDefFormat)
   {
   case 1:
   {
      stbtt_uint16 startGlyphID = ttUSHORT(classDefTable + 2);
      stbtt_uint16 glyphCount = ttUSHORT(classDefTable + 4);
      stbtt_uint8 *classDef1ValueArray = classDefTable + 6;

      if (glyph >= startGlyphID && glyph < startGlyphID + glyphCount)
         return (stbtt_int32)ttUSHORT(classDef1ValueArray + 2 * (glyph - startGlyphID));

      classDefTable = classDef1ValueArray + 2 * glyphCount;
   }
   break;

   case 2:
   {
      stbtt_uint16 classRangeCount = ttUSHORT(classDefTable + 2);
      stbtt_uint8 *classRangeRecords = classDefTable + 4;

      // Binary search.
      stbtt_int32 l = 0, r = classRangeCount - 1, m;
      int strawStart, strawEnd, needle = glyph;
      while (l <= r)
      {
         stbtt_uint8 *classRangeRecord;
         m = (l + r) >> 1;
         classRangeRecord = classRangeRecords + 6 * m;
         strawStart = ttUSHORT(classRangeRecord);
         strawEnd = ttUSHORT(classRangeRecord + 2);
         if (needle < strawStart)
            r = m - 1;
         else if (needle > strawEnd)
            l = m + 1;
         else
            return (stbtt_int32)ttUSHORT(classRangeRecord + 4);
      }

      classDefTable = classRangeRecords + 6 * classRangeCount;
   }
   break;

   default:
   {
      // There are no other cases.
      STBTT_assert(0);
   }
   break;
   }

   return -1;
}
*/

   static stbtt_int32 stbtt__GetGlyphClass(const stbtt_fontinfo *info, stbtt_uint32 classDefTable, int glyph)
   {
      info->stream->seek(classDefTable);
      stbtt_uint16 classDefFormat = read_uint16(info->stream);
      switch (classDefFormat)
      {
      case 1:
      {
         stbtt_uint16 startGlyphID = read_uint16(info->stream);
         stbtt_uint16 glyphCount = read_uint16(info->stream);
         stbtt_uint32 classDef1ValueArray = classDefTable + 6;

         if (glyph >= startGlyphID && glyph < startGlyphID + glyphCount)
         {
            info->stream->seek(classDef1ValueArray + 2 * (glyph - startGlyphID));
            return (stbtt_int32)read_uint16(info->stream);
         }
         classDefTable = classDef1ValueArray + 2 * glyphCount;
      }
      break;

      case 2:
      {
         stbtt_uint16 classRangeCount = read_uint16(info->stream);
         stbtt_uint32 classRangeRecords = classDefTable + 4;

         // Binary search.
         stbtt_int32 l = 0, r = classRangeCount - 1, m;
         int strawStart, strawEnd, needle = glyph;
         while (l <= r)
         {
            stbtt_uint32 classRangeRecord;
            m = (l + r) >> 1;
            classRangeRecord = classRangeRecords + 6 * m;
            info->stream->seek(classRangeRecord);
            strawStart = read_uint16(info->stream);
            strawEnd = read_uint16(info->stream);
            if (needle < strawStart)
               r = m - 1;
            else if (needle > strawEnd)
               l = m + 1;
            else
            {
               return (stbtt_int32)read_uint16(info->stream);
            }
         }

         classDefTable = classRangeRecords + 6 * classRangeCount;
      }
      break;

      default:
      {
         // There are no other cases.
         STBTT_assert(0);
      }
      break;
      }

      return -1;
   }

// Define to STBTT_assert(x) if you want to break on unimplemented formats.
#define STBTT_GPOS_TODO_assert(x)
   static stbtt_int32 stbtt__GetGlyphGPOSInfoAdvance(const stbtt_fontinfo *info, int glyph1, int glyph2)
   {
      stbtt_uint16 lookupListOffset;
      stbtt_uint32 lookupList;
      stbtt_uint16 lookupCount;
      stbtt_uint32 data;
      stbtt_int32 i;

      if (!info->gpos)
         return 0;

      data = info->gpos;
      info->stream->seek(data);
      if (read_uint16(info->stream) != 1)
         return 0; // Major version 1
      if (read_uint16(info->stream) != 0)
         return 0; // Minor version 0
      info->stream->seek(data+8);
      lookupListOffset = read_uint16(info->stream);
      lookupList = data + lookupListOffset;
      info->stream->seek(lookupList);
      lookupCount = read_uint16(info->stream);

      for (i = 0; i < lookupCount; ++i)
      {
         info->stream->seek(lookupList+2+2*i);
         stbtt_uint16 lookupOffset = read_uint16(info->stream);
         stbtt_uint32 lookupTable = lookupList + lookupOffset;
         info->stream->seek(lookupTable);
         stbtt_uint16 lookupType = read_uint16(info->stream);
         info->stream->seek(lookupTable+4);
         stbtt_uint16 subTableCount = read_uint16(info->stream);
         stbtt_uint32 subTableOffsets = lookupTable + 6;
         switch (lookupType)
         {
         case 2:
         { // Pair Adjustment Positioning Subtable
            stbtt_int32 sti;
            for (sti = 0; sti < subTableCount; sti++)
            {
               info->stream->seek(subTableOffsets+2*sti);
               stbtt_uint16 subtableOffset = read_uint16(info->stream);
               stbtt_uint32 table = lookupTable + subtableOffset;
               info->stream->seek(table);
               stbtt_uint16 posFormat = read_uint16(info->stream);
               stbtt_uint16 coverageOffset = read_uint16(info->stream);
               stbtt_int32 coverageIndex = stbtt__GetCoverageIndex(info, table + coverageOffset, glyph1);
               if (coverageIndex == -1)
                  continue;

               switch (posFormat)
               {
               case 1:
               {
                  stbtt_int32 l, r, m;
                  int straw, needle;
                  info->stream->seek(table+4);
                  stbtt_uint16 valueFormat1 = read_uint16(info->stream);
                  stbtt_uint16 valueFormat2 = read_uint16(info->stream);
                  stbtt_int32 valueRecordPairSizeInBytes = 2;
                  stbtt_uint16 pairSetCount = read_uint16(info->stream);
                  info->stream->seek(table+10+2*coverageIndex);
                  stbtt_uint16 pairPosOffset = read_uint16(info->stream);
                  stbtt_uint32 pairValueTable = table + pairPosOffset;
                  info->stream->seek(pairValueTable);
                  stbtt_uint16 pairValueCount = read_uint16(info->stream);
                  stbtt_uint32 pairValueArray = pairValueTable + 2;
                  // TODO: Support more formats.
                  STBTT_GPOS_TODO_assert(valueFormat1 == 4);
                  if (valueFormat1 != 4)
                     return 0;
                  STBTT_GPOS_TODO_assert(valueFormat2 == 0);
                  if (valueFormat2 != 0)
                     return 0;

                  STBTT_assert(coverageIndex < pairSetCount);
                  STBTT__NOTUSED(pairSetCount);

                  needle = glyph2;
                  r = pairValueCount - 1;
                  l = 0;

                  // Binary search.
                  while (l <= r)
                  {
                     stbtt_uint16 secondGlyph;
                     stbtt_uint32 pairValue;
                     m = (l + r) >> 1;
                     pairValue = pairValueArray + (2 + valueRecordPairSizeInBytes) * m;
                     info->stream->seek(pairValue);
                     secondGlyph = read_uint16(info->stream);
                     straw = secondGlyph;
                     if (needle < straw)
                        r = m - 1;
                     else if (needle > straw)
                        l = m + 1;
                     else
                     {
                        stbtt_int16 xAdvance = read_int16(info->stream);
                        return xAdvance;
                     }
                  }
               }
               break;

               case 2:
               {
                  info->stream->seek(table+4);
                  stbtt_uint16 valueFormat1 = read_uint16(info->stream);
                  stbtt_uint16 valueFormat2 = read_uint16(info->stream);
                  
                  stbtt_uint16 classDef1Offset = read_uint16(info->stream);
                  stbtt_uint16 classDef2Offset = read_uint16(info->stream);
                  int glyph1class = stbtt__GetGlyphClass(info,table + classDef1Offset, glyph1);
                  int glyph2class = stbtt__GetGlyphClass(info,table + classDef2Offset, glyph2);
                  info->stream->seek(table+12);
                  stbtt_uint16 class1Count = read_uint16(info->stream);
                  stbtt_uint16 class2Count = read_uint16(info->stream);
                  STBTT_assert(glyph1class < class1Count);
                  STBTT_assert(glyph2class < class2Count);

                  // TODO: Support more formats.
                  STBTT_GPOS_TODO_assert(valueFormat1 == 4);
                  if (valueFormat1 != 4)
                     return 0;
                  STBTT_GPOS_TODO_assert(valueFormat2 == 0);
                  if (valueFormat2 != 0)
                     return 0;

                  if (glyph1class >= 0 && glyph1class < class1Count && glyph2class >= 0 && glyph2class < class2Count)
                  {
                     stbtt_uint32 class1Records = table + 16;
                     stbtt_uint32 class2Records = class1Records + 2 * (glyph1class * class2Count);
                     info->stream->seek(class2Records+2*glyph2class);
                     stbtt_int16 xAdvance = read_int16(info->stream);
                     return xAdvance;
                  }
               }
               break;

               default:
               {
                  // There are no other cases.
                  STBTT_assert(0);
                  break;
               };
               }
            }
            break;
         };

         default:
            // TODO: Implement other stuff.
            break;
         }
      }

      return 0;
   }

   STBTT_DEF int stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info, int g1, int g2)
   {
      int xAdvance = 0;

      if (info->gpos)
         xAdvance += stbtt__GetGlyphGPOSInfoAdvance(info, g1, g2);
      else if (info->kern)
         xAdvance += stbtt__GetGlyphKernInfoAdvance(info, g1, g2);

      return xAdvance;
   }

   STBTT_DEF int stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info, int ch1, int ch2)
   {
      if (!info->kern && !info->gpos) // if no kerning table, don't waste time looking up both codepoint->glyphs
         return 0;
      return stbtt_GetGlyphKernAdvance(info, stbtt_FindGlyphIndex(info, ch1), stbtt_FindGlyphIndex(info, ch2));
   }

   STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info, int codepoint, int *advanceWidth, int *leftSideBearing)
   {
      stbtt_GetGlyphHMetrics(info, stbtt_FindGlyphIndex(info, codepoint), advanceWidth, leftSideBearing);
   }

   /*STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap)
{
   if (ascent)
      *ascent = ttSHORT(info->data + info->hhea + 4);
   if (descent)
      *descent = ttSHORT(info->data + info->hhea + 6);
   if (lineGap)
      *lineGap = ttSHORT(info->data + info->hhea + 8);
}*/

   STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent, int *descent, int *lineGap)
   {
      if (ascent)
      {
         info->stream->seek(info->hhea + 4);
         *ascent = read_int16(info->stream);
      }
      if (descent)
      {
         info->stream->seek(info->hhea + 6);
         *descent = read_int16(info->stream);
      }
      if (lineGap)
      {
         info->stream->seek(info->hhea + 8);
         *lineGap = read_int16(info->stream);
      }
   }

   /*STBTT_DEF int stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info, int *typoAscent, int *typoDescent, int *typoLineGap)
   {
      int tab = stbtt__find_table(info->stream, info->fontstart, "OS/2");
      if (!tab)
         return 0;
      info->stream->seek(tab + 68);
      if (typoAscent)
         *typoAscent = read_int16(info->stream);
      if (typoDescent)
         *typoDescent = read_int16(info->stream);
      if (typoLineGap)
         *typoLineGap = read_int16(info->stream);
      return 1;
   }*/

   STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0, int *y0, int *x1, int *y1)
   {
      info->stream->seek(info->head + 36);
      *x0 = read_int16(info->stream);
      *y0 = read_int16(info->stream);
      *x1 = read_int16(info->stream);
      *y1 = read_int16(info->stream);
   }

   STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info, float height)
   {
      info->stream->seek(info->hhea + 4);
      int fheight = read_int16(info->stream) - read_int16(info->stream);
      return (float)height / fheight;
   }

   /*STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels)
   {
      info->stream->seek(info->head + 18);
      int unitsPerEm = read_uint16(info->stream);
      return pixels / unitsPerEm;
   }*/

   /*STBTT_DEF void stbtt_FreeShape(const stbtt_fontinfo *info, stbtt_vertex *v)
   {
      STBTT_free(v, info->userdata);
   }*/

   /*STBTT_DEF stbtt_uint32 stbtt_FindSVGDoc(const stbtt_fontinfo *info, int gl)
   {
      int i;
      stbtt_uint32 svg_doc_list = stbtt__get_svg((stbtt_fontinfo *)info);
      info->stream->seek(svg_doc_list);
      int numEntries = read_uint16(info->stream);
      stbtt_uint32 svg_docs = svg_doc_list + 2;

      for (i = 0; i < numEntries; i++)
      {
         stbtt_uint32 svg_doc = svg_docs + (12 * i);
         info->stream->seek(svg_doc);
         if ((gl >= read_uint16(info->stream)) && (gl <= read_uint16(info->stream)))
            return svg_doc;
      }
      return 0;
   }*/

   /*STBTT_DEF int stbtt_GetGlyphSVG(const stbtt_fontinfo *info, int gl, char *buffer, int buffer_length)
   {
      stbtt_uint32 svg_doc;

      if (info->svg == 0)
         return 0;

      svg_doc = stbtt_FindSVGDoc(info, gl);
      if (svg_doc != 0)
      {
         info->stream->seek(svg_doc + 4);
         stbtt_uint32 offs = read_uint32(info->stream);
         info->stream->seek(info->svg + offs);
         info->stream->read((uint8_t *)buffer, buffer_length);
         buffer[buffer_length - 1] = 0;
         info->stream->seek(svg_doc + 8);
         return read_uint32(info->stream);
      }
      else
      {
         return 0;
      }
   }*/

   /*STBTT_DEF int stbtt_GetCodepointSVG(const stbtt_fontinfo *info, int unicode_codepoint, char *buffer, int buffer_length)
   {
      return stbtt_GetGlyphSVG(info, stbtt_FindGlyphIndex(info, unicode_codepoint), buffer, buffer_length);
   }*/

   //////////////////////////////////////////////////////////////////////////////
   //
   // antialiasing software rasterizer
   //

   STBTT_DEF void stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
   {
      int x0 = 0, y0 = 0, x1, y1; // =0 suppresses compiler warning
      if (!stbtt_GetGlyphBox(font, glyph, &x0, &y0, &x1, &y1))
      {
         // e.g. space character
         if (ix0)
            *ix0 = 0;
         if (iy0)
            *iy0 = 0;
         if (ix1)
            *ix1 = 0;
         if (iy1)
            *iy1 = 0;
      }
      else
      {
         // move to integral bboxes (treating pixels as little squares, what pixels get touched)?
         if (ix0)
            *ix0 = STBTT_ifloor(x0 * scale_x + shift_x);
         if (iy0)
            *iy0 = STBTT_ifloor(-y1 * scale_y + shift_y);
         if (ix1)
            *ix1 = STBTT_iceil(x1 * scale_x + shift_x);
         if (iy1)
            *iy1 = STBTT_iceil(-y0 * scale_y + shift_y);
      }
   }

   /*STBTT_DEF void stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
   {
      stbtt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y, 0.0f, 0.0f, ix0, iy0, ix1, iy1);
   }*/

   STBTT_DEF void stbtt_GetCodepointBitmapBoxSubpixel(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
   {
      stbtt_GetGlyphBitmapBoxSubpixel(font, stbtt_FindGlyphIndex(font, codepoint), scale_x, scale_y, shift_x, shift_y, ix0, iy0, ix1, iy1);
   }

   /*STBTT_DEF void stbtt_GetCodepointBitmapBox(const stbtt_fontinfo *font, int codepoint, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
   {
      stbtt_GetCodepointBitmapBoxSubpixel(font, codepoint, scale_x, scale_y, 0.0f, 0.0f, ix0, iy0, ix1, iy1);
   }*/

   //////////////////////////////////////////////////////////////////////////////
   //
   //  Rasterizer

   typedef struct stbtt__hheap_chunk
   {
      struct stbtt__hheap_chunk *next;
   } stbtt__hheap_chunk;

   typedef struct stbtt__hheap
   {
      struct stbtt__hheap_chunk *head;
      void *first_free;
      int num_remaining_in_head_chunk;
   } stbtt__hheap;

   static void *stbtt__hheap_alloc(stbtt__hheap *hh, size_t size, void *userdata)
   {
      if (hh->first_free)
      {
         void *p = hh->first_free;
         hh->first_free = *(void **)p;
         return p;
      }
      else
      {
         if (hh->num_remaining_in_head_chunk == 0)
         {
            int count = (size < 32 ? 2000 : size < 128 ? 800
                                                       : 100);
            stbtt__hheap_chunk *c = (stbtt__hheap_chunk *)STBTT_malloc(sizeof(stbtt__hheap_chunk) + size * count, userdata);
            if (c == NULL)
               return NULL;
            c->next = hh->head;
            hh->head = c;
            hh->num_remaining_in_head_chunk = count;
         }
         --hh->num_remaining_in_head_chunk;
         return (char *)(hh->head) + sizeof(stbtt__hheap_chunk) + size * hh->num_remaining_in_head_chunk;
      }
   }

   static void stbtt__hheap_free(stbtt__hheap *hh, void *p)
   {
      *(void **)p = hh->first_free;
      hh->first_free = p;
   }

   static void stbtt__hheap_cleanup(stbtt__hheap *hh, void *userdata)
   {
      stbtt__hheap_chunk *c = hh->head;
      while (c)
      {
         stbtt__hheap_chunk *n = c->next;
         STBTT_free(c, userdata);
         c = n;
      }
   }

   typedef struct stbtt__edge
   {
      float x0, y0, x1, y1;
      int invert;
   } stbtt__edge;

   typedef struct stbtt__active_edge
   {
      struct stbtt__active_edge *next;
#if STBTT_RASTERIZER_VERSION == 1
      int x, dx;
      float ey;
      int direction;
#elif STBTT_RASTERIZER_VERSION == 2
   float fx, fdx, fdy;
   float direction;
   float sy;
   float ey;
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif
   } stbtt__active_edge;

#if STBTT_RASTERIZER_VERSION == 1
#define STBTT_FIXSHIFT 10
#define STBTT_FIX (1 << STBTT_FIXSHIFT)
#define STBTT_FIXMASK (STBTT_FIX - 1)

   static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
   {
      stbtt__active_edge *z = (stbtt__active_edge *)stbtt__hheap_alloc(hh, sizeof(*z), userdata);
      float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
      STBTT_assert(z != NULL);
      if (!z)
         return z;

      // round dx down to avoid overshooting
      if (dxdy < 0)
         z->dx = -STBTT_ifloor(STBTT_FIX * -dxdy);
      else
         z->dx = STBTT_ifloor(STBTT_FIX * dxdy);

      z->x = STBTT_ifloor(STBTT_FIX * e->x0 + z->dx * (start_point - e->y0)); // use z->dx so when we offset later it's by the same amount
      z->x -= off_x * STBTT_FIX;

      z->ey = e->y1;
      z->next = 0;
      z->direction = e->invert ? 1 : -1;
      return z;
   }
#elif STBTT_RASTERIZER_VERSION == 2
static stbtt__active_edge *stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
   stbtt__active_edge *z = (stbtt__active_edge *)stbtt__hheap_alloc(hh, sizeof(*z), userdata);
   float dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
   STBTT_assert(z != NULL);
   //STBTT_assert(e->y0 <= start_point);
   if (!z)
      return z;
   z->fdx = dxdy;
   z->fdy = dxdy != 0.0f ? (1.0f / dxdy) : 0.0f;
   z->fx = e->x0 + dxdy * (start_point - e->y0);
   z->fx -= off_x;
   z->direction = e->invert ? 1.0f : -1.0f;
   z->sy = e->y0;
   z->ey = e->y1;
   z->next = 0;
   return z;
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#if STBTT_RASTERIZER_VERSION == 1
   // note: this routine clips fills that extend off the edges... ideally this
   // wouldn't happen, but it could happen if the truetype glyph bounding boxes
   // are wrong, or if the user supplies a too-small bitmap
   static void stbtt__fill_active_edges(unsigned char *scanline, int len, stbtt__active_edge *e, int max_weight)
   {
      // non-zero winding fill
      int x0 = 0, w = 0;

      while (e)
      {
         if (w == 0)
         {
            // if we're currently at zero, we need to record the edge start point
            x0 = e->x;
            w += e->direction;
         }
         else
         {
            int x1 = e->x;
            w += e->direction;
            // if we went to zero, we need to draw
            if (w == 0)
            {
               int i = x0 >> STBTT_FIXSHIFT;
               int j = x1 >> STBTT_FIXSHIFT;

               if (i < len && j >= 0)
               {
                  if (i == j)
                  {
                     // x0,x1 are the same pixel, so compute combined coverage
                     scanline[i] = scanline[i] + (stbtt_uint8)((x1 - x0) * max_weight >> STBTT_FIXSHIFT);
                  }
                  else
                  {
                     if (i >= 0) // add antialiasing for x0
                        scanline[i] = scanline[i] + (stbtt_uint8)(((STBTT_FIX - (x0 & STBTT_FIXMASK)) * max_weight) >> STBTT_FIXSHIFT);
                     else
                        i = -1; // clip

                     if (j < len) // add antialiasing for x1
                        scanline[j] = scanline[j] + (stbtt_uint8)(((x1 & STBTT_FIXMASK) * max_weight) >> STBTT_FIXSHIFT);
                     else
                        j = len; // clip

                     for (++i; i < j; ++i) // fill pixels between x0 and x1
                        scanline[i] = scanline[i] + (stbtt_uint8)max_weight;
                  }
               }
            }
         }

         e = e->next;
      }
   }

   static void stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
   {
      stbtt__hheap hh = {0, 0, 0};
      stbtt__active_edge *active = NULL;
      int y, j = 0;
      int max_weight = (255 / vsubsample); // weight per vertical scanline
      int s;                               // vertical subsample index
      unsigned char scanline_data[512], *scanline;

      if (result->w > 512)
         scanline = (unsigned char *)STBTT_malloc(result->w, userdata);
      else
         scanline = scanline_data;

      y = off_y * vsubsample;
      e[n].y0 = (off_y + result->h) * (float)vsubsample + 1;

      while (j < result->h)
      {
         STBTT_memset(scanline, 0, result->w);
         for (s = 0; s < vsubsample; ++s)
         {
            // find center of pixel for this scanline
            float scan_y = y + 0.5f;
            stbtt__active_edge **step = &active;

            // update all active edges;
            // remove all active edges that terminate before the center of this scanline
            while (*step)
            {
               stbtt__active_edge *z = *step;
               if (z->ey <= scan_y)
               {
                  *step = z->next; // delete from list
                  STBTT_assert(z->direction);
                  z->direction = 0;
                  stbtt__hheap_free(&hh, z);
               }
               else
               {
                  z->x += z->dx;           // advance to position for current scanline
                  step = &((*step)->next); // advance through list
               }
            }

            // resort the list if needed
            for (;;)
            {
               int changed = 0;
               step = &active;
               while (*step && (*step)->next)
               {
                  if ((*step)->x > (*step)->next->x)
                  {
                     stbtt__active_edge *t = *step;
                     stbtt__active_edge *q = t->next;

                     t->next = q->next;
                     q->next = t;
                     *step = q;
                     changed = 1;
                  }
                  step = &(*step)->next;
               }
               if (!changed)
                  break;
            }

            // insert all edges that start before the center of this scanline -- omit ones that also end on this scanline
            while (e->y0 <= scan_y)
            {
               if (e->y1 > scan_y)
               {
                  stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y, userdata);
                  if (z != NULL)
                  {
                     // find insertion point
                     if (active == NULL)
                        active = z;
                     else if (z->x < active->x)
                     {
                        // insert at front
                        z->next = active;
                        active = z;
                     }
                     else
                     {
                        // find thing to insert AFTER
                        stbtt__active_edge *p = active;
                        while (p->next && p->next->x < z->x)
                           p = p->next;
                        // at this point, p->next->x is NOT < z->x
                        z->next = p->next;
                        p->next = z;
                     }
                  }
               }
               ++e;
            }

            // now process all active edges in XOR fashion
            if (active)
               stbtt__fill_active_edges(scanline, result->w, active, max_weight);

            ++y;
         }
         STBTT_memcpy(result->pixels + j * result->stride, scanline, result->w);
         ++j;
      }

      stbtt__hheap_cleanup(&hh, userdata);

      if (scanline != scanline_data)
         STBTT_free(scanline, userdata);
   }

#elif STBTT_RASTERIZER_VERSION == 2

// the edge passed in here does not cross the vertical line at x or the vertical line at x+1
// (i.e. it has already been clipped to those)
static void stbtt__handle_clipped_edge(float *scanline, int x, stbtt__active_edge *e, float x0, float y0, float x1, float y1)
{
   if (y0 == y1)
      return;
   STBTT_assert(y0 < y1);
   STBTT_assert(e->sy <= e->ey);
   if (y0 > e->ey)
      return;
   if (y1 < e->sy)
      return;
   if (y0 < e->sy)
   {
      x0 += (x1 - x0) * (e->sy - y0) / (y1 - y0);
      y0 = e->sy;
   }
   if (y1 > e->ey)
   {
      x1 += (x1 - x0) * (e->ey - y1) / (y1 - y0);
      y1 = e->ey;
   }

   if (x0 == x)
      STBTT_assert(x1 <= x + 1);
   else if (x0 == x + 1)
      STBTT_assert(x1 >= x);
   else if (x0 <= x)
      STBTT_assert(x1 <= x);
   else if (x0 >= x + 1)
      STBTT_assert(x1 >= x + 1);
   else
      STBTT_assert(x1 >= x && x1 <= x + 1);

   if (x0 <= x && x1 <= x)
      scanline[x] += e->direction * (y1 - y0);
   else if (x0 >= x + 1 && x1 >= x + 1)
      ;
   else
   {
      STBTT_assert(x0 >= x && x0 <= x + 1 && x1 >= x && x1 <= x + 1);
      scanline[x] += e->direction * (y1 - y0) * (1 - ((x0 - x) + (x1 - x)) / 2); // coverage = 1 - average x position
   }
}

static void stbtt__fill_active_edges_new(float *scanline, float *scanline_fill, int len, stbtt__active_edge *e, float y_top)
{
   float y_bottom = y_top + 1;

   while (e)
   {
      // brute force every pixel

      // compute intersection points with top & bottom
      STBTT_assert(e->ey >= y_top);

      if (e->fdx == 0)
      {
         float x0 = e->fx;
         if (x0 < len)
         {
            if (x0 >= 0)
            {
               stbtt__handle_clipped_edge(scanline, (int)x0, e, x0, y_top, x0, y_bottom);
               stbtt__handle_clipped_edge(scanline_fill - 1, (int)x0 + 1, e, x0, y_top, x0, y_bottom);
            }
            else
            {
               stbtt__handle_clipped_edge(scanline_fill - 1, 0, e, x0, y_top, x0, y_bottom);
            }
         }
      }
      else
      {
         float x0 = e->fx;
         float dx = e->fdx;
         float xb = x0 + dx;
         float x_top, x_bottom;
         float sy0, sy1;
         float dy = e->fdy;
         STBTT_assert(e->sy <= y_bottom && e->ey >= y_top);

         // compute endpoints of line segment clipped to this scanline (if the
         // line segment starts on this scanline. x0 is the intersection of the
         // line with y_top, but that may be off the line segment.
         if (e->sy > y_top)
         {
            x_top = x0 + dx * (e->sy - y_top);
            sy0 = e->sy;
         }
         else
         {
            x_top = x0;
            sy0 = y_top;
         }
         if (e->ey < y_bottom)
         {
            x_bottom = x0 + dx * (e->ey - y_top);
            sy1 = e->ey;
         }
         else
         {
            x_bottom = xb;
            sy1 = y_bottom;
         }

         if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len)
         {
            // from here on, we don't have to range check x values

            if ((int)x_top == (int)x_bottom)
            {
               float height;
               // simple case, only spans one pixel
               int x = (int)x_top;
               height = sy1 - sy0;
               STBTT_assert(x >= 0 && x < len);
               scanline[x] += e->direction * (1 - ((x_top - x) + (x_bottom - x)) / 2) * height;
               scanline_fill[x] += e->direction * height; // everything right of this pixel is filled
            }
            else
            {
               int x, x1, x2;
               float y_crossing, step, sign, area;
               // covers 2+ pixels
               if (x_top > x_bottom)
               {
                  // flip scanline vertically; signed area is the same
                  float t;
                  sy0 = y_bottom - (sy0 - y_top);
                  sy1 = y_bottom - (sy1 - y_top);
                  t = sy0, sy0 = sy1, sy1 = t;
                  t = x_bottom, x_bottom = x_top, x_top = t;
                  dx = -dx;
                  dy = -dy;
                  t = x0, x0 = xb, xb = t;
               }

               x1 = (int)x_top;
               x2 = (int)x_bottom;
               // compute intersection with y axis at x1+1
               y_crossing = (x1 + 1 - x0) * dy + y_top;

               sign = e->direction;
               // area of the rectangle covered from y0..y_crossing
               area = sign * (y_crossing - sy0);
               // area of the triangle (x_top,y0), (x+1,y0), (x+1,y_crossing)
               scanline[x1] += area * (1 - ((x_top - x1) + (x1 + 1 - x1)) / 2);

               step = sign * dy;
               for (x = x1 + 1; x < x2; ++x)
               {
                  scanline[x] += area + step / 2;
                  area += step;
               }
               y_crossing += dy * (x2 - (x1 + 1));

               STBTT_assert(STBTT_fabs(area) <= 1.01f);

               scanline[x2] += area + sign * (1 - ((x2 - x2) + (x_bottom - x2)) / 2) * (sy1 - y_crossing);

               scanline_fill[x2] += sign * (sy1 - sy0);
            }
         }
         else
         {
            // if edge goes outside of box we're drawing, we require
            // clipping logic. since this does not match the intended use
            // of this library, we use a different, very slow brute
            // force implementation
            int x;
            for (x = 0; x < len; ++x)
            {
               // cases:
               //
               // there can be up to two intersections with the pixel. any intersection
               // with left or right edges can be handled by splitting into two (or three)
               // regions. intersections with top & bottom do not necessitate case-wise logic.
               //
               // the old way of doing this found the intersections with the left & right edges,
               // then used some simple logic to produce up to three segments in sorted order
               // from top-to-bottom. however, this had a problem: if an x edge was epsilon
               // across the x border, then the corresponding y position might not be distinct
               // from the other y segment, and it might ignored as an empty segment. to avoid
               // that, we need to explicitly produce segments based on x positions.

               // rename variables to clearly-defined pairs
               float y0 = y_top;
               float x1 = (float)(x);
               float x2 = (float)(x + 1);
               float x3 = xb;
               float y3 = y_bottom;

               // x = e->x + e->dx * (y-y_top)
               // (y-y_top) = (x - e->x) / e->dx
               // y = (x - e->x) / e->dx + y_top
               float y1 = (x - x0) / dx + y_top;
               float y2 = (x + 1 - x0) / dx + y_top;

               if (x0 < x1 && x3 > x2)
               { // three segments descending down-right
                  stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                  stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x2, y2);
                  stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
               }
               else if (x3 < x1 && x0 > x2)
               { // three segments descending down-left
                  stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                  stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x1, y1);
                  stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
               }
               else if (x0 < x1 && x3 > x1)
               { // two segments across x, down-right
                  stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                  stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
               }
               else if (x3 < x1 && x0 > x1)
               { // two segments across x, down-left
                  stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
                  stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
               }
               else if (x0 < x2 && x3 > x2)
               { // two segments across x+1, down-right
                  stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                  stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
               }
               else if (x3 < x2 && x0 > x2)
               { // two segments across x+1, down-left
                  stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
                  stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
               }
               else
               { // one segment
                  stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x3, y3);
               }
            }
         }
      }
      e = e->next;
   }
}

// directly AA rasterize edges w/o supersampling
// TODO: Implement some error checking!
static void stbtt__rasterize_sorted_edges(stbtt_render_callback callback, void *callback_state, int target_width, int target_height, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
   stbtt__hheap hh = {0, 0, 0};
   stbtt__active_edge *active = NULL;
   int y, j = 0, i;
   float scanline_data[129], *scanline, *scanline2;

   STBTT__NOTUSED(vsubsample);

   if (target_width > 64)
      scanline = (float *)STBTT_malloc((target_width * 2 + 1) * sizeof(float), userdata);
   else
      scanline = scanline_data;

   scanline2 = scanline + target_width;

   y = off_y;
   e[n].y0 = (float)(off_y + target_height) + 1;

   while (j < target_height)
   {
      // find center of pixel for this scanline
      float scan_y_top = y + 0.0f;
      float scan_y_bottom = y + 1.0f;
      stbtt__active_edge **step = &active;

      STBTT_memset(scanline, 0, target_width * sizeof(scanline[0]));
      STBTT_memset(scanline2, 0, (target_width + 1) * sizeof(scanline[0]));

      // update all active edges;
      // remove all active edges that terminate before the top of this scanline
      while (*step)
      {
         stbtt__active_edge *z = *step;
         if (z->ey <= scan_y_top)
         {
            *step = z->next; // delete from list
            STBTT_assert(z->direction);
            z->direction = 0;
            stbtt__hheap_free(&hh, z);
         }
         else
         {
            step = &((*step)->next); // advance through list
         }
      }

      // insert all edges that start before the bottom of this scanline
      while (e->y0 <= scan_y_bottom)
      {
         if (e->y0 != e->y1)
         {
            stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y_top, userdata);
            if (z != NULL)
            {
               if (j == 0 && off_y != 0)
               {
                  if (z->ey < scan_y_top)
                  {
                     // this can happen due to subpixel positioning and some kind of fp rounding error i think
                     z->ey = scan_y_top;
                  }
               }
               STBTT_assert(z->ey >= scan_y_top); // if we get really unlucky a tiny bit of an edge can be out of bounds
               // insert at front
               z->next = active;
               active = z;
            }
         }
         ++e;
      }

      // now process all active edges
      if (active)
         stbtt__fill_active_edges_new(scanline, scanline2 + 1, target_width, active, scan_y_top);

      {
         float sum = 0;
         for (i = 0; i < target_width; ++i)
         {
            float k;
            int m;
            sum += scanline2[i];
            k = scanline[i] + sum;
            k = (float)STBTT_fabs(k) * 255 + 0.5f;
            m = (int)k;
            if (m > 255)
               m = 255;
            callback(i, j, m, callback_state);
               
            //result->pixels[j * result->stride + i] = (unsigned char)m;
         }
      }
      // advance all the edges
      step = &active;
      while (*step)
      {
         stbtt__active_edge *z = *step;
         z->fx += z->fdx;         // advance to position for current scanline
         step = &((*step)->next); // advance through list
      }

      ++y;
      ++j;
   }

   stbtt__hheap_cleanup(&hh, userdata);

   if (scanline != scanline_data)
      STBTT_free(scanline, userdata);
}
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif

#define STBTT__COMPARE(a, b) ((a)->y0 < (b)->y0)

   static void stbtt__sort_edges_ins_sort(stbtt__edge *p, int n)
   {
      int i, j;
      for (i = 1; i < n; ++i)
      {
         stbtt__edge t = p[i], *a = &t;
         j = i;
         while (j > 0)
         {
            stbtt__edge *b = &p[j - 1];
            int c = STBTT__COMPARE(a, b);
            if (!c)
               break;
            p[j] = p[j - 1];
            --j;
         }
         if (i != j)
            p[j] = t;
      }
   }

   static void stbtt__sort_edges_quicksort(stbtt__edge *p, int n)
   {
      /* threshold for transitioning to insertion sort */
      while (n > 12)
      {
         stbtt__edge t;
         int c01, c12, c, m, i, j;

         /* compute median of three */
         m = n >> 1;
         c01 = STBTT__COMPARE(&p[0], &p[m]);
         c12 = STBTT__COMPARE(&p[m], &p[n - 1]);
         /* if 0 >= mid >= end, or 0 < mid < end, then use mid */
         if (c01 != c12)
         {
            /* otherwise, we'll need to swap something else to middle */
            int z;
            c = STBTT__COMPARE(&p[0], &p[n - 1]);
            /* 0>mid && mid<n:  0>n => n; 0<n => 0 */
            /* 0<mid && mid>n:  0>n => 0; 0<n => n */
            z = (c == c12) ? 0 : n - 1;
            t = p[z];
            p[z] = p[m];
            p[m] = t;
         }
         /* now p[m] is the median-of-three */
         /* swap it to the beginning so it won't move around */
         t = p[0];
         p[0] = p[m];
         p[m] = t;

         /* partition loop */
         i = 1;
         j = n - 1;
         for (;;)
         {
            /* handling of equality is crucial here */
            /* for sentinels & efficiency with duplicates */
            for (;; ++i)
            {
               if (!STBTT__COMPARE(&p[i], &p[0]))
                  break;
            }
            for (;; --j)
            {
               if (!STBTT__COMPARE(&p[0], &p[j]))
                  break;
            }
            /* make sure we haven't crossed */
            if (i >= j)
               break;
            t = p[i];
            p[i] = p[j];
            p[j] = t;

            ++i;
            --j;
         }
         /* recurse on smaller side, iterate on larger */
         if (j < (n - i))
         {
            stbtt__sort_edges_quicksort(p, j);
            p = p + i;
            n = n - i;
         }
         else
         {
            stbtt__sort_edges_quicksort(p + i, n - i);
            n = j;
         }
      }
   }

   static void stbtt__sort_edges(stbtt__edge *p, int n)
   {
      stbtt__sort_edges_quicksort(p, n);
      stbtt__sort_edges_ins_sort(p, n);
   }

   typedef struct
   {
      float x, y;
   } stbtt__point;

   static void stbtt__rasterize(stbtt_render_callback callback, void *callback_state, int target_width, int target_height, stbtt__point *pts, int *wcount, int windings, float scale_x, float scale_y, float shift_x, float shift_y, int off_x, int off_y, int invert, void *userdata)
   {
      float y_scale_inv = invert ? -scale_y : scale_y;
      stbtt__edge *e;
      int n, i, j, k, m;
#if STBTT_RASTERIZER_VERSION == 1
      int vsubsample = result->h < 8 ? 15 : 5;
#elif STBTT_RASTERIZER_VERSION == 2
   int vsubsample = 1;
#else
#error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#endif
      // vsubsample should divide 255 evenly; otherwise we won't reach full opacity

      // now we have to blow out the windings into explicit edge lists
      n = 0;
      for (i = 0; i < windings; ++i)
         n += wcount[i];

      e = (stbtt__edge *)STBTT_malloc(sizeof(*e) * (n + 1), userdata); // add an extra one as a sentinel
      if (e == 0)
         return;
      n = 0;

      m = 0;
      for (i = 0; i < windings; ++i)
      {
         stbtt__point *p = pts + m;
         m += wcount[i];
         j = wcount[i] - 1;
         for (k = 0; k < wcount[i]; j = k++)
         {
            int a = k, b = j;
            // skip the edge if horizontal
            if (p[j].y == p[k].y)
               continue;
            // add edge from j to k to the list
            e[n].invert = 0;
            if (invert ? p[j].y > p[k].y : p[j].y < p[k].y)
            {
               e[n].invert = 1;
               a = j, b = k;
            }
            e[n].x0 = p[a].x * scale_x + shift_x;
            e[n].y0 = (p[a].y * y_scale_inv + shift_y) * vsubsample;
            e[n].x1 = p[b].x * scale_x + shift_x;
            e[n].y1 = (p[b].y * y_scale_inv + shift_y) * vsubsample;
            ++n;
         }
      }

      // now sort the edges by their highest point (should snap to integer, and then by x)
      //STBTT_sort(e, n, sizeof(e[0]), stbtt__edge_compare);
      stbtt__sort_edges(e, n);

      // now, traverse the scanlines and find the intersections on each scanline, use xor winding rule
      stbtt__rasterize_sorted_edges(callback, callback_state, target_width, target_height, e, n, vsubsample, off_x, off_y, userdata);

      STBTT_free(e, userdata);
   }

   static void stbtt__add_point(stbtt__point *points, int n, float x, float y)
   {
      if (!points)
         return; // during first pass, it's unallocated
      points[n].x = x;
      points[n].y = y;
   }

   // tessellate until threshold p is happy... @TODO warped to compensate for non-linear stretching
   static int stbtt__tesselate_curve(stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n)
   {
      // midpoint
      float mx = (x0 + 2 * x1 + x2) / 4;
      float my = (y0 + 2 * y1 + y2) / 4;
      // versus directly drawn line
      float dx = (x0 + x2) / 2 - mx;
      float dy = (y0 + y2) / 2 - my;
      if (n > 16) // 65536 segments on one curve better be enough!
         return 1;
      if (dx * dx + dy * dy > objspace_flatness_squared)
      { // half-pixel error allowed... need to be smaller if AA
         stbtt__tesselate_curve(points, num_points, x0, y0, (x0 + x1) / 2.0f, (y0 + y1) / 2.0f, mx, my, objspace_flatness_squared, n + 1);
         stbtt__tesselate_curve(points, num_points, mx, my, (x1 + x2) / 2.0f, (y1 + y2) / 2.0f, x2, y2, objspace_flatness_squared, n + 1);
      }
      else
      {
         stbtt__add_point(points, *num_points, x2, y2);
         *num_points = *num_points + 1;
      }
      return 1;
   }

   static void stbtt__tesselate_cubic(stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float objspace_flatness_squared, int n)
   {
      // @TODO this "flatness" calculation is just made-up nonsense that seems to work well enough
      float dx0 = x1 - x0;
      float dy0 = y1 - y0;
      float dx1 = x2 - x1;
      float dy1 = y2 - y1;
      float dx2 = x3 - x2;
      float dy2 = y3 - y2;
      float dx = x3 - x0;
      float dy = y3 - y0;
      float longlen = (float)(STBTT_sqrt(dx0 * dx0 + dy0 * dy0) + STBTT_sqrt(dx1 * dx1 + dy1 * dy1) + STBTT_sqrt(dx2 * dx2 + dy2 * dy2));
      float shortlen = (float)STBTT_sqrt(dx * dx + dy * dy);
      float flatness_squared = longlen * longlen - shortlen * shortlen;

      if (n > 16) // 65536 segments on one curve better be enough!
         return;

      if (flatness_squared > objspace_flatness_squared)
      {
         float x01 = (x0 + x1) / 2;
         float y01 = (y0 + y1) / 2;
         float x12 = (x1 + x2) / 2;
         float y12 = (y1 + y2) / 2;
         float x23 = (x2 + x3) / 2;
         float y23 = (y2 + y3) / 2;

         float xa = (x01 + x12) / 2;
         float ya = (y01 + y12) / 2;
         float xb = (x12 + x23) / 2;
         float yb = (y12 + y23) / 2;

         float mx = (xa + xb) / 2;
         float my = (ya + yb) / 2;

         stbtt__tesselate_cubic(points, num_points, x0, y0, x01, y01, xa, ya, mx, my, objspace_flatness_squared, n + 1);
         stbtt__tesselate_cubic(points, num_points, mx, my, xb, yb, x23, y23, x3, y3, objspace_flatness_squared, n + 1);
      }
      else
      {
         stbtt__add_point(points, *num_points, x3, y3);
         *num_points = *num_points + 1;
      }
   }

   // returns number of contours
   static stbtt__point *stbtt_FlattenCurves(stbtt_vertex *vertices, int num_verts, float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata)
   {
      stbtt__point *points = 0;
      int num_points = 0;

      float objspace_flatness_squared = objspace_flatness * objspace_flatness;
      int i, n = 0, start = 0, pass;

      // count how many "moves" there are to get the contour count
      for (i = 0; i < num_verts; ++i)
         if (vertices[i].type == STBTT_vmove)
            ++n;

      *num_contours = n;
      if (n == 0)
         return 0;

      *contour_lengths = (int *)STBTT_malloc(sizeof(**contour_lengths) * n, userdata);

      if (*contour_lengths == 0)
      {
         *num_contours = 0;
         return 0;
      }

      // make two passes through the points so we don't need to realloc
      for (pass = 0; pass < 2; ++pass)
      {
         float x = 0, y = 0;
         if (pass == 1)
         {
            points = (stbtt__point *)STBTT_malloc(num_points * sizeof(points[0]), userdata);
            if (points == NULL)
               goto error;
         }
         num_points = 0;
         n = -1;
         for (i = 0; i < num_verts; ++i)
         {
            switch (vertices[i].type)
            {
            case STBTT_vmove:
               // start the next contour
               if (n >= 0)
                  (*contour_lengths)[n] = num_points - start;
               ++n;
               start = num_points;

               x = vertices[i].x, y = vertices[i].y;
               stbtt__add_point(points, num_points++, x, y);
               break;
            case STBTT_vline:
               x = vertices[i].x, y = vertices[i].y;
               stbtt__add_point(points, num_points++, x, y);
               break;
            case STBTT_vcurve:
               stbtt__tesselate_curve(points, &num_points, x, y,
                                      vertices[i].cx, vertices[i].cy,
                                      vertices[i].x, vertices[i].y,
                                      objspace_flatness_squared, 0);
               x = vertices[i].x, y = vertices[i].y;
               break;
            case STBTT_vcubic:
               stbtt__tesselate_cubic(points, &num_points, x, y,
                                      vertices[i].cx, vertices[i].cy,
                                      vertices[i].cx1, vertices[i].cy1,
                                      vertices[i].x, vertices[i].y,
                                      objspace_flatness_squared, 0);
               x = vertices[i].x, y = vertices[i].y;
               break;
            }
         }
         (*contour_lengths)[n] = num_points - start;
      }

      return points;
   error:
      STBTT_free(points, userdata);
      STBTT_free(*contour_lengths, userdata);
      *contour_lengths = 0;
      *num_contours = 0;
      return NULL;
   }

   STBTT_DEF void stbtt_Rasterize(stbtt_render_callback callback, void *callback_state, int target_width, int target_height, float flatness_in_pixels, stbtt_vertex *vertices, int num_verts, float scale_x, float scale_y, float shift_x, float shift_y, int x_off, int y_off, int invert, void *userdata)
   {
      float scale = scale_x > scale_y ? scale_y : scale_x;
      int winding_count = 0;
      int *winding_lengths = NULL;
      stbtt__point *windings = stbtt_FlattenCurves(vertices, num_verts, flatness_in_pixels / scale, &winding_lengths, &winding_count, userdata);
      if (windings)
      {
         stbtt__rasterize(callback, callback_state, target_width, target_height, windings, winding_lengths, winding_count, scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, userdata);
         STBTT_free(winding_lengths, userdata);
         STBTT_free(windings, userdata);
      }
   }

   /*STBTT_DEF void stbtt_FreeBitmap(unsigned char *bitmap, void *userdata)
   {
      STBTT_free(bitmap, userdata);
   }*/

   /*STBTT_DEF unsigned char *stbtt_GetGlyphBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
   int ix0, iy0, ix1, iy1;
   stbtt__bitmap gbm;
   stbtt_vertex *vertices;
   int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);

   if (scale_x == 0)
      scale_x = scale_y;
   if (scale_y == 0)
   {
      if (scale_x == 0)
      {
         STBTT_free(vertices, info->userdata);
         return NULL;
      }
      scale_y = scale_x;
   }

   stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0, &iy0, &ix1, &iy1);

   // now we get the size
   gbm.w = (ix1 - ix0);
   gbm.h = (iy1 - iy0);
   gbm.pixels = NULL; // in case we error

   if (width)
      *width = gbm.w;
   if (height)
      *height = gbm.h;
   if (xoff)
      *xoff = ix0;
   if (yoff)
      *yoff = iy0;

   if (gbm.w && gbm.h)
   {
      gbm.pixels = (unsigned char *)STBTT_malloc(gbm.w * gbm.h, info->userdata);
      if (gbm.pixels)
      {
         gbm.stride = gbm.w;

         stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0, iy0, 1, info->userdata);
      }
   }
   STBTT_free(vertices, info->userdata);
   return gbm.pixels;
}*/

   /*STBTT_DEF unsigned char *stbtt_GetGlyphBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int glyph, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetGlyphBitmapSubpixel(info, scale_x, scale_y, 0.0f, 0.0f, glyph, width, height, xoff, yoff);
}*/

   STBTT_DEF void stbtt_MakeGlyphBitmapSubpixel(const stbtt_fontinfo *info, stbtt_render_callback callback, void *callback_state, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph)
   {
      int ix0, iy0;
      stbtt_vertex *vertices;
      int num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);

      stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0, &iy0, 0, 0);
      if (out_w && out_h)
         stbtt_Rasterize(callback, callback_state, out_w, out_h, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0, iy0, 1, info->userdata);

      STBTT_free(vertices, info->userdata);
   }

   /*STBTT_DEF void stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph)
{
   stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f, 0.0f, glyph);
}

STBTT_DEF unsigned char *stbtt_GetCodepointBitmapSubpixel(const stbtt_fontinfo *info, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetGlyphBitmapSubpixel(info, scale_x, scale_y, shift_x, shift_y, stbtt_FindGlyphIndex(info, codepoint), width, height, xoff, yoff);
}

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int oversample_x, int oversample_y, float *sub_x, float *sub_y, int codepoint)
{
   stbtt_MakeGlyphBitmapSubpixelPrefilter(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, oversample_x, oversample_y, sub_x, sub_y, stbtt_FindGlyphIndex(info, codepoint));
}

STBTT_DEF void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint)
{
   stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, stbtt_FindGlyphIndex(info, codepoint));
}*/
   /*STBTT_DEF void stbtt_MakeCodepointBitmapSubpixel(const stbtt_fontinfo *info, stbtt_render_callback callback, void *callback_state, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int codepoint)
   {
      stbtt_MakeGlyphBitmapSubpixel(info, callback, callback_state, out_w, out_h, out_stride, scale_x, scale_y, shift_x, shift_y, stbtt_FindGlyphIndex(info, codepoint));
   }*/

   /*STBTT_DEF unsigned char *stbtt_GetCodepointBitmap(const stbtt_fontinfo *info, float scale_x, float scale_y, int codepoint, int *width, int *height, int *xoff, int *yoff)
{
   return stbtt_GetCodepointBitmapSubpixel(info, scale_x, scale_y, 0.0f, 0.0f, codepoint, width, height, xoff, yoff);
}

STBTT_DEF void stbtt_MakeCodepointBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int codepoint)
{
   stbtt_MakeCodepointBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f, 0.0f, codepoint);
}*/

   //////////////////////////////////////////////////////////////////////////////
   //
   // bitmap baking
   //
   // This is SUPER-CRAPPY packing to keep source code small
   /*
static int stbtt_BakeFontBitmap_internal(unsigned char *data, int offset,  // font location (use offset=0 for plain .ttf)
                                float pixel_height,                     // height of font in pixels
                                unsigned char *pixels, int pw, int ph,  // bitmap to be filled in
                                int first_char, int num_chars,          // characters to bake
                                stbtt_bakedchar *chardata)
{
   float scale;
   int x,y,bottom_y, i;
   stbtt_fontinfo f;
   f.userdata = NULL;
   if (!stbtt_InitFont(&f, data, offset))
      return -1;
   STBTT_memset(pixels, 0, pw*ph); // background of 0 around pixels
   x=y=1;
   bottom_y = 1;

   scale = stbtt_ScaleForPixelHeight(&f, pixel_height);

   for (i=0; i < num_chars; ++i) {
      int advance, lsb, x0,y0,x1,y1,gw,gh;
      int g = stbtt_FindGlyphIndex(&f, first_char + i);
      stbtt_GetGlyphHMetrics(&f, g, &advance, &lsb);
      stbtt_GetGlyphBitmapBox(&f, g, scale,scale, &x0,&y0,&x1,&y1);
      gw = x1-x0;
      gh = y1-y0;
      if (x + gw + 1 >= pw)
         y = bottom_y, x = 1; // advance to next row
      if (y + gh + 1 >= ph) // check if it fits vertically AFTER potentially moving to next row
         return -i;
      STBTT_assert(x+gw < pw);
      STBTT_assert(y+gh < ph);
      stbtt_MakeGlyphBitmap(&f, pixels+x+y*pw, gw,gh,pw, scale,scale, g);
      chardata[i].x0 = (stbtt_int16) x;
      chardata[i].y0 = (stbtt_int16) y;
      chardata[i].x1 = (stbtt_int16) (x + gw);
      chardata[i].y1 = (stbtt_int16) (y + gh);
      chardata[i].xadvance = scale * advance;
      chardata[i].xoff     = (float) x0;
      chardata[i].yoff     = (float) y0;
      x = x + gw + 1;
      if (y+gh+1 > bottom_y)
         bottom_y = y+gh+1;
   }
   return bottom_y;
}
*/
   /*STBTT_DEF void stbtt_GetBakedQuad(const stbtt_bakedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stbtt_aligned_quad *q, int opengl_fillrule)
   {
      float d3d_bias = opengl_fillrule ? 0 : -0.5f;
      float ipw = 1.0f / pw, iph = 1.0f / ph;
      const stbtt_bakedchar *b = chardata + char_index;
      int round_x = STBTT_ifloor((*xpos + b->xoff) + 0.5f);
      int round_y = STBTT_ifloor((*ypos + b->yoff) + 0.5f);

      q->x0 = round_x + d3d_bias;
      q->y0 = round_y + d3d_bias;
      q->x1 = round_x + b->x1 - b->x0 + d3d_bias;
      q->y1 = round_y + b->y1 - b->y0 + d3d_bias;

      q->s0 = b->x0 * ipw;
      q->t0 = b->y0 * iph;
      q->s1 = b->x1 * ipw;
      q->t1 = b->y1 * iph;

      *xpos += b->xadvance;
   }*/

   //////////////////////////////////////////////////////////////////////////////
   //
   // rectangle packing replacement routines if you don't have stb_rect_pack.h
   //

#ifndef STB_RECT_PACK_VERSION

   typedef int stbrp_coord;

   ////////////////////////////////////////////////////////////////////////////////////
   //                                                                                //
   //                                                                                //
   // COMPILER WARNING ?!?!?                                                         //
   //                                                                                //
   //                                                                                //
   // if you get a compile warning due to these symbols being defined more than      //
   // once, move #include "stb_rect_pack.h" before #include "stb_truetype.h"         //
   //                                                                                //
   ////////////////////////////////////////////////////////////////////////////////////

   typedef struct
   {
      int width, height;
      int x, y, bottom_y;
   } stbrp_context;

   typedef struct
   {
      unsigned char x;
   } stbrp_node;

   struct stbrp_rect
   {
      stbrp_coord x, y;
      int id, w, h, was_packed;
   };

   static void stbrp_init_target(stbrp_context *con, int pw, int ph, stbrp_node *nodes, int num_nodes)
   {
      con->width = pw;
      con->height = ph;
      con->x = 0;
      con->y = 0;
      con->bottom_y = 0;
      STBTT__NOTUSED(nodes);
      STBTT__NOTUSED(num_nodes);
   }

   static void stbrp_pack_rects(stbrp_context *con, stbrp_rect *rects, int num_rects)
   {
      int i;
      for (i = 0; i < num_rects; ++i)
      {
         if (con->x + rects[i].w > con->width)
         {
            con->x = 0;
            con->y = con->bottom_y;
         }
         if (con->y + rects[i].h > con->height)
            break;
         rects[i].x = con->x;
         rects[i].y = con->y;
         rects[i].was_packed = 1;
         con->x += rects[i].w;
         if (con->y + rects[i].h > con->bottom_y)
            con->bottom_y = con->y + rects[i].h;
      }
      for (; i < num_rects; ++i)
         rects[i].was_packed = 0;
   }
#endif

   //////////////////////////////////////////////////////////////////////////////
   //
   // bitmap baking
   //
   // This is SUPER-AWESOME (tm Ryan Gordon) packing using stb_rect_pack.h. If
   // stb_rect_pack.h isn't available, it uses the BakeFontBitmap strategy.

   /*STBTT_DEF int stbtt_PackBegin(stbtt_pack_context *spc, unsigned char *pixels, int pw, int ph, int stride_in_bytes, int padding, void *alloc_context)
   {
      stbrp_context *context = (stbrp_context *)STBTT_malloc(sizeof(*context), alloc_context);
      int num_nodes = pw - padding;
      stbrp_node *nodes = (stbrp_node *)STBTT_malloc(sizeof(*nodes) * num_nodes, alloc_context);

      if (context == NULL || nodes == NULL)
      {
         if (context != NULL)
            STBTT_free(context, alloc_context);
         if (nodes != NULL)
            STBTT_free(nodes, alloc_context);
         return 0;
      }

      spc->user_allocator_context = alloc_context;
      spc->width = pw;
      spc->height = ph;
      spc->pixels = pixels;
      spc->pack_info = context;
      spc->nodes = nodes;
      spc->padding = padding;
      spc->stride_in_bytes = stride_in_bytes != 0 ? stride_in_bytes : pw;
      spc->h_oversample = 1;
      spc->v_oversample = 1;
      spc->skip_missing = 0;

      stbrp_init_target(context, pw - padding, ph - padding, nodes, num_nodes);

      if (pixels)
         STBTT_memset(pixels, 0, pw * ph); // background of 0 around pixels

      return 1;
   }*/

   /*STBTT_DEF void stbtt_PackEnd(stbtt_pack_context *spc)
   {
      STBTT_free(spc->nodes, spc->user_allocator_context);
      STBTT_free(spc->pack_info, spc->user_allocator_context);
   }*/

   /*STBTT_DEF void stbtt_PackSetOversampling(stbtt_pack_context *spc, unsigned int h_oversample, unsigned int v_oversample)
   {
      STBTT_assert(h_oversample <= STBTT_MAX_OVERSAMPLE);
      STBTT_assert(v_oversample <= STBTT_MAX_OVERSAMPLE);
      if (h_oversample <= STBTT_MAX_OVERSAMPLE)
         spc->h_oversample = h_oversample;
      if (v_oversample <= STBTT_MAX_OVERSAMPLE)
         spc->v_oversample = v_oversample;
   }*/

   /*STBTT_DEF void stbtt_PackSetSkipMissingCodepoints(stbtt_pack_context *spc, int skip)
   {
      spc->skip_missing = skip;
   }*/

#define STBTT__OVER_MASK (STBTT_MAX_OVERSAMPLE - 1)

   /*static void stbtt__h_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
   {
      unsigned char buffer[STBTT_MAX_OVERSAMPLE];
      int safe_w = w - kernel_width;
      int j;
      STBTT_memset(buffer, 0, STBTT_MAX_OVERSAMPLE); // suppress bogus warning from VS2013 -analyze
      for (j = 0; j < h; ++j)
      {
         int i;
         unsigned int total;
         STBTT_memset(buffer, 0, kernel_width);

         total = 0;

         // make kernel_width a constant in common cases so compiler can optimize out the divide
         switch (kernel_width)
         {
         case 2:
            for (i = 0; i <= safe_w; ++i)
            {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char)(total / 2);
            }
            break;
         case 3:
            for (i = 0; i <= safe_w; ++i)
            {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char)(total / 3);
            }
            break;
         case 4:
            for (i = 0; i <= safe_w; ++i)
            {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char)(total / 4);
            }
            break;
         case 5:
            for (i = 0; i <= safe_w; ++i)
            {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char)(total / 5);
            }
            break;
         default:
            for (i = 0; i <= safe_w; ++i)
            {
               total += pixels[i] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i];
               pixels[i] = (unsigned char)(total / kernel_width);
            }
            break;
         }

         for (; i < w; ++i)
         {
            STBTT_assert(pixels[i] == 0);
            total -= buffer[i & STBTT__OVER_MASK];
            pixels[i] = (unsigned char)(total / kernel_width);
         }

         pixels += stride_in_bytes;
      }
   }*/

   /*static void stbtt__v_prefilter(unsigned char *pixels, int w, int h, int stride_in_bytes, unsigned int kernel_width)
   {
      unsigned char buffer[STBTT_MAX_OVERSAMPLE];
      int safe_h = h - kernel_width;
      int j;
      STBTT_memset(buffer, 0, STBTT_MAX_OVERSAMPLE); // suppress bogus warning from VS2013 -analyze
      for (j = 0; j < w; ++j)
      {
         int i;
         unsigned int total;
         STBTT_memset(buffer, 0, kernel_width);

         total = 0;

         // make kernel_width a constant in common cases so compiler can optimize out the divide
         switch (kernel_width)
         {
         case 2:
            for (i = 0; i <= safe_h; ++i)
            {
               total += pixels[i * stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i * stride_in_bytes];
               pixels[i * stride_in_bytes] = (unsigned char)(total / 2);
            }
            break;
         case 3:
            for (i = 0; i <= safe_h; ++i)
            {
               total += pixels[i * stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i * stride_in_bytes];
               pixels[i * stride_in_bytes] = (unsigned char)(total / 3);
            }
            break;
         case 4:
            for (i = 0; i <= safe_h; ++i)
            {
               total += pixels[i * stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i * stride_in_bytes];
               pixels[i * stride_in_bytes] = (unsigned char)(total / 4);
            }
            break;
         case 5:
            for (i = 0; i <= safe_h; ++i)
            {
               total += pixels[i * stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i * stride_in_bytes];
               pixels[i * stride_in_bytes] = (unsigned char)(total / 5);
            }
            break;
         default:
            for (i = 0; i <= safe_h; ++i)
            {
               total += pixels[i * stride_in_bytes] - buffer[i & STBTT__OVER_MASK];
               buffer[(i + kernel_width) & STBTT__OVER_MASK] = pixels[i * stride_in_bytes];
               pixels[i * stride_in_bytes] = (unsigned char)(total / kernel_width);
            }
            break;
         }

         for (; i < h; ++i)
         {
            STBTT_assert(pixels[i * stride_in_bytes] == 0);
            total -= buffer[i & STBTT__OVER_MASK];
            pixels[i * stride_in_bytes] = (unsigned char)(total / kernel_width);
         }

         pixels += 1;
      }
   }*/

   /*static float stbtt__oversample_shift(int oversample)
   {
      if (!oversample)
         return 0.0f;

      // The prefilter is a box filter of width "oversample",
      // which shifts phase by (oversample - 1)/2 pixels in
      // oversampled space. We want to shift in the opposite
      // direction to counter this.
      return (float)-(oversample - 1) / (2.0f * (float)oversample);
   }*/

   // rects array must be big enough to accommodate all characters in the given ranges
   /*STBTT_DEF int stbtt_PackFontRangesGatherRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
   {
      int i, j, k;
      int missing_glyph_added = 0;

      k = 0;
      for (i = 0; i < num_ranges; ++i)
      {
         float fh = ranges[i].font_size;
         float scale = fh > 0 ? stbtt_ScaleForPixelHeight(info, fh) : stbtt_ScaleForMappingEmToPixels(info, -fh);
         ranges[i].h_oversample = (unsigned char)spc->h_oversample;
         ranges[i].v_oversample = (unsigned char)spc->v_oversample;
         for (j = 0; j < ranges[i].num_chars; ++j)
         {
            int x0, y0, x1, y1;
            int codepoint = ranges[i].array_of_unicode_codepoints == NULL ? ranges[i].first_unicode_codepoint_in_range + j : ranges[i].array_of_unicode_codepoints[j];
            int glyph = stbtt_FindGlyphIndex(info, codepoint);
            if (glyph == 0 && (spc->skip_missing || missing_glyph_added))
            {
               rects[k].w = rects[k].h = 0;
            }
            else
            {
               stbtt_GetGlyphBitmapBoxSubpixel(info, glyph,
                                               scale * spc->h_oversample,
                                               scale * spc->v_oversample,
                                               0, 0,
                                               &x0, &y0, &x1, &y1);
               rects[k].w = (stbrp_coord)(x1 - x0 + spc->padding + spc->h_oversample - 1);
               rects[k].h = (stbrp_coord)(y1 - y0 + spc->padding + spc->v_oversample - 1);
               if (glyph == 0)
                  missing_glyph_added = 1;
            }
            ++k;
         }
      }

      return k;
   }*/

   /*STBTT_DEF void stbtt_MakeGlyphBitmapSubpixelPrefilter(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int prefilter_x, int prefilter_y, float *sub_x, float *sub_y, int glyph)
{
   stbtt_MakeGlyphBitmapSubpixel(info,
                                 output,
                                 out_w - (prefilter_x - 1),
                                 out_h - (prefilter_y - 1),
                                 out_stride,
                                 scale_x,
                                 scale_y,
                                 shift_x,
                                 shift_y,
                                 glyph);

   if (prefilter_x > 1)
      stbtt__h_prefilter(output, out_w, out_h, out_stride, prefilter_x);

   if (prefilter_y > 1)
      stbtt__v_prefilter(output, out_w, out_h, out_stride, prefilter_y);

   *sub_x = stbtt__oversample_shift(prefilter_x);
   *sub_y = stbtt__oversample_shift(prefilter_y);
}*/

   // rects array must be big enough to accommodate all characters in the given ranges
   /*STBTT_DEF int stbtt_PackFontRangesRenderIntoRects(stbtt_pack_context *spc, const stbtt_fontinfo *info, stbtt_pack_range *ranges, int num_ranges, stbrp_rect *rects)
{
   int i, j, k, missing_glyph = -1, return_value = 1;

   // save current values
   int old_h_over = spc->h_oversample;
   int old_v_over = spc->v_oversample;

   k = 0;
   for (i = 0; i < num_ranges; ++i)
   {
      float fh = ranges[i].font_size;
      float scale = fh > 0 ? stbtt_ScaleForPixelHeight(info, fh) : stbtt_ScaleForMappingEmToPixels(info, -fh);
      float recip_h, recip_v, sub_x, sub_y;
      spc->h_oversample = ranges[i].h_oversample;
      spc->v_oversample = ranges[i].v_oversample;
      recip_h = 1.0f / spc->h_oversample;
      recip_v = 1.0f / spc->v_oversample;
      sub_x = stbtt__oversample_shift(spc->h_oversample);
      sub_y = stbtt__oversample_shift(spc->v_oversample);
      for (j = 0; j < ranges[i].num_chars; ++j)
      {
         stbrp_rect *r = &rects[k];
         if (r->was_packed && r->w != 0 && r->h != 0)
         {
            stbtt_packedchar *bc = &ranges[i].chardata_for_range[j];
            int advance, lsb, x0, y0, x1, y1;
            int codepoint = ranges[i].array_of_unicode_codepoints == NULL ? ranges[i].first_unicode_codepoint_in_range + j : ranges[i].array_of_unicode_codepoints[j];
            int glyph = stbtt_FindGlyphIndex(info, codepoint);
            stbrp_coord pad = (stbrp_coord)spc->padding;

            // pad on left and top
            r->x += pad;
            r->y += pad;
            r->w -= pad;
            r->h -= pad;
            stbtt_GetGlyphHMetrics(info, glyph, &advance, &lsb);
            stbtt_GetGlyphBitmapBox(info, glyph,
                                    scale * spc->h_oversample,
                                    scale * spc->v_oversample,
                                    &x0, &y0, &x1, &y1);
            stbtt_MakeGlyphBitmapSubpixel(info,
                                          spc->pixels + r->x + r->y * spc->stride_in_bytes,
                                          r->w - spc->h_oversample + 1,
                                          r->h - spc->v_oversample + 1,
                                          spc->stride_in_bytes,
                                          scale * spc->h_oversample,
                                          scale * spc->v_oversample,
                                          0, 0,
                                          glyph);

            if (spc->h_oversample > 1)
               stbtt__h_prefilter(spc->pixels + r->x + r->y * spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->h_oversample);

            if (spc->v_oversample > 1)
               stbtt__v_prefilter(spc->pixels + r->x + r->y * spc->stride_in_bytes,
                                  r->w, r->h, spc->stride_in_bytes,
                                  spc->v_oversample);

            bc->x0 = (stbtt_int16)r->x;
            bc->y0 = (stbtt_int16)r->y;
            bc->x1 = (stbtt_int16)(r->x + r->w);
            bc->y1 = (stbtt_int16)(r->y + r->h);
            bc->xadvance = scale * advance;
            bc->xoff = (float)x0 * recip_h + sub_x;
            bc->yoff = (float)y0 * recip_v + sub_y;
            bc->xoff2 = (x0 + r->w) * recip_h + sub_x;
            bc->yoff2 = (y0 + r->h) * recip_v + sub_y;

            if (glyph == 0)
               missing_glyph = j;
         }
         else if (spc->skip_missing)
         {
            return_value = 0;
         }
         else if (r->was_packed && r->w == 0 && r->h == 0 && missing_glyph >= 0)
         {
            ranges[i].chardata_for_range[j] = ranges[i].chardata_for_range[missing_glyph];
         }
         else
         {
            return_value = 0; // if any fail, report failure
         }

         ++k;
      }
   }

   // restore original values
   spc->h_oversample = old_h_over;
   spc->v_oversample = old_v_over;

   return return_value;
}
*/
   /*STBTT_DEF void stbtt_PackFontRangesPackRects(stbtt_pack_context *spc, stbrp_rect *rects, int num_rects)
   {
      stbrp_pack_rects((stbrp_context *)spc->pack_info, rects, num_rects);
   }*/
   /*
STBTT_DEF int stbtt_PackFontRanges(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, stbtt_pack_range *ranges, int num_ranges)
{
   stbtt_fontinfo info;
   int i,j,n, return_value = 1;
   //stbrp_context *context = (stbrp_context *) spc->pack_info;
   stbrp_rect    *rects;

   // flag all characters as NOT packed
   for (i=0; i < num_ranges; ++i)
      for (j=0; j < ranges[i].num_chars; ++j)
         ranges[i].chardata_for_range[j].x0 =
         ranges[i].chardata_for_range[j].y0 =
         ranges[i].chardata_for_range[j].x1 =
         ranges[i].chardata_for_range[j].y1 = 0;

   n = 0;
   for (i=0; i < num_ranges; ++i)
      n += ranges[i].num_chars;

   rects = (stbrp_rect *) STBTT_malloc(sizeof(*rects) * n, spc->user_allocator_context);
   if (rects == NULL)
      return 0;

   info.userdata = spc->user_allocator_context;
   stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata,font_index));

   n = stbtt_PackFontRangesGatherRects(spc, &info, ranges, num_ranges, rects);

   stbtt_PackFontRangesPackRects(spc, rects, n);

   return_value = stbtt_PackFontRangesRenderIntoRects(spc, &info, ranges, num_ranges, rects);

   STBTT_free(rects, spc->user_allocator_context);
   return return_value;
}

STBTT_DEF int stbtt_PackFontRange(stbtt_pack_context *spc, const unsigned char *fontdata, int font_index, float font_size,
            int first_unicode_codepoint_in_range, int num_chars_in_range, stbtt_packedchar *chardata_for_range)
{
   stbtt_pack_range range;
   range.first_unicode_codepoint_in_range = first_unicode_codepoint_in_range;
   range.array_of_unicode_codepoints = NULL;
   range.num_chars                   = num_chars_in_range;
   range.chardata_for_range          = chardata_for_range;
   range.font_size                   = font_size;
   return stbtt_PackFontRanges(spc, fontdata, font_index, &range, 1);
}

STBTT_DEF void stbtt_GetScaledFontVMetrics(const unsigned char *fontdata, int index, float size, float *ascent, float *descent, float *lineGap)
{
   int i_ascent, i_descent, i_lineGap;
   float scale;
   stbtt_fontinfo info;
   stbtt_InitFont(&info, fontdata, stbtt_GetFontOffsetForIndex(fontdata, index));
   scale = size > 0 ? stbtt_ScaleForPixelHeight(&info, size) : stbtt_ScaleForMappingEmToPixels(&info, -size);
   stbtt_GetFontVMetrics(&info, &i_ascent, &i_descent, &i_lineGap);
   *ascent  = (float) i_ascent  * scale;
   *descent = (float) i_descent * scale;
   *lineGap = (float) i_lineGap * scale;
}
*/
   /*STBTT_DEF void stbtt_GetPackedQuad(const stbtt_packedchar *chardata, int pw, int ph, int char_index, float *xpos, float *ypos, stbtt_aligned_quad *q, int align_to_integer)
   {
      float ipw = 1.0f / pw, iph = 1.0f / ph;
      const stbtt_packedchar *b = chardata + char_index;

      if (align_to_integer)
      {
         float x = (float)STBTT_ifloor((*xpos + b->xoff) + 0.5f);
         float y = (float)STBTT_ifloor((*ypos + b->yoff) + 0.5f);
         q->x0 = x;
         q->y0 = y;
         q->x1 = x + b->xoff2 - b->xoff;
         q->y1 = y + b->yoff2 - b->yoff;
      }
      else
      {
         q->x0 = *xpos + b->xoff;
         q->y0 = *ypos + b->yoff;
         q->x1 = *xpos + b->xoff2;
         q->y1 = *ypos + b->yoff2;
      }

      q->s0 = b->x0 * ipw;
      q->t0 = b->y0 * iph;
      q->s1 = b->x1 * ipw;
      q->t1 = b->y1 * iph;

      *xpos += b->xadvance;
   }*/

   //////////////////////////////////////////////////////////////////////////////
   //
   // sdf computation
   //

#define STBTT_min(a, b) ((a) < (b) ? (a) : (b))
#define STBTT_max(a, b) ((a) < (b) ? (b) : (a))

   /*static int stbtt__ray_intersect_bezier(float orig[2], float ray[2], float q0[2], float q1[2], float q2[2], float hits[2][2])
   {
      float q0perp = q0[1] * ray[0] - q0[0] * ray[1];
      float q1perp = q1[1] * ray[0] - q1[0] * ray[1];
      float q2perp = q2[1] * ray[0] - q2[0] * ray[1];
      float roperp = orig[1] * ray[0] - orig[0] * ray[1];

      float a = q0perp - 2 * q1perp + q2perp;
      float b = q1perp - q0perp;
      float c = q0perp - roperp;

      float s0 = 0., s1 = 0.;
      int num_s = 0;

      if (a != 0.0)
      {
         float discr = b * b - a * c;
         if (discr > 0.0)
         {
            float rcpna = -1 / a;
            float d = (float)STBTT_sqrt(discr);
            s0 = (b + d) * rcpna;
            s1 = (b - d) * rcpna;
            if (s0 >= 0.0 && s0 <= 1.0)
               num_s = 1;
            if (d > 0.0 && s1 >= 0.0 && s1 <= 1.0)
            {
               if (num_s == 0)
                  s0 = s1;
               ++num_s;
            }
         }
      }
      else
      {
         // 2*b*s + c = 0
         // s = -c / (2*b)
         s0 = c / (-2 * b);
         if (s0 >= 0.0 && s0 <= 1.0)
            num_s = 1;
      }

      if (num_s == 0)
         return 0;
      else
      {
         float rcp_len2 = 1 / (ray[0] * ray[0] + ray[1] * ray[1]);
         float rayn_x = ray[0] * rcp_len2, rayn_y = ray[1] * rcp_len2;

         float q0d = q0[0] * rayn_x + q0[1] * rayn_y;
         float q1d = q1[0] * rayn_x + q1[1] * rayn_y;
         float q2d = q2[0] * rayn_x + q2[1] * rayn_y;
         float rod = orig[0] * rayn_x + orig[1] * rayn_y;

         float q10d = q1d - q0d;
         float q20d = q2d - q0d;
         float q0rd = q0d - rod;

         hits[0][0] = q0rd + s0 * (2.0f - 2.0f * s0) * q10d + s0 * s0 * q20d;
         hits[0][1] = a * s0 + b;

         if (num_s > 1)
         {
            hits[1][0] = q0rd + s1 * (2.0f - 2.0f * s1) * q10d + s1 * s1 * q20d;
            hits[1][1] = a * s1 + b;
            return 2;
         }
         else
         {
            return 1;
         }
      }
   }*/

   /*static int equal(float *a, float *b)
   {
      return (a[0] == b[0] && a[1] == b[1]);
   }*/

   /*static int stbtt__compute_crossings_x(float x, float y, int nverts, stbtt_vertex *verts)
   {
      int i;
      float orig[2], ray[2] = {1, 0};
      float y_frac;
      int winding = 0;

      orig[0] = x;
      orig[1] = y;

      // make sure y never passes through a vertex of the shape
      y_frac = (float)STBTT_fmod(y, 1.0f);
      if (y_frac < 0.01f)
         y += 0.01f;
      else if (y_frac > 0.99f)
         y -= 0.01f;
      orig[1] = y;

      // test a ray from (-infinity,y) to (x,y)
      for (i = 0; i < nverts; ++i)
      {
         if (verts[i].type == STBTT_vline)
         {
            int x0 = (int)verts[i - 1].x, y0 = (int)verts[i - 1].y;
            int x1 = (int)verts[i].x, y1 = (int)verts[i].y;
            if (y > STBTT_min(y0, y1) && y < STBTT_max(y0, y1) && x > STBTT_min(x0, x1))
            {
               float x_inter = (y - y0) / (y1 - y0) * (x1 - x0) + x0;
               if (x_inter < x)
                  winding += (y0 < y1) ? 1 : -1;
            }
         }
         if (verts[i].type == STBTT_vcurve)
         {
            int x0 = (int)verts[i - 1].x, y0 = (int)verts[i - 1].y;
            int x1 = (int)verts[i].cx, y1 = (int)verts[i].cy;
            int x2 = (int)verts[i].x, y2 = (int)verts[i].y;
            int ax = STBTT_min(x0, STBTT_min(x1, x2)), ay = STBTT_min(y0, STBTT_min(y1, y2));
            int by = STBTT_max(y0, STBTT_max(y1, y2));
            if (y > ay && y < by && x > ax)
            {
               float q0[2], q1[2], q2[2];
               float hits[2][2];
               q0[0] = (float)x0;
               q0[1] = (float)y0;
               q1[0] = (float)x1;
               q1[1] = (float)y1;
               q2[0] = (float)x2;
               q2[1] = (float)y2;
               if (equal(q0, q1) || equal(q1, q2))
               {
                  x0 = (int)verts[i - 1].x;
                  y0 = (int)verts[i - 1].y;
                  x1 = (int)verts[i].x;
                  y1 = (int)verts[i].y;
                  if (y > STBTT_min(y0, y1) && y < STBTT_max(y0, y1) && x > STBTT_min(x0, x1))
                  {
                     float x_inter = (y - y0) / (y1 - y0) * (x1 - x0) + x0;
                     if (x_inter < x)
                        winding += (y0 < y1) ? 1 : -1;
                  }
               }
               else
               {
                  int num_hits = stbtt__ray_intersect_bezier(orig, ray, q0, q1, q2, hits);
                  if (num_hits >= 1)
                     if (hits[0][0] < 0)
                        winding += (hits[0][1] < 0 ? -1 : 1);
                  if (num_hits >= 2)
                     if (hits[1][0] < 0)
                        winding += (hits[1][1] < 0 ? -1 : 1);
               }
            }
         }
      }
      return winding;
   }*/

   /*static float stbtt__cuberoot(float x)
   {
      if (x < 0)
         return -(float)STBTT_pow(-x, 1.0f / 3.0f);
      else
         return (float)STBTT_pow(x, 1.0f / 3.0f);
   }*/

   // x^3 + c*x^2 + b*x + a = 0
   /*static int stbtt__solve_cubic(float a, float b, float c, float *r)
   {
      float s = -a / 3;
      float p = b - a * a / 3;
      float q = a * (2 * a * a - 9 * b) / 27 + c;
      float p3 = p * p * p;
      float d = q * q + 4 * p3 / 27;
      if (d >= 0)
      {
         float z = (float)STBTT_sqrt(d);
         float u = (-q + z) / 2;
         float v = (-q - z) / 2;
         u = stbtt__cuberoot(u);
         v = stbtt__cuberoot(v);
         r[0] = s + u + v;
         return 1;
      }
      else
      {
         float u = (float)STBTT_sqrt(-p / 3);
         float v = (float)STBTT_acos(-STBTT_sqrt(-27 / p3) * q / 2) / 3; // p3 must be negative, since d is negative
         float m = (float)STBTT_cos(v);
         float n = (float)STBTT_cos(v - 3.141592 / 2) * 1.732050808f;
         r[0] = s + u * 2 * m;
         r[1] = s - u * (m + n);
         r[2] = s - u * (m - n);

         //STBTT_assert( STBTT_fabs(((r[0]+a)*r[0]+b)*r[0]+c) < 0.05f);  // these asserts may not be safe at all scales, though they're in bezier t parameter units so maybe?
         //STBTT_assert( STBTT_fabs(((r[1]+a)*r[1]+b)*r[1]+c) < 0.05f);
         //STBTT_assert( STBTT_fabs(((r[2]+a)*r[2]+b)*r[2]+c) < 0.05f);
         return 3;
      }
   }*/

   /*STBTT_DEF unsigned char *stbtt_GetGlyphSDF(const stbtt_fontinfo *info, float scale, int glyph, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
   {
      float scale_x = scale, scale_y = scale;
      int ix0, iy0, ix1, iy1;
      int w, h;
      unsigned char *data;

      if (scale == 0)
         return NULL;

      stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale, scale, 0.0f, 0.0f, &ix0, &iy0, &ix1, &iy1);

      // if empty, return NULL
      if (ix0 == ix1 || iy0 == iy1)
         return NULL;

      ix0 -= padding;
      iy0 -= padding;
      ix1 += padding;
      iy1 += padding;

      w = (ix1 - ix0);
      h = (iy1 - iy0);

      if (width)
         *width = w;
      if (height)
         *height = h;
      if (xoff)
         *xoff = ix0;
      if (yoff)
         *yoff = iy0;

      // invert for y-downwards bitmaps
      scale_y = -scale_y;

      {
         int x, y, i, j;
         float *precompute;
         stbtt_vertex *verts;
         int num_verts = stbtt_GetGlyphShape(info, glyph, &verts);
         data = (unsigned char *)STBTT_malloc(w * h, info->userdata);
         precompute = (float *)STBTT_malloc(num_verts * sizeof(float), info->userdata);

         for (i = 0, j = num_verts - 1; i < num_verts; j = i++)
         {
            if (verts[i].type == STBTT_vline)
            {
               float x0 = verts[i].x * scale_x, y0 = verts[i].y * scale_y;
               float x1 = verts[j].x * scale_x, y1 = verts[j].y * scale_y;
               float dist = (float)STBTT_sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
               precompute[i] = (dist == 0) ? 0.0f : 1.0f / dist;
            }
            else if (verts[i].type == STBTT_vcurve)
            {
               float x2 = verts[j].x * scale_x, y2 = verts[j].y * scale_y;
               float x1 = verts[i].cx * scale_x, y1 = verts[i].cy * scale_y;
               float x0 = verts[i].x * scale_x, y0 = verts[i].y * scale_y;
               float bx = x0 - 2 * x1 + x2, by = y0 - 2 * y1 + y2;
               float len2 = bx * bx + by * by;
               if (len2 != 0.0f)
                  precompute[i] = 1.0f / (bx * bx + by * by);
               else
                  precompute[i] = 0.0f;
            }
            else
               precompute[i] = 0.0f;
         }

         for (y = iy0; y < iy1; ++y)
         {
            for (x = ix0; x < ix1; ++x)
            {
               float val;
               float min_dist = 999999.0f;
               float sx = (float)x + 0.5f;
               float sy = (float)y + 0.5f;
               float x_gspace = (sx / scale_x);
               float y_gspace = (sy / scale_y);

               int winding = stbtt__compute_crossings_x(x_gspace, y_gspace, num_verts, verts); // @OPTIMIZE: this could just be a rasterization, but needs to be line vs. non-tesselated curves so a new path

               for (i = 0; i < num_verts; ++i)
               {
                  float x0 = verts[i].x * scale_x, y0 = verts[i].y * scale_y;

                  // check against every point here rather than inside line/curve primitives -- @TODO: wrong if multiple 'moves' in a row produce a garbage point, and given culling, probably more efficient to do within line/curve
                  float dist2 = (x0 - sx) * (x0 - sx) + (y0 - sy) * (y0 - sy);
                  if (dist2 < min_dist * min_dist)
                     min_dist = (float)STBTT_sqrt(dist2);

                  if (verts[i].type == STBTT_vline)
                  {
                     float x1 = verts[i - 1].x * scale_x, y1 = verts[i - 1].y * scale_y;

                     // coarse culling against bbox
                     //if (sx > STBTT_min(x0,x1)-min_dist && sx < STBTT_max(x0,x1)+min_dist &&
                     //    sy > STBTT_min(y0,y1)-min_dist && sy < STBTT_max(y0,y1)+min_dist)
                     float dist = (float)STBTT_fabs((x1 - x0) * (y0 - sy) - (y1 - y0) * (x0 - sx)) * precompute[i];
                     STBTT_assert(i != 0);
                     if (dist < min_dist)
                     {
                        // check position along line
                        // x' = x0 + t*(x1-x0), y' = y0 + t*(y1-y0)
                        // minimize (x'-sx)*(x'-sx)+(y'-sy)*(y'-sy)
                        float dx = x1 - x0, dy = y1 - y0;
                        float px = x0 - sx, py = y0 - sy;
                        // minimize (px+t*dx)^2 + (py+t*dy)^2 = px*px + 2*px*dx*t + t^2*dx*dx + py*py + 2*py*dy*t + t^2*dy*dy
                        // derivative: 2*px*dx + 2*py*dy + (2*dx*dx+2*dy*dy)*t, set to 0 and solve
                        float t = -(px * dx + py * dy) / (dx * dx + dy * dy);
                        if (t >= 0.0f && t <= 1.0f)
                           min_dist = dist;
                     }
                  }
                  else if (verts[i].type == STBTT_vcurve)
                  {
                     float x2 = verts[i - 1].x * scale_x, y2 = verts[i - 1].y * scale_y;
                     float x1 = verts[i].cx * scale_x, y1 = verts[i].cy * scale_y;
                     float box_x0 = STBTT_min(STBTT_min(x0, x1), x2);
                     float box_y0 = STBTT_min(STBTT_min(y0, y1), y2);
                     float box_x1 = STBTT_max(STBTT_max(x0, x1), x2);
                     float box_y1 = STBTT_max(STBTT_max(y0, y1), y2);
                     // coarse culling against bbox to avoid computing cubic unnecessarily
                     if (sx > box_x0 - min_dist && sx < box_x1 + min_dist && sy > box_y0 - min_dist && sy < box_y1 + min_dist)
                     {
                        int num = 0;
                        float ax = x1 - x0, ay = y1 - y0;
                        float bx = x0 - 2 * x1 + x2, by = y0 - 2 * y1 + y2;
                        float mx = x0 - sx, my = y0 - sy;
                        float res[3], px, py, t, it;
                        float a_inv = precompute[i];
                        if (a_inv == 0.0)
                        { // if a_inv is 0, it's 2nd degree so use quadratic formula
                           float a = 3 * (ax * bx + ay * by);
                           float b = 2 * (ax * ax + ay * ay) + (mx * bx + my * by);
                           float c = mx * ax + my * ay;
                           if (a == 0.0)
                           { // if a is 0, it's linear
                              if (b != 0.0)
                              {
                                 res[num++] = -c / b;
                              }
                           }
                           else
                           {
                              float discriminant = b * b - 4 * a * c;
                              if (discriminant < 0)
                                 num = 0;
                              else
                              {
                                 float root = (float)STBTT_sqrt(discriminant);
                                 res[0] = (-b - root) / (2 * a);
                                 res[1] = (-b + root) / (2 * a);
                                 num = 2; // don't bother distinguishing 1-solution case, as code below will still work
                              }
                           }
                        }
                        else
                        {
                           float b = 3 * (ax * bx + ay * by) * a_inv; // could precompute this as it doesn't depend on sample point
                           float c = (2 * (ax * ax + ay * ay) + (mx * bx + my * by)) * a_inv;
                           float d = (mx * ax + my * ay) * a_inv;
                           num = stbtt__solve_cubic(b, c, d, res);
                        }
                        if (num >= 1 && res[0] >= 0.0f && res[0] <= 1.0f)
                        {
                           t = res[0], it = 1.0f - t;
                           px = it * it * x0 + 2 * t * it * x1 + t * t * x2;
                           py = it * it * y0 + 2 * t * it * y1 + t * t * y2;
                           dist2 = (px - sx) * (px - sx) + (py - sy) * (py - sy);
                           if (dist2 < min_dist * min_dist)
                              min_dist = (float)STBTT_sqrt(dist2);
                        }
                        if (num >= 2 && res[1] >= 0.0f && res[1] <= 1.0f)
                        {
                           t = res[1], it = 1.0f - t;
                           px = it * it * x0 + 2 * t * it * x1 + t * t * x2;
                           py = it * it * y0 + 2 * t * it * y1 + t * t * y2;
                           dist2 = (px - sx) * (px - sx) + (py - sy) * (py - sy);
                           if (dist2 < min_dist * min_dist)
                              min_dist = (float)STBTT_sqrt(dist2);
                        }
                        if (num >= 3 && res[2] >= 0.0f && res[2] <= 1.0f)
                        {
                           t = res[2], it = 1.0f - t;
                           px = it * it * x0 + 2 * t * it * x1 + t * t * x2;
                           py = it * it * y0 + 2 * t * it * y1 + t * t * y2;
                           dist2 = (px - sx) * (px - sx) + (py - sy) * (py - sy);
                           if (dist2 < min_dist * min_dist)
                              min_dist = (float)STBTT_sqrt(dist2);
                        }
                     }
                  }
               }
               if (winding == 0)
                  min_dist = -min_dist; // if outside the shape, value is negative
               val = onedge_value + pixel_dist_scale * min_dist;
               if (val < 0)
                  val = 0;
               else if (val > 255)
                  val = 255;
               data[(y - iy0) * w + (x - ix0)] = (unsigned char)val;
            }
         }
         STBTT_free(precompute, info->userdata);
         STBTT_free(verts, info->userdata);
      }
      return data;
   }*/

   /*STBTT_DEF unsigned char *stbtt_GetCodepointSDF(const stbtt_fontinfo *info, float scale, int codepoint, int padding, unsigned char onedge_value, float pixel_dist_scale, int *width, int *height, int *xoff, int *yoff)
   {
      return stbtt_GetGlyphSDF(info, scale, stbtt_FindGlyphIndex(info, codepoint), padding, onedge_value, pixel_dist_scale, width, height, xoff, yoff);
   }*/

   /*STBTT_DEF void stbtt_FreeSDF(unsigned char *bitmap, void *userdata)
   {
      STBTT_free(bitmap, userdata);
   }*/

   //////////////////////////////////////////////////////////////////////////////
   //
   // font name matching -- recommended not to use this
   //

   // check if a utf8 string contains a prefix which is the utf16 string; if so return length of matching utf8 string
   /*static stbtt_int32 stbtt__CompareUTF8toUTF16_bigendian_prefix(stbtt_uint8 *s1, stbtt_int32 len1, stbtt_uint8 *s2, stbtt_int32 len2)
   {
      stbtt_int32 i = 0;

      // convert utf16 to utf8 and compare the results while converting
      while (len2)
      {
         stbtt_uint16 ch = s2[0] * 256 + s2[1];
         if (ch < 0x80)
         {
            if (i >= len1)
               return -1;
            if (s1[i++] != ch)
               return -1;
         }
         else if (ch < 0x800)
         {
            if (i + 1 >= len1)
               return -1;
            if (s1[i++] != 0xc0 + (ch >> 6))
               return -1;
            if (s1[i++] != 0x80 + (ch & 0x3f))
               return -1;
         }
         else if (ch >= 0xd800 && ch < 0xdc00)
         {
            stbtt_uint32 c;
            stbtt_uint16 ch2 = s2[2] * 256 + s2[3];
            if (i + 3 >= len1)
               return -1;
            c = ((ch - 0xd800) << 10) + (ch2 - 0xdc00) + 0x10000;
            if (s1[i++] != 0xf0 + (c >> 18))
               return -1;
            if (s1[i++] != 0x80 + ((c >> 12) & 0x3f))
               return -1;
            if (s1[i++] != 0x80 + ((c >> 6) & 0x3f))
               return -1;
            if (s1[i++] != 0x80 + ((c)&0x3f))
               return -1;
            s2 += 2; // plus another 2 below
            len2 -= 2;
         }
         else if (ch >= 0xdc00 && ch < 0xe000)
         {
            return -1;
         }
         else
         {
            if (i + 2 >= len1)
               return -1;
            if (s1[i++] != 0xe0 + (ch >> 12))
               return -1;
            if (s1[i++] != 0x80 + ((ch >> 6) & 0x3f))
               return -1;
            if (s1[i++] != 0x80 + ((ch)&0x3f))
               return -1;
         }
         s2 += 2;
         len2 -= 2;
      }
      return i;
   }*/

   /*static int stbtt_CompareUTF8toUTF16_bigendian_internal(char *s1, int len1, char *s2, int len2)
   {
      return len1 == stbtt__CompareUTF8toUTF16_bigendian_prefix((stbtt_uint8 *)s1, len1, (stbtt_uint8 *)s2, len2);
   }*/

   // returns results in whatever encoding you request... but note that 2-byte encodings
   // will be BIG-ENDIAN... use stbtt_CompareUTF8toUTF16_bigendian() to compare
   /*STBTT_DEF size_t stbtt_GetFontNameString(const stbtt_fontinfo *info, int platformID, int encodingID, int languageID, int nameID, char *result, size_t result_len)
   {
      if (result_len == 0)
         return 0;
      size_t length;
      stbtt_int32 i, count, stringOffset;
      //stbtt_uint8 *fc = font->data;
      stbtt_uint32 nm = stbtt__find_table(info->stream, info->fontstart, "name");
      if (!nm)
         return 0;
      info->stream->seek(nm + 2);
      count = read_uint16(info->stream);
      stringOffset = nm + read_uint16(info->stream);
      for (i = 0; i < count; ++i)
      {
         //stbtt_uint32 loc = nm + 6 + 12 * i;
         //info->stream->seek(loc);
         if (platformID == read_uint16(info->stream) && encodingID == read_uint16(info->stream) && languageID == read_uint16(info->stream) && nameID == read_uint16(info->stream))
         {
            length = read_uint16(info->stream);
            info->stream->seek(stringOffset + read_uint16(info->stream));
            result[result_len - 1] = 0;
            return info->stream->read((uint8_t *)result, result_len > length ? length : result_len - 1);
         }
      }
      return 0;
   }*/

   /*static int stbtt__matchpair(stbtt_uint8 *fc, stbtt_uint32 nm, stbtt_uint8 *name, stbtt_int32 nlen, stbtt_int32 target_id, stbtt_int32 next_id)
   {
      stbtt_int32 i;
      stbtt_int32 count = ttUSHORT(fc + nm + 2);
      stbtt_int32 stringOffset = nm + ttUSHORT(fc + nm + 4);

      for (i = 0; i < count; ++i)
      {
         stbtt_uint32 loc = nm + 6 + 12 * i;
         stbtt_int32 id = ttUSHORT(fc + loc + 6);
         if (id == target_id)
         {
            // find the encoding
            stbtt_int32 platform = ttUSHORT(fc + loc + 0), encoding = ttUSHORT(fc + loc + 2), language = ttUSHORT(fc + loc + 4);

            // is this a Unicode encoding?
            if (platform == 0 || (platform == 3 && encoding == 1) || (platform == 3 && encoding == 10))
            {
               stbtt_int32 slen = ttUSHORT(fc + loc + 8);
               stbtt_int32 off = ttUSHORT(fc + loc + 10);

               // check if there's a prefix match
               stbtt_int32 matchlen = stbtt__CompareUTF8toUTF16_bigendian_prefix(name, nlen, fc + stringOffset + off, slen);
               if (matchlen >= 0)
               {
                  // check for target_id+1 immediately following, with same encoding & language
                  if (i + 1 < count && ttUSHORT(fc + loc + 12 + 6) == next_id && ttUSHORT(fc + loc + 12) == platform && ttUSHORT(fc + loc + 12 + 2) == encoding && ttUSHORT(fc + loc + 12 + 4) == language)
                  {
                     slen = ttUSHORT(fc + loc + 12 + 8);
                     off = ttUSHORT(fc + loc + 12 + 10);
                     if (slen == 0)
                     {
                        if (matchlen == nlen)
                           return 1;
                     }
                     else if (matchlen < nlen && name[matchlen] == ' ')
                     {
                        ++matchlen;
                        if (stbtt_CompareUTF8toUTF16_bigendian_internal((char *)(name + matchlen), nlen - matchlen, (char *)(fc + stringOffset + off), slen))
                           return 1;
                     }
                  }
                  else
                  {
                     // if nothing immediately following
                     if (matchlen == nlen)
                        return 1;
                  }
               }
            }

            // @TODO handle other encodings
         }
      }
      return 0;
   }*/
/*
static int stbtt__matches(stbtt_uint8 *fc, stbtt_uint32 offset, stbtt_uint8 *name, stbtt_int32 flags)
{
   stbtt_int32 nlen = (stbtt_int32) STBTT_strlen((char *) name);
   stbtt_uint32 nm,hd;
   if (!stbtt__isfont(fc+offset)) return 0;

   // check italics/bold/underline flags in macStyle...
   if (flags) {
      hd = stbtt__find_table(fc, offset, "head");
      if ((ttUSHORT(fc+hd+44) & 7) != (flags & 7)) return 0;
   }

   nm = stbtt__find_table(fc, offset, "name");
   if (!nm) return 0;

   if (flags) {
      // if we checked the macStyle flags, then just check the family and ignore the subfamily
      if (stbtt__matchpair(fc, nm, name, nlen, 16, -1))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  1, -1))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   } else {
      if (stbtt__matchpair(fc, nm, name, nlen, 16, 17))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  1,  2))  return 1;
      if (stbtt__matchpair(fc, nm, name, nlen,  3, -1))  return 1;
   }

   return 0;
}

static int stbtt_FindMatchingFont_internal(unsigned char *font_collection, char *name_utf8, stbtt_int32 flags)
{
   stbtt_int32 i;
   for (i=0;;++i) {
      stbtt_int32 off = stbtt_GetFontOffsetForIndex(font_collection, i);
      if (off < 0) return off;
      if (stbtt__matches((stbtt_uint8 *) font_collection, off, (stbtt_uint8*) name_utf8, flags))
         return off;
   }
}
*/
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

   /*STBTT_DEF int stbtt_BakeFontBitmap(const unsigned char *data, int offset,
                                float pixel_height, unsigned char *pixels, int pw, int ph,
                                int first_char, int num_chars, stbtt_bakedchar *chardata)
{
   return stbtt_BakeFontBitmap_internal((unsigned char *) data, offset, pixel_height, pixels, pw, ph, first_char, num_chars, chardata);
}*/

   /*STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data, int index)
   {
      return stbtt_GetFontOffsetForIndex_internal((unsigned char *)data, index);
   }*/

   /*STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data)
   {
      return stbtt_GetNumberOfFonts_internal((unsigned char *)data);
   }*/

   STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, io::stream *stream, int offset)
   {
      return stbtt_InitFont_internal(info, stream, offset);
   }

   /*STBTT_DEF int stbtt_FindMatchingFont(const unsigned char *fontdata, const char *name, int flags)
{
   return stbtt_FindMatchingFont_internal((unsigned char *) fontdata, (char *) name, flags);
}*/

   /*STBTT_DEF int stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int len1, const char *s2, int len2)
   {
      return stbtt_CompareUTF8toUTF16_bigendian_internal((char *)s1, len1, (char *)s2, len2);
   }*/
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
int open_font_hash(const int& value) {
   return value;
}
namespace gfx {
      void open_font_cache::deinitialize() {
         if(m_cache!=nullptr) {
            m_cache->clear();
            m_cache->~cache_type();
            m_deallocator(m_cache);
            m_cache = nullptr;
         }
      }
      bool open_font_cache::initialize() {
         if(m_cache == nullptr) {
            m_cache = (cache_type*)m_allocator(sizeof(cache_type));
            if(m_cache == nullptr) {
               return false;
            }
            *m_cache = cache_type(open_font_hash, m_allocator,m_reallocator,m_deallocator);
         }
         return true;
      }
      open_font_cache::open_font_cache(void*(allocator)(size_t),void*(reallocator)(void*,size_t),void(deallocator)(void*)) : m_allocator(allocator),m_reallocator(reallocator),m_deallocator(deallocator),m_cache(nullptr) {

      }
      open_font_cache::~open_font_cache() {
         deinitialize();
      }
      void open_font_cache::clear() {
         deinitialize();
      }
      
    open_font::open_font() {
        m_info_data = nullptr;
    }
    open_font::open_font(stream* stream, void*(allocator)(size_t), void(deallocator)(void*)) :
      m_allocator(allocator),m_deallocator(deallocator) {
      m_info_data = m_allocator(sizeof(stbtt::stbtt_fontinfo));
      if(m_info_data==nullptr) {
         return;
      }
      ((stbtt::stbtt_fontinfo*)m_info_data)->stream = nullptr;
      if(nullptr==stream) {
         return;
      }
      stbtt::stbtt_InitFont(((stbtt::stbtt_fontinfo*)m_info_data),stream,0);
    }
    open_font::open_font(open_font&& rhs) : m_allocator(rhs.m_allocator),m_deallocator(rhs.m_deallocator), m_info_data(rhs.m_info_data) {
        rhs.m_info_data = nullptr;
    }
    open_font& open_font::operator=(open_font&& rhs) {
         free();
         m_allocator = rhs.m_allocator;
         m_deallocator = rhs.m_deallocator;   
         m_info_data = rhs.m_info_data;
         rhs.m_info_data = nullptr;
         return *this;
    }
    open_font::~open_font() {
        free();
    }
    void open_font::draw(int(*render_cb)(int x,int y,int c,void* state),void* state, int width,int height,int out_stride,float scale_x,float scale_y,float shift_x, float shift_y,int glyph) const {
       stbtt::stbtt_MakeGlyphBitmapSubpixel((const stbtt::stbtt_fontinfo*)m_info_data,
       render_cb,state,width,height,out_stride,scale_x,scale_y,shift_x,shift_y,glyph);
    }
    void open_font::glyph_bitmap_bounding_box(int glyph_index,float scale_x,float scale_y,float shift_x,float shift_y,int* x1, int* y1, int* x2, int* y2) const {
      stbtt::stbtt_GetGlyphBitmapBoxSubpixel((const stbtt::stbtt_fontinfo*)m_info_data,glyph_index,scale_x,scale_y,shift_x,shift_y,x1,y1,x2,y2);
    }
    void open_font::font_vmetrics(int* ascent, int* descent, int* line_gap) const {
       stbtt::stbtt_GetFontVMetrics((const stbtt::stbtt_fontinfo*)m_info_data,ascent,descent,line_gap);
       
    }
    void open_font::glyph_hmetrics(int glyph_index, int* advance_width, int* left_side_bearing) const {
      stbtt::stbtt_GetGlyphHMetrics((const stbtt::stbtt_fontinfo*)m_info_data,glyph_index,advance_width,left_side_bearing);
    }
    void open_font::bounding_box(int* x1, int* y1, int* x2, int* y2) const {
       return stbtt::stbtt_GetFontBoundingBox((const stbtt::stbtt_fontinfo*)m_info_data,x1,y1,x2,y2);
    }
    // caches a string for faster drawing
    void open_font::cache(open_font_cache* cache,const char* text,gfx_encoding encoding) {
       if(nullptr==cache||nullptr==text) {
          return;
       }
       if(!cache->initialize()) {
          return;
       }
       const char* sz = text;
       while(*sz) {
         int cp;
         int c = to_utf32_codepoint(sz,4,&cp,encoding);
         if(c<0) {
            return;
         }
         const int* pi = cache->m_cache->find(cp);
         if(nullptr==pi) {
            int result = stbtt::stbtt_FindGlyphIndex((const stbtt::stbtt_fontinfo*)m_info_data,cp);
            cache->m_cache->insert({cp,result});
         }
       }
    }
    int open_font::glyph_index(const char* sz, size_t* out_advance, gfx_encoding encoding,open_font_cache* cache) const {
       int cp;
       int c = to_utf32_codepoint(sz,4,&cp,encoding);
       if(c<0) {
          if(out_advance) *out_advance = 0;
         return -1;
       }
       if(out_advance) *out_advance = c;
       int result;
       if(cache) {
          if(cache->initialize()) {
             const int* pi = cache->m_cache->find(cp);
             if(pi!=nullptr) {
                return *pi;
             }
             result = stbtt::stbtt_FindGlyphIndex((const stbtt::stbtt_fontinfo*)m_info_data,cp);
             cache->m_cache->insert({cp,result});
          }
       }
       result = stbtt::stbtt_FindGlyphIndex((const stbtt::stbtt_fontinfo*)m_info_data,cp);
       return result;
    }
    void open_font::free() {
       if(m_deallocator != nullptr) {
          if(m_info_data != nullptr) {
             m_deallocator(m_info_data);
             m_info_data = nullptr;
          }
       }
    }
    uint16_t open_font::ascent() const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return 0;
        int result = 0;
        stbtt::stbtt_GetFontVMetrics((const stbtt::stbtt_fontinfo*)m_info_data,&result,nullptr,nullptr);
        return (uint16_t)result;
    }
    uint16_t open_font::descent() const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return 0;
        int result = 0;
        stbtt::stbtt_GetFontVMetrics((const stbtt::stbtt_fontinfo*)m_info_data,nullptr,&result,nullptr);
        return (uint16_t)result;
    }
    uint16_t open_font::line_gap() const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return 0;
        int result = 0;
        stbtt::stbtt_GetFontVMetrics((stbtt::stbtt_fontinfo*)m_info_data,nullptr,nullptr,&result);
        return (uint16_t)result;
    }
    float open_font::scale(float pixel_height) const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return NAN;
        return stbtt::stbtt_ScaleForPixelHeight((const stbtt::stbtt_fontinfo*)m_info_data,pixel_height);
    }
    uint16_t open_font::advance_width(int codepoint) const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return 0;
        int result;
        stbtt::stbtt_GetCodepointHMetrics((const stbtt::stbtt_fontinfo*)m_info_data,codepoint,&result,nullptr);
        return (uint16_t)result;
    }
    inline float open_font::advance_width(int codepoint,float scale) const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return NAN;
        return advance_width(codepoint)*scale+.5;
    }
    uint16_t open_font::left_bearing(int codepoint) const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return 0;
        int result;
        stbtt::stbtt_GetCodepointHMetrics((const stbtt::stbtt_fontinfo*)m_info_data,codepoint,nullptr,&result);
        return (uint16_t)result;
    }
    inline float open_font::left_bearing(int codepoint,float scale) const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return NAN;
        return left_bearing(codepoint)*scale+.5;
    }
    uint16_t open_font::kern_advance_width(int codepoint1,int codepoint2) const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return 0;
        return stbtt::stbtt_GetCodepointKernAdvance((const stbtt::stbtt_fontinfo*)m_info_data,codepoint1,codepoint2);
    }
    inline float open_font::kern_advance_width(int codepoint1,int codepoint2,float scale) const {
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return NAN;
        return kern_advance_width(codepoint1,codepoint2)*scale+.5;
    }
    rect16 open_font::bounds(int codepoint,float scale,float x_shift,float y_shift) const {
        rect16 result;
        if(nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream) return {0,0,0,0};
        int x1,y1,x2,y2;
        stbtt::stbtt_GetCodepointBitmapBoxSubpixel((const stbtt::stbtt_fontinfo*)m_info_data,codepoint,scale,scale,x_shift,y_shift,&x1,&y1,&x2,&y2);
        return {(uint16_t)x1,(uint16_t)y1,(uint16_t)x2,(uint16_t)y2};
    }
    // measures the size of the text within the destination area
    ssize16 open_font::measure_text(
        ssize16 dest_size,
        spoint16 offset,
        const char* text,
        float scale,
        float scaled_tab_width,gfx::gfx_encoding encoding,
        open_font_cache* cache) const {
        ssize16 result(0,0);
        if(nullptr==text || 0==*text || nullptr==m_info_data || nullptr==((stbtt::stbtt_fontinfo*)m_info_data)->stream)
            return result;
        int asc,dsc,lgap;
        float height;
        const stbtt::stbtt_fontinfo* info = (const stbtt::stbtt_fontinfo*)m_info_data;
        stbtt::stbtt_GetFontVMetrics(info,&asc,&dsc,&lgap);
        int x1,y1,x2,y2;
        stbtt::stbtt_GetFontBoundingBox(info,&x1,&y1,&x2,&y2);
        if(scaled_tab_width<=0.0) {
            scaled_tab_width = (x2-x1+1)*scale*4;
        }
        height = asc*scale;
        int advw,lsb,gi;
        float xpos=offset.x,ypos=offset.y;
        float x_extent=0,y_extent=0;
        bool adv_line = false;
        const char*sz=text;
        while(*sz) {
            if(*sz<32) {
                int ti;
                switch(*sz) {
                    case '\t':
                        ti=xpos/scaled_tab_width;
                        xpos=(ti+1)*scaled_tab_width;
                        if(xpos>=dest_size.width) {
                            adv_line=true;
                        }
                        break;
                    case '\r':
                        xpos = offset.x;
                        break;
                    case '\n':
                        adv_line=true;
                        break;
                }
                if(adv_line) {
                    ypos+=height;
                    xpos = offset.x;
                    if(height+ypos>=dest_size.height) {
                        return {(int16_t)(x_extent+1),(int16_t)(ypos+1)};
                    }                        
                    if(ypos+height>y_extent) {
                        y_extent=ypos+height;
                    }
                    ypos+=lgap*scale;
                }
                ++sz;
                continue;
            }
            size_t adv;
            if(cache!=nullptr) {
               
            }
            gi=glyph_index(sz,&adv,encoding,cache);
            stbtt::stbtt_GetGlyphHMetrics(info,gi,&advw,&lsb);
            stbtt::stbtt_GetGlyphBitmapBoxSubpixel(info,gi,scale,scale,xpos-floor(xpos),0,&x1,&y1,&x2,&y2);
            float xe = x2-x1;
            adv_line = false;
            if(xe+xpos>=dest_size.width) {
                ypos+=height;
                xpos=offset.x;
                adv_line = true;
            } 
            if(adv_line && height+ypos>=dest_size.height) {
                return {(int16_t)(x_extent+1),(int16_t)(ypos+1)};
            }
            if(xpos+xe>x_extent) {
                x_extent=xpos+xe;
            }
            if(ypos+height>y_extent+(lgap*scale)) {
                y_extent=ypos+height;
            }
            xpos+=(advw*scale);    
            if(*(sz+adv)) {
               size_t adv2;
               int gi2=glyph_index(sz+adv,&adv2,encoding,cache);
               xpos+=(stbtt::stbtt_GetGlyphKernAdvance(info,gi,gi2)*scale);
            }
            if(adv_line) {
                ypos+=lgap*scale;
            }
            sz+=adv;
        }
        return {(int16_t)(ceil(x_extent)),(int16_t)(ceil(y_extent+abs(dsc*scale)))};
    }
    // opens a font stream. Note that the stream must remain open as long as the font is being used. The stream must be readable and seekable. This class does not close the stream
    // This always chooses the first font out of a font collection.
    gfx_result open_font::open(stream* stream,open_font* out_result, void*(*allocator)(size_t), void(*deallocator)(void*)) {
         if(nullptr==stream || 
            nullptr==out_result ||
            !stream->caps().read ||
            !stream->caps().seek) {
               return gfx_result::invalid_argument;
         }
         out_result->m_allocator = allocator;
         out_result->m_deallocator = deallocator;
         out_result->m_info_data = allocator(sizeof(stbtt::stbtt_fontinfo));
         if(out_result->m_info_data==nullptr) {
            return gfx_result::out_of_memory;
         }
         if(!stbtt::stbtt_InitFont((stbtt::stbtt_fontinfo*)out_result->m_info_data,stream,0)) {
               return gfx_result::invalid_format;
         }    
         return gfx_result::success;
    }
    // from libxml http://xmlsoft.org/
   int open_font::latin1_to_utf8(unsigned char* out, size_t outlen, const unsigned char* in, size_t inlen)
   {
      unsigned char* outstart= out;
      unsigned char* outend= out+outlen;
      const unsigned char* inend= in+inlen;
      unsigned char c;

      while (in < inend) {
         c= *in++;
         if (c < 0x80) {
               if (out >= outend)  return -1;
               *out++ = c;
               return 1;
         }
         else {
               if (out >= outend)  return -1;
               *out++ = 0xC0 | (c >> 6);
               if (out >= outend)  return -1;
               *out++ = 0x80 | (0x3F & c);
               return 2;
         }
      }
      return out-outstart;
   }
   // from libxml http://xmlsoft.org/
   int open_font::utf8_to_utf16(uint16_t* out, size_t outlen, const unsigned char* in, size_t inlen)
   {
      uint16_t* outstart= out;
      uint16_t* outend= out+outlen;
      const unsigned char* inend= in+inlen;
      unsigned int c, d, trailing;

      while (in < inend) {
         d= *in++;
         if      (d < 0x80)  { c= d; trailing= 0; }
         else if (d < 0xC0)  return -2;    /* trailing byte in leading position */
         else if (d < 0xE0)  { c= d & 0x1F; trailing= 1; }
         else if (d < 0xF0)  { c= d & 0x0F; trailing= 2; }
         else if (d < 0xF8)  { c= d & 0x07; trailing= 3; }
         else return -2;    /* no chance for this in UTF-16 */

         for ( ; trailing; trailing--) {
            if ((in >= inend) || (((d= *in++) & 0xC0) != 0x80))  return -1;
            c <<= 6;
            c |= d & 0x3F;
         }

         /* assertion: c is a single UTF-4 value */
         if (c < 0x10000) {
               if (out >= outend)  return -1;
               *out++ = c;
               return 1;
         }
         else if (c < 0x110000) {
               if (out+1 >= outend)  return -1;
               c -= 0x10000;
               *out++ = 0xD800 | (c >> 10);
               *out++ = 0xDC00 | (c & 0x03FF);
               return 2;
         }
         else  return -1;
      }
      return out-outstart;
   }
   int open_font::to_utf32_codepoint(const char* in,size_t in_length, int* codepoint, gfx_encoding encoding) {
      int c;
      uint16_t out_tmp[4];
      switch(encoding) {
         case gfx_encoding::utf8: {
            c = utf8_to_utf16(out_tmp,4,(const unsigned char*)in,in_length);
         }
         break;
         case gfx_encoding::latin1: {
            unsigned char out_tmp1[4];
            c = latin1_to_utf8(out_tmp1,4,(const unsigned char*)in,in_length);
            if(c<0) {
               *codepoint = 0;
               return c;
            }
            c=utf8_to_utf16(out_tmp,4,out_tmp1,4);
         }
         break;
         default:
            c=0;
            break;
      }
      switch(c) {
         case 1:
            *codepoint = out_tmp[0];
            break;
         case 2:
            *codepoint = (out_tmp[0] << 10) + out_tmp[1] - 0x35fdc00;
            break;
         default:
            *codepoint = 0;
            break;
      }
      return c;
   }

}