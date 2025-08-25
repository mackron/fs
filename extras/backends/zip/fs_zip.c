#ifndef fs_zip_c
#define fs_zip_c

#include "../../../fs.h"
#include "fs_zip.h"

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void fs_zip_zero_memory_default(void* p, size_t sz)
{
    if (sz > 0) {
        memset(p, 0, sz);
    }
}

#ifndef FS_ZIP_ZERO_MEMORY
#define FS_ZIP_ZERO_MEMORY(p, sz) fs_zip_zero_memory_default((p), (sz))
#endif

#ifndef FS_ZIP_COPY_MEMORY
#define FS_ZIP_COPY_MEMORY(dst, src, sz) memcpy((dst), (src), (sz))
#endif

#ifndef FS_ZIP_MOVE_MEMORY
#define FS_ZIP_MOVE_MEMORY(dst, src, sz) memmove((dst), (src), (sz))
#endif

#ifndef FS_ZIP_ASSERT
#define FS_ZIP_ASSERT(x) assert(x)
#endif

#define FS_ZIP_ZERO_OBJECT(p)           FS_ZIP_ZERO_MEMORY((p), sizeof(*(p)))
#define FS_ZIP_MAX(x, y)                (((x) > (y)) ? (x) : (y))
#define FS_ZIP_MIN(x, y)                (((x) < (y)) ? (x) : (y))
#define FS_ZIP_ABS(x)                   (((x) > 0) ? (x) : -(x))
#define FS_ZIP_OFFSET_PTR(p, offset)    (((unsigned char*)(p)) + (offset))
#define FS_ZIP_ALIGN(x, a)              ((x + (a-1)) & ~(a-1))


static int fs_zip_strncpy_s(char* dst, size_t dstCap, const char* src, size_t count)
{
    size_t maxcount;
    size_t i;

    if (dst == 0) {
        return EINVAL;
    }
    if (dstCap == 0) {
        return EINVAL;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }

    maxcount = count;
    if (count == ((size_t)-1) || count >= dstCap) {        /* -1 = _TRUNCATE */
        maxcount = dstCap - 1;
    }

    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return ERANGE;
}



/* BEG fs_zip_deflate.h */
/*
This deflate stuff was taken from the old public domain version of miniz.c.
*/
enum
{
    FS_ZIP_DEFLATE_FLAG_PARSE_ZLIB_HEADER = 1,
    FS_ZIP_DEFLATE_FLAG_HAS_MORE_INPUT = 2,
    FS_ZIP_DEFLATE_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
    FS_ZIP_DEFLATE_FLAG_COMPUTE_ADLER32 = 8
};

enum
{
    FS_ZIP_DEFLATE_MAX_HUFF_TABLES    = 3,
    FS_ZIP_DEFLATE_MAX_HUFF_SYMBOLS_0 = 288,
    FS_ZIP_DEFLATE_MAX_HUFF_SYMBOLS_1 = 32,
    FS_ZIP_DEFLATE_MAX_HUFF_SYMBOLS_2 = 19,
    FS_ZIP_DEFLATE_FAST_LOOKUP_BITS   = 10,
    FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE   = 1 << FS_ZIP_DEFLATE_FAST_LOOKUP_BITS
};

typedef struct
{
    fs_uint8 codeSize[FS_ZIP_DEFLATE_MAX_HUFF_SYMBOLS_0];
    fs_int16 lookup[FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE];
    fs_int16 tree[FS_ZIP_DEFLATE_MAX_HUFF_SYMBOLS_0 * 2];
} fs_zip_deflate_huff_table;

#ifdef FS_64BIT
    typedef fs_uint64 fs_zip_deflate_bitbuf;
#else
    typedef fs_uint32 fs_zip_deflate_bitbuf;
#endif

typedef struct fs_zip_deflate_decompressor
{
    fs_uint32 state;
    fs_uint32 bitCount;
    fs_uint32 zhdr0;
    fs_uint32 zhdr1;
    fs_uint32 zAdler32;
    fs_uint32 final;
    fs_uint32 type;
    fs_uint32 checkAdler32;
    fs_uint32 dist;
    fs_uint32 counter;
    fs_uint32 extraCount;
    fs_uint32 tableSizes[FS_ZIP_DEFLATE_MAX_HUFF_TABLES];
    fs_zip_deflate_bitbuf bitBuffer;
    size_t distFromOutBufStart;
    fs_zip_deflate_huff_table tables[FS_ZIP_DEFLATE_MAX_HUFF_TABLES];
    fs_uint8 rawHeader[4];
    fs_uint8 lenCodes[FS_ZIP_DEFLATE_MAX_HUFF_SYMBOLS_0 + FS_ZIP_DEFLATE_MAX_HUFF_SYMBOLS_1 + 137];
} fs_zip_deflate_decompressor;

FS_API fs_result fs_zip_deflate_decompressor_init(fs_zip_deflate_decompressor* pDecompressor);
FS_API fs_result fs_zip_deflate_decompress(fs_zip_deflate_decompressor* pDecompressor, const fs_uint8* pInputBuffer, size_t* pInputBufferSize, fs_uint8* pOutputBufferStart, fs_uint8* pOutputBufferNext, size_t* pOutputBufferSize, fs_uint32 flags);
/* END fs_zip_deflate.h */


/* BEG fs_zip_deflate.c */
#define FS_ZIP_READ_LE16(p) ((fs_uint32)(((const fs_uint8*)(p))[0]) | ((fs_uint32)(((const fs_uint8*)(p))[1]) << 8U))
#define FS_ZIP_READ_LE32(p) ((fs_uint32)(((const fs_uint8*)(p))[0]) | ((fs_uint32)(((const fs_uint8*)(p))[1]) << 8U) | ((fs_uint32)(((const fs_uint8*)(p))[2]) << 16U) | ((fs_uint32)(((const fs_uint8*)(p))[3]) << 24U))
#define FS_ZIP_READ_LE64(p) ((fs_uint64)(((const fs_uint8*)(p))[0]) | ((fs_uint64)(((const fs_uint8*)(p))[1]) << 8U) | ((fs_uint64)(((const fs_uint8*)(p))[2]) << 16U) | ((fs_uint64)(((const fs_uint8*)(p))[3]) << 24U) | ((fs_uint64)(((const fs_uint8*)(p))[4]) << 32U) | ((fs_uint64)(((const fs_uint8*)(p))[5]) << 40U) | ((fs_uint64)(((const fs_uint8*)(p))[6]) << 48U) | ((fs_uint64)(((const fs_uint8*)(p))[7]) << 56U))

/*
This is all taken from the old public domain version of miniz.c but restyled for consistency with
the rest of the code base.
*/
#ifdef _MSC_VER
   #define FS_ZIP_DEFLATE_MACRO_END while (0, 0)
#else
   #define FS_ZIP_DEFLATE_MACRO_END while (0)
#endif

#define FS_ZIP_DEFLATE_CR_BEGIN switch(pDecompressor->state) { case 0:
#define FS_ZIP_DEFLATE_CR_RETURN(stateIndex, result) do { status = result; pDecompressor->state = stateIndex; goto common_exit; case stateIndex:; } FS_ZIP_DEFLATE_MACRO_END
#define FS_ZIP_DEFLATE_CR_RETURN_FOREVER(stateIndex, result) do { for (;;) { FS_ZIP_DEFLATE_CR_RETURN(stateIndex, result); } } FS_ZIP_DEFLATE_MACRO_END
#define FS_ZIP_DEFLATE_CR_FINISH }

/*
TODO: If the caller has indicated that there's no more input, and we attempt to read beyond the input buf, then something is wrong with the input because the inflator never
reads ahead more than it needs to. Currently FS_ZIP_DEFLATE_GET_BYTE() pads the end of the stream with 0's in this scenario.
*/
#define FS_ZIP_DEFLATE_GET_BYTE(stateIndex, c) do { \
    if (pInputBufferCurrent >= pInputBufferEnd) { \
        for (;;) { \
            if (flags & FS_ZIP_DEFLATE_FLAG_HAS_MORE_INPUT) { \
                FS_ZIP_DEFLATE_CR_RETURN(stateIndex, FS_NEEDS_MORE_INPUT); \
                if (pInputBufferCurrent < pInputBufferEnd) { \
                    c = *pInputBufferCurrent++; \
                    break; \
                } \
            } else { \
                c = 0; \
                break; \
            } \
        } \
    } else c = *pInputBufferCurrent++; } FS_ZIP_DEFLATE_MACRO_END

#define FS_ZIP_DEFLATE_NEED_BITS(stateIndex, n) do { unsigned int c; FS_ZIP_DEFLATE_GET_BYTE(stateIndex, c); bitBuffer |= (((fs_zip_deflate_bitbuf)c) << bitCount); bitCount += 8; } while (bitCount < (unsigned int)(n))
#define FS_ZIP_DEFLATE_SKIP_BITS(stateIndex, n) do { if (bitCount < (unsigned int)(n)) { FS_ZIP_DEFLATE_NEED_BITS(stateIndex, n); } bitBuffer >>= (n); bitCount -= (n); } FS_ZIP_DEFLATE_MACRO_END
#define FS_ZIP_DEFLATE_GET_BITS(stateIndex, b, n) do { if (bitCount < (unsigned int)(n)) { FS_ZIP_DEFLATE_NEED_BITS(stateIndex, n); } b = bitBuffer & ((1 << (n)) - 1); bitBuffer >>= (n); bitCount -= (n); } FS_ZIP_DEFLATE_MACRO_END

/*
FS_ZIP_DEFLATE_HUFF_BITBUF_FILL() is only used rarely, when the number of bytes remaining in the input buffer falls below 2.
It reads just enough bytes from the input stream that are needed to decode the next Huffman code (and absolutely no more). It works by trying to fully decode a
Huffman code by using whatever bits are currently present in the bit buffer. If this fails, it reads another byte, and tries again until it succeeds or until the
bit buffer contains >=15 bits (deflate's max. Huffman code size).
*/
#define FS_ZIP_DEFLATE_HUFF_BITBUF_FILL(stateIndex, pHuff) \
    do { \
        temp = (pHuff)->lookup[bitBuffer & (FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE - 1)]; \
        if (temp >= 0) { \
            codeLen = temp >> 9; \
            if ((codeLen) && (bitCount >= codeLen)) { \
                break; \
            } \
        } else if (bitCount > FS_ZIP_DEFLATE_FAST_LOOKUP_BITS) { \
            codeLen = FS_ZIP_DEFLATE_FAST_LOOKUP_BITS; \
            do { \
               temp = (pHuff)->tree[~temp + ((bitBuffer >> codeLen++) & 1)]; \
            } while ((temp < 0) && (bitCount >= (codeLen + 1))); \
            if (temp >= 0) {\
                break; \
            } \
        } \
        FS_ZIP_DEFLATE_GET_BYTE(stateIndex, c); \
        bitBuffer |= (((fs_zip_deflate_bitbuf)c) << bitCount); \
        bitCount += 8; \
    } while (bitCount < 15);

/*
FS_ZIP_DEFLATE_HUFF_DECODE() decodes the next Huffman coded symbol. It's more complex than you would initially expect because the zlib API expects the decompressor to never read
beyond the final byte of the deflate stream. (In other words, when this macro wants to read another byte from the input, it REALLY needs another byte in order to fully
decode the next Huffman code.) Handling this properly is particularly important on raw deflate (non-zlib) streams, which aren't followed by a byte aligned adler-32.
The slow path is only executed at the very end of the input buffer.
*/
#define FS_ZIP_DEFLATE_HUFF_DECODE(stateIndex, sym, pHuff) do { \
    int temp; \
    unsigned int codeLen; \
    unsigned int c; \
    if (bitCount < 15) { \
        if ((pInputBufferEnd - pInputBufferCurrent) < 2) { \
            FS_ZIP_DEFLATE_HUFF_BITBUF_FILL(stateIndex, pHuff); \
        } else { \
            bitBuffer |= (((fs_zip_deflate_bitbuf)pInputBufferCurrent[0]) << bitCount) | (((fs_zip_deflate_bitbuf)pInputBufferCurrent[1]) << (bitCount + 8)); \
            pInputBufferCurrent += 2; \
            bitCount += 16; \
        } \
    } \
    if ((temp = (pHuff)->lookup[bitBuffer & (FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE - 1)]) >= 0) { \
        codeLen = temp >> 9, temp &= 511; \
    } \
    else { \
        codeLen = FS_ZIP_DEFLATE_FAST_LOOKUP_BITS; do { temp = (pHuff)->tree[~temp + ((bitBuffer >> codeLen++) & 1)]; } while (temp < 0); \
    } sym = temp; bitBuffer >>= codeLen; bitCount -= codeLen; } FS_ZIP_DEFLATE_MACRO_END


#define fs_zip_deflate_init(r) do { (r)->state = 0; } FS_ZIP_DEFLATE_MACRO_END
#define fs_zip_deflate_get_adler32(r) (r)->checkAdler32

FS_API fs_result fs_zip_deflate_decompressor_init(fs_zip_deflate_decompressor* pDecompressor)
{
    if (pDecompressor == NULL) {
        return FS_INVALID_ARGS;
    }

    fs_zip_deflate_init(pDecompressor);

    return FS_SUCCESS;
}

FS_API fs_result fs_zip_deflate_decompress(fs_zip_deflate_decompressor* pDecompressor, const fs_uint8* pInputBuffer, size_t* pInputBufferSize, fs_uint8* pOutputBufferStart, fs_uint8* pOutputBufferNext, size_t* pOutputBufferSize, fs_uint32 flags)
{
    static const int sLengthBase[31] =
    {
        3,  4,  5,  6,   7,   8,   9,   10,  11,  13,
        15, 17, 19, 23,  27,  31,  35,  43,  51,  59,
        67, 83, 99, 115, 131, 163, 195, 227, 258, 0,
        0
    };
    static const int sLengthExtra[31] =
    {
        0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4,
        5, 5, 5, 5, 0, 0, 0
    };
    static const int sDistBase[32] =
    {
        1,   2,   3,   4,   5,    7,    9,    13,   17,   25,   33,   49,    65,    97,    129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0,   0
    };
    static const int sDistExtra[32] =
    {
        0,  0,  0,  0,  1,  1,  2,  2,
        3,  3,  4,  4,  5,  5,  6,  6,
        7,  7,  8,  8,  9,  9,  10, 10,
        11, 11, 12, 12, 13, 13
    };
    static const fs_uint8 sLengthDeZigZag[19] =
    {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    static const int sMinTableSizes[3] = { 257, 1, 4 };

    fs_result status = FS_ERROR;
    fs_uint32 bitCount;
    fs_uint32 dist;
    fs_uint32 counter;
    fs_uint32 extraCount;
    fs_zip_deflate_bitbuf bitBuffer;
    const fs_uint8* pInputBufferCurrent = pInputBuffer;
    const fs_uint8* const pInputBufferEnd = pInputBuffer + *pInputBufferSize;
    fs_uint8 *pOutputBufferCurrent = pOutputBufferNext;
    fs_uint8 *const pOutputBufferEnd = pOutputBufferNext + *pOutputBufferSize;
    size_t outputBufferSizeMask = (flags & FS_ZIP_DEFLATE_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) ? (size_t)-1 : ((pOutputBufferNext - pOutputBufferStart) + *pOutputBufferSize) - 1, distFromOutBufStart;

    /* Ensure the output buffer's size is a power of 2, unless the output buffer is large enough to hold the entire output file (in which case it doesn't matter). */
    if (((outputBufferSizeMask + 1) & outputBufferSizeMask) || (pOutputBufferNext < pOutputBufferStart)) {
        *pInputBufferSize = *pOutputBufferSize = 0;
        return FS_INVALID_ARGS;
    }
    
    bitCount = pDecompressor->bitCount; bitBuffer = pDecompressor->bitBuffer; dist = pDecompressor->dist; counter = pDecompressor->counter; extraCount = pDecompressor->extraCount; distFromOutBufStart = pDecompressor->distFromOutBufStart;
    FS_ZIP_DEFLATE_CR_BEGIN
    
    bitBuffer = bitCount = dist = counter = extraCount = pDecompressor->zhdr0 = pDecompressor->zhdr1 = 0;
    pDecompressor->zAdler32 = pDecompressor->checkAdler32 = 1;

    if (flags & FS_ZIP_DEFLATE_FLAG_PARSE_ZLIB_HEADER) {
        FS_ZIP_DEFLATE_GET_BYTE(1, pDecompressor->zhdr0);
        FS_ZIP_DEFLATE_GET_BYTE(2, pDecompressor->zhdr1);
        
        counter = (((pDecompressor->zhdr0 * 256 + pDecompressor->zhdr1) % 31 != 0) || (pDecompressor->zhdr1 & 32) || ((pDecompressor->zhdr0 & 15) != 8));
        
        if (!(flags & FS_ZIP_DEFLATE_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)) {
            counter |= (((1U << (8U + (pDecompressor->zhdr0 >> 4))) > 32768U) || ((outputBufferSizeMask + 1) < (size_t)(1U << (8U + (pDecompressor->zhdr0 >> 4)))));
        }
        
        if (counter) {
            FS_ZIP_DEFLATE_CR_RETURN_FOREVER(36, FS_ERROR);
        }
    }

    do {
        FS_ZIP_DEFLATE_GET_BITS(3, pDecompressor->final, 3); pDecompressor->type = pDecompressor->final >> 1;

        if (pDecompressor->type == 0) {
            FS_ZIP_DEFLATE_SKIP_BITS(5, bitCount & 7);

            for (counter = 0; counter < 4; ++counter) {
                if (bitCount) {
                    FS_ZIP_DEFLATE_GET_BITS(6, pDecompressor->rawHeader[counter], 8);
                } else {
                    FS_ZIP_DEFLATE_GET_BYTE(7, pDecompressor->rawHeader[counter]);
                }
            }

            if ((counter = (pDecompressor->rawHeader[0] | (pDecompressor->rawHeader[1] << 8))) != (unsigned int)(0xFFFF ^ (pDecompressor->rawHeader[2] | (pDecompressor->rawHeader[3] << 8)))) {
                FS_ZIP_DEFLATE_CR_RETURN_FOREVER(39, FS_ERROR);
            }

            while ((counter) && (bitCount)) {
                FS_ZIP_DEFLATE_GET_BITS(51, dist, 8);

                while (pOutputBufferCurrent >= pOutputBufferEnd) {
                    FS_ZIP_DEFLATE_CR_RETURN(52, FS_HAS_MORE_OUTPUT);
                }

                *pOutputBufferCurrent++ = (fs_uint8)dist;
                counter--;
            }

            while (counter) {
                size_t n;
                
                while (pOutputBufferCurrent >= pOutputBufferEnd) {
                    FS_ZIP_DEFLATE_CR_RETURN(9, FS_HAS_MORE_OUTPUT);
                }

                while (pInputBufferCurrent >= pInputBufferEnd) {
                    if (flags & FS_ZIP_DEFLATE_FLAG_HAS_MORE_INPUT) {
                        FS_ZIP_DEFLATE_CR_RETURN(38, FS_NEEDS_MORE_INPUT);
                    } else {
                        FS_ZIP_DEFLATE_CR_RETURN_FOREVER(40, FS_ERROR);
                    }
                }

                n = FS_ZIP_MIN(FS_ZIP_MIN((size_t)(pOutputBufferEnd - pOutputBufferCurrent), (size_t)(pInputBufferEnd - pInputBufferCurrent)), counter);
                FS_ZIP_COPY_MEMORY(pOutputBufferCurrent, pInputBufferCurrent, n); pInputBufferCurrent += n; pOutputBufferCurrent += n; counter -= (unsigned int)n;
            }
        }
        else if (pDecompressor->type == 3) {
            FS_ZIP_DEFLATE_CR_RETURN_FOREVER(10, FS_ERROR);
        } else {
            if (pDecompressor->type == 1) {
                fs_uint8 *p = pDecompressor->tables[0].codeSize;
                unsigned int i;

                pDecompressor->tableSizes[0] = 288;
                pDecompressor->tableSizes[1] = 32;
                memset(pDecompressor->tables[1].codeSize, 5, 32);

                for (i = 0; i <= 143; ++i) {
                    *p++ = 8;
                }
                
                for (; i <= 255; ++i) {
                    *p++ = 9;
                }
                
                for (; i <= 279; ++i) {
                    *p++ = 7;
                }
                
                for (; i <= 287; ++i) {
                    *p++ = 8;
                }
            } else {
                for (counter = 0; counter < 3; counter++) {
                    FS_ZIP_DEFLATE_GET_BITS(11, pDecompressor->tableSizes[counter], "\05\05\04"[counter]);
                    pDecompressor->tableSizes[counter] += sMinTableSizes[counter];
                }

                FS_ZIP_ZERO_MEMORY(&pDecompressor->tables[2].codeSize, sizeof(pDecompressor->tables[2].codeSize));

                for (counter = 0; counter < pDecompressor->tableSizes[2]; counter++) {
                    unsigned int s;
                    FS_ZIP_DEFLATE_GET_BITS(14, s, 3);
                    pDecompressor->tables[2].codeSize[sLengthDeZigZag[counter]] = (fs_uint8)s;
                }

                pDecompressor->tableSizes[2] = 19;
            }

            for (; (int)pDecompressor->type >= 0; pDecompressor->type--) {
                int tree_next;
                int tree_cur;
                fs_zip_deflate_huff_table *pTable;
                unsigned int i;
                unsigned int j;
                unsigned int usedSyms;
                unsigned int total;
                unsigned int symIndex;
                unsigned int nextCode[17];
                unsigned int totalSyms[16];
                
                pTable = &pDecompressor->tables[pDecompressor->type];
                
                FS_ZIP_ZERO_MEMORY(totalSyms, sizeof(totalSyms));
                FS_ZIP_ZERO_MEMORY(pTable->lookup, sizeof(pTable->lookup));
                FS_ZIP_ZERO_MEMORY(pTable->tree, sizeof(pTable->tree));

                for (i = 0; i < pDecompressor->tableSizes[pDecompressor->type]; ++i) {
                    totalSyms[pTable->codeSize[i]]++;
                }

                usedSyms = 0;
                total = 0;
                nextCode[0] = nextCode[1] = 0;

                for (i = 1; i <= 15; ++i) {
                    usedSyms += totalSyms[i];
                    nextCode[i + 1] = (total = ((total + totalSyms[i]) << 1));
                }

                if ((65536 != total) && (usedSyms > 1)) {
                    FS_ZIP_DEFLATE_CR_RETURN_FOREVER(35, FS_ERROR);
                }

                for (tree_next = -1, symIndex = 0; symIndex < pDecompressor->tableSizes[pDecompressor->type]; ++symIndex) {
                    unsigned int revCode = 0;
                    unsigned int l;
                    unsigned int curCode;
                    unsigned int codeSize = pTable->codeSize[symIndex];
                    
                    if (!codeSize) {
                        continue;
                    }

                    curCode = nextCode[codeSize]++;
                    
                    for (l = codeSize; l > 0; l--, curCode >>= 1) {
                        revCode = (revCode << 1) | (curCode & 1);
                    }

                    if (codeSize <= FS_ZIP_DEFLATE_FAST_LOOKUP_BITS) {
                        fs_int16 k = (fs_int16)((codeSize << 9) | symIndex);
                        
                        while (revCode < FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE) {
                            pTable->lookup[revCode] = k;
                            revCode += (1 << codeSize);
                        }
                        
                        continue;
                    }

                    if (0 == (tree_cur = pTable->lookup[revCode & (FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE - 1)])) {
                        pTable->lookup[revCode & (FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE - 1)] = (fs_int16)tree_next;
                        tree_cur = tree_next;
                        tree_next -= 2;
                    }

                    revCode >>= (FS_ZIP_DEFLATE_FAST_LOOKUP_BITS - 1);

                    for (j = codeSize; j > (FS_ZIP_DEFLATE_FAST_LOOKUP_BITS + 1); j--) {
                        tree_cur -= ((revCode >>= 1) & 1);

                        if (!pTable->tree[-tree_cur - 1]) {
                            pTable->tree[-tree_cur - 1] = (fs_int16)tree_next; tree_cur = tree_next; tree_next -= 2;
                        } else {
                            tree_cur = pTable->tree[-tree_cur - 1];
                        }
                    }

                    tree_cur -= ((revCode >>= 1) & 1);
                    pTable->tree[-tree_cur - 1] = (fs_int16)symIndex;
                }

                if (pDecompressor->type == 2) {
                    for (counter = 0; counter < (pDecompressor->tableSizes[0] + pDecompressor->tableSizes[1]); ) {
                        unsigned int s;
                        
                        FS_ZIP_DEFLATE_HUFF_DECODE(16, dist, &pDecompressor->tables[2]);
                        
                        if (dist < 16) {
                            pDecompressor->lenCodes[counter++] = (fs_uint8)dist;
                            continue;
                        }

                        if ((dist == 16) && (!counter)) {
                            FS_ZIP_DEFLATE_CR_RETURN_FOREVER(17, FS_ERROR);
                        }

                        extraCount = "\02\03\07"[dist - 16];
                        FS_ZIP_DEFLATE_GET_BITS(18, s, extraCount);
                        s += "\03\03\013"[dist - 16];
                        memset(pDecompressor->lenCodes + counter, (dist == 16) ? pDecompressor->lenCodes[counter - 1] : 0, s);
                        counter += s;
                    }

                    if ((pDecompressor->tableSizes[0] + pDecompressor->tableSizes[1]) != counter) {
                        FS_ZIP_DEFLATE_CR_RETURN_FOREVER(21, FS_ERROR);
                    }

                    FS_ZIP_COPY_MEMORY(pDecompressor->tables[0].codeSize, pDecompressor->lenCodes, pDecompressor->tableSizes[0]); FS_ZIP_COPY_MEMORY(pDecompressor->tables[1].codeSize, pDecompressor->lenCodes + pDecompressor->tableSizes[0], pDecompressor->tableSizes[1]);
                }
            }
            for (;;) {
                fs_uint8 *pSrc;
                for (;;) {
                    if (((pInputBufferEnd - pInputBufferCurrent) < 4) || ((pOutputBufferEnd - pOutputBufferCurrent) < 2)) {
                        FS_ZIP_DEFLATE_HUFF_DECODE(23, counter, &pDecompressor->tables[0]);
                        
                        if (counter >= 256) {
                          break;
                        }
                        
                        while (pOutputBufferCurrent >= pOutputBufferEnd) {
                            FS_ZIP_DEFLATE_CR_RETURN(24, FS_HAS_MORE_OUTPUT);
                        }
                    
                        *pOutputBufferCurrent++ = (fs_uint8)counter;
                    } else {
                        int sym2;
                        unsigned int codeLen;
#ifdef FS_64BIT      
                        if (bitCount < 30) {
                            bitBuffer |= (((fs_zip_deflate_bitbuf)FS_ZIP_READ_LE32(pInputBufferCurrent)) << bitCount);
                            pInputBufferCurrent += 4;
                            bitCount += 32;
                        }
#else               
                        if (bitCount < 15) {
                            bitBuffer |= (((fs_zip_deflate_bitbuf)FS_ZIP_READ_LE16(pInputBufferCurrent)) << bitCount);
                            pInputBufferCurrent += 2;
                            bitCount += 16;
                        }
#endif              
                        if ((sym2 = pDecompressor->tables[0].lookup[bitBuffer & (FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE - 1)]) >= 0) {
                            codeLen = sym2 >> 9;
                        } else {
                            codeLen = FS_ZIP_DEFLATE_FAST_LOOKUP_BITS;

                            do {
                                sym2 = pDecompressor->tables[0].tree[~sym2 + ((bitBuffer >> codeLen++) & 1)];
                            } while (sym2 < 0);
                        }
                    
                        counter = sym2;
                        bitBuffer >>= codeLen;
                        bitCount -= codeLen;
                    
                        if (counter & 256) {
                            break;
                        }
#ifndef FS_64BIT     
                        if (bitCount < 15) {
                            bitBuffer |= (((fs_zip_deflate_bitbuf)FS_ZIP_READ_LE16(pInputBufferCurrent)) << bitCount);
                            pInputBufferCurrent += 2;
                            bitCount += 16;
                        }
#endif              
                        if ((sym2 = pDecompressor->tables[0].lookup[bitBuffer & (FS_ZIP_DEFLATE_FAST_LOOKUP_SIZE - 1)]) >= 0) {
                            codeLen = sym2 >> 9;
                        } else {
                            codeLen = FS_ZIP_DEFLATE_FAST_LOOKUP_BITS;

                            do {
                                sym2 = pDecompressor->tables[0].tree[~sym2 + ((bitBuffer >> codeLen++) & 1)];
                            } while (sym2 < 0);
                        }
                    
                        bitBuffer >>= codeLen; bitCount -= codeLen;
                        
                        pOutputBufferCurrent[0] = (fs_uint8)counter;
                        if (sym2 & 256) {
                            pOutputBufferCurrent++;
                            counter = sym2;
                            break;
                        }

                        pOutputBufferCurrent[1] = (fs_uint8)sym2;
                        pOutputBufferCurrent += 2;
                    }
                }
                
                if ((counter &= 511) == 256) {
                    break;
                }
                
                extraCount = sLengthExtra[counter - 257];
                counter = sLengthBase[counter - 257];
                
                if (extraCount) {
                    unsigned int extraBits;
                    FS_ZIP_DEFLATE_GET_BITS(25, extraBits, extraCount);
                    counter += extraBits;
                }
                
                FS_ZIP_DEFLATE_HUFF_DECODE(26, dist, &pDecompressor->tables[1]);
                
                extraCount = sDistExtra[dist];
                dist = sDistBase[dist];
                
                if (extraCount) {
                    unsigned int extraBits;
                    FS_ZIP_DEFLATE_GET_BITS(27, extraBits, extraCount);
                    dist += extraBits;
                }
                
                distFromOutBufStart = pOutputBufferCurrent - pOutputBufferStart;
                if ((dist > distFromOutBufStart) && (flags & FS_ZIP_DEFLATE_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)) {
                    FS_ZIP_DEFLATE_CR_RETURN_FOREVER(37, FS_ERROR);
                }
                
                pSrc = pOutputBufferStart + ((distFromOutBufStart - dist) & outputBufferSizeMask);
                
                if ((FS_ZIP_MAX(pOutputBufferCurrent, pSrc) + counter) > pOutputBufferEnd) {
                    while (counter--) {
                        while (pOutputBufferCurrent >= pOutputBufferEnd) {
                            FS_ZIP_DEFLATE_CR_RETURN(53, FS_HAS_MORE_OUTPUT);
                        }
                        
                        *pOutputBufferCurrent++ = pOutputBufferStart[(distFromOutBufStart++ - dist) & outputBufferSizeMask];
                    }
                
                    continue;
                }
                
                do {
                    pOutputBufferCurrent[0] = pSrc[0];
                    pOutputBufferCurrent[1] = pSrc[1];
                    pOutputBufferCurrent[2] = pSrc[2];
                    pOutputBufferCurrent += 3;
                    pSrc += 3;
                } while ((int)(counter -= 3) > 2);
                
                if ((int)counter > 0) {
                    pOutputBufferCurrent[0] = pSrc[0];
                
                    if ((int)counter > 1) {
                        pOutputBufferCurrent[1] = pSrc[1];
                    }
                
                    pOutputBufferCurrent += counter;
                }
            }
        }
    } while (!(pDecompressor->final & 1));

    if (flags & FS_ZIP_DEFLATE_FLAG_PARSE_ZLIB_HEADER) {
        FS_ZIP_DEFLATE_SKIP_BITS(32, bitCount & 7);
        
        for (counter = 0; counter < 4; ++counter) {
            unsigned int s;
            if (bitCount) {
                FS_ZIP_DEFLATE_GET_BITS(41, s, 8);
            } else {
                FS_ZIP_DEFLATE_GET_BYTE(42, s);
            }
            
            pDecompressor->zAdler32 = (pDecompressor->zAdler32 << 8) | s;
        }
    }

    FS_ZIP_DEFLATE_CR_RETURN_FOREVER(34, FS_SUCCESS);
    FS_ZIP_DEFLATE_CR_FINISH

common_exit:
    pDecompressor->bitCount = bitCount;
    pDecompressor->bitBuffer = bitBuffer;
    pDecompressor->dist = dist;
    pDecompressor->counter = counter;
    pDecompressor->extraCount = extraCount;
    pDecompressor->distFromOutBufStart = distFromOutBufStart;

    *pInputBufferSize  = pInputBufferCurrent  - pInputBuffer;
    *pOutputBufferSize = pOutputBufferCurrent - pOutputBufferNext;

    if ((flags & (FS_ZIP_DEFLATE_FLAG_PARSE_ZLIB_HEADER | FS_ZIP_DEFLATE_FLAG_COMPUTE_ADLER32)) && (status >= 0)) {
        const fs_uint8* ptr = pOutputBufferNext;
        size_t buf_len = *pOutputBufferSize;
        fs_uint32 s1 = pDecompressor->checkAdler32 & 0xffff;
        fs_uint32 s2 = pDecompressor->checkAdler32 >> 16;
        size_t block_len = buf_len % 5552;

        while (buf_len) {
            fs_uint32 i;

            for (i = 0; i + 7 < block_len; i += 8, ptr += 8) {
                s1 += ptr[0], s2 += s1; s1 += ptr[1], s2 += s1; s1 += ptr[2], s2 += s1; s1 += ptr[3], s2 += s1;
                s1 += ptr[4], s2 += s1; s1 += ptr[5], s2 += s1; s1 += ptr[6], s2 += s1; s1 += ptr[7], s2 += s1;
            }

            for (; i < block_len; ++i) {
                s1 += *ptr++, s2 += s1;
            }

            s1 %= 65521U;
            s2 %= 65521U;
            buf_len -= block_len;
            block_len = 5552;
        }

        pDecompressor->checkAdler32 = (s2 << 16) + s1;

        if ((status == FS_SUCCESS) && (flags & FS_ZIP_DEFLATE_FLAG_PARSE_ZLIB_HEADER) && (pDecompressor->checkAdler32 != pDecompressor->zAdler32)) {
            status = FS_CHECKSUM_MISMATCH;
        }
    }

    return status;
}
/* END fs_zip_deflate.c */


/* BEG fs_zip.c */
#ifndef FS_ZIP_CACHE_SIZE_IN_BYTES
#define FS_ZIP_CACHE_SIZE_IN_BYTES              32768
#endif

#ifndef FS_ZIP_COMPRESSED_CACHE_SIZE_IN_BYTES
#define FS_ZIP_COMPRESSED_CACHE_SIZE_IN_BYTES   4096
#endif

#define FS_ZIP_EOCD_SIGNATURE                   0x06054b50
#define FS_ZIP_EOCD64_SIGNATURE                 0x06064b50
#define FS_ZIP_EOCD64_LOCATOR_SIGNATURE         0x07064b50
#define FS_ZIP_CD_FILE_HEADER_SIGNATURE         0x02014b50

#define FS_ZIP_COMPRESSION_METHOD_STORE         0
#define FS_ZIP_COMPRESSION_METHOD_DEFLATE       8


typedef struct fs_zip_cd_node fs_zip_cd_node;
struct fs_zip_cd_node
{
    size_t iFile;                   /* Will be undefined for non leaf nodes. */
    const char* pName;
    size_t nameLen;
    size_t childCount;
    fs_zip_cd_node* pChildren;
    size_t _descendantRangeBeg;     /* Only used for building the CD node graph. */
    size_t _descendantRangeEnd;     /* Only used for building the CD node graph. */
    size_t _descendantPrefixLen;    /* Only used for building the CD node graph. */
};

typedef struct fs_zip_index
{
    size_t offsetInBytes;           /* The offset in bytes of the item relative to the start of the central directory. */
} fs_zip_index;

typedef struct fs_zip
{
    size_t fileCount;               /* Total number of records in the central directory. */
    size_t centralDirectorySize;    /* Size in bytes of the central directory. */
    void* pCentralDirectory;        /* Offset of pHeap. */
    fs_zip_index* pIndex;           /* Offset of pHeap. There will be fileCount items in this array, and each item is sorted by the file path of each item. */
    fs_zip_cd_node* pCDRootNode;    /* The root node of our accelerated central directory data structure. */
    void* pHeap;                    /* A single heap allocation for storing the central directory and index. */
} fs_zip;

typedef struct fs_zip_file_info
{
    const char* pPath;
    size_t pathLen;
    fs_uint16 compressionMethod;
    fs_uint64 compressedSize;
    fs_uint64 uncompressedSize;
    fs_uint64 fileOffset;            /* The offset in bytes from the start of the archive file. */
    fs_bool32 directory;
} fs_zip_file_info;

static size_t fs_alloc_size_zip(const void* pBackendConfig)
{
    (void)pBackendConfig;

    return sizeof(fs_zip);
}


typedef struct fs_zip_refstring
{
    const char* str;
    size_t len;
} fs_zip_refstring;

static int fs_zip_binary_search_zip_cd_node_compare(void* pUserData, const void* pKey, const void* pVal)
{
    const fs_zip_refstring* pRefString = (const fs_zip_refstring*)pKey;
    const fs_zip_cd_node* pNode = (const fs_zip_cd_node*)pVal;
    int compareResult;

    (void)pUserData;

    compareResult = strncmp(pRefString->str, pNode->pName, FS_ZIP_MIN(pRefString->len, pNode->nameLen));
    if (compareResult == 0 && pRefString->len != pNode->nameLen) {
        compareResult = (pRefString->len < pNode->nameLen) ? -1 : 1;
    }

    return compareResult;
}

static fs_zip_cd_node* fs_zip_cd_node_find_child(fs_zip_cd_node* pParent, const char* pChildName, size_t childNameLen)
{
    fs_zip_refstring str;
    str.str = pChildName;
    str.len = childNameLen;

    return (fs_zip_cd_node*)fs_sorted_search(&str, pParent->pChildren, pParent->childCount, sizeof(*pParent->pChildren), fs_zip_binary_search_zip_cd_node_compare, NULL);
}


static fs_result fs_zip_get_file_info_by_record_offset(fs_zip* pZip, size_t offset, fs_zip_file_info* pInfo)
{
    fs_uint16 filePathLen;
    const unsigned char* pCentralDirectoryRecord;

    FS_ZIP_ASSERT(pZip  != NULL);
    FS_ZIP_ASSERT(pInfo != NULL);

    FS_ZIP_ZERO_OBJECT(pInfo);

    pCentralDirectoryRecord = (const unsigned char*)FS_ZIP_OFFSET_PTR(pZip->pCentralDirectory, offset);

    /* Check that we're not going to overflow the central directory. */
    if (offset + 46 > pZip->centralDirectorySize) {   /* 46 is the offset of the file path. */
        return FS_INVALID_FILE;  /* Look like an invalid central directory. */
    }

    /* Grab the length of the file path. */
    filePathLen = ((fs_uint16)pCentralDirectoryRecord[29] << 8) | pCentralDirectoryRecord[28];

    /* Now we can move to the file path, again making sure we have enough room for the file path. */
    if (offset + 46 + filePathLen > pZip->centralDirectorySize) {
        return FS_INVALID_FILE;  /* Looks like an invalid central directory. */
    }

    pInfo->pPath   = (const char*)(pCentralDirectoryRecord + 46);
    pInfo->pathLen = filePathLen;

    /* We can determine if the entry is a directory by checking if the path ends in a slash. */
    if (pInfo->pPath[pInfo->pathLen-1] == '/' || pInfo->pPath[pInfo->pathLen-1] == '\\') {
        pInfo->directory = FS_TRUE;
    }

    /* Compression method. */
    pInfo->compressionMethod = ((fs_uint16)pCentralDirectoryRecord[11] << 8) | pCentralDirectoryRecord[10];

    /* Get the size of the file. */
    pInfo->compressedSize   = ((fs_uint32)pCentralDirectoryRecord[23] << 24) | ((fs_uint32)pCentralDirectoryRecord[22] << 16) | ((fs_uint32)pCentralDirectoryRecord[21] << 8) | (fs_uint32)pCentralDirectoryRecord[20];
    pInfo->uncompressedSize = ((fs_uint32)pCentralDirectoryRecord[27] << 24) | ((fs_uint32)pCentralDirectoryRecord[26] << 16) | ((fs_uint32)pCentralDirectoryRecord[25] << 8) | (fs_uint32)pCentralDirectoryRecord[24];

    /* File offset. */
    pInfo->fileOffset = ((fs_uint32)pCentralDirectoryRecord[45] << 24) | ((fs_uint32)pCentralDirectoryRecord[44] << 16) | ((fs_uint32)pCentralDirectoryRecord[43] << 8) | pCentralDirectoryRecord[42];


    /*
    Load Zip64 data if necessary. It's in the extra data. The extra data is made up of a
    number of blocks. Each block has a 2 byte ID and a 2 byte size. When reading from
    each block, we need to make sure we don't try reading beyond the reported size of
    the extra data.
    
    The Zip64 data will be stored in a block with the ID of 0x0001. The presence of each
    member inside this block is conditional to whether or not it's set to 0xFFFFFFFF in
    the main part of the central directory record.
    */
    if (pInfo->compressedSize == 0xFFFFFFFF || pInfo->uncompressedSize == 0xFFFFFFFF || pInfo->fileOffset == 0xFFFFFFFF) {
        fs_uint16 extraDataSize   = ((fs_uint16)pCentralDirectoryRecord[31] << 8) | pCentralDirectoryRecord[30];
        fs_uint16 extraDataOffset = 0;

        const unsigned char* pExtraData = (const unsigned char*)(pCentralDirectoryRecord + 46 + filePathLen);

        /* For each chunk in the extra data. */
        for (;;) {
            fs_uint16 chunkID;
            fs_uint16 chunkSize;

            if (extraDataOffset == extraDataSize) {
                break;  /* We're done. */
            }

            if (extraDataOffset > extraDataSize) {
                return FS_INVALID_FILE;  /* We've somehow read past the extra data. Abort. */
            }

            if ((extraDataSize - extraDataOffset) < 4) {
                return FS_INVALID_FILE;  /* Not enough data in the extra data to read the chunk header. */
            }

            chunkID   = ((fs_uint16)pExtraData[extraDataOffset+1] << 8) | pExtraData[extraDataOffset+0];
            chunkSize = ((fs_uint16)pExtraData[extraDataOffset+3] << 8) | pExtraData[extraDataOffset+2];

            /* Increment the offset to make it easy to parse the data in the next section. */
            extraDataOffset += 4;

            if ((extraDataSize - extraDataOffset) < chunkSize) {
                return FS_INVALID_FILE;  /* Not enough data in the extra data to read the chunk. */
            }

            if (chunkID == 0x0001) {
                /* Zip64 data. */
                fs_uint32 chunkLocalOffset = 0;

                if (pInfo->uncompressedSize == 0xFFFFFFFF) {
                    if (chunkLocalOffset + 8 > chunkSize) {
                        return FS_INVALID_FILE;  /* Not enough data in the chunk. */
                    }

                    pInfo->uncompressedSize = ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+7] << 56) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+6] << 48) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+5] << 40) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+4] << 32) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+3] << 24) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+2] << 16) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+1] << 8) | (fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+0];
                    chunkLocalOffset += 8;
                }

                if (pInfo->compressedSize == 0xFFFFFFFF) {
                    if (chunkLocalOffset + 8 > chunkSize) {
                        return FS_INVALID_FILE;  /* Not enough data in the chunk. */
                    }

                    pInfo->compressedSize = ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+7] << 56) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+6] << 48) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+5] << 40) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+4] << 32) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+3] << 24) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+2] << 16) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+1] << 8) | (fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+0];
                    chunkLocalOffset += 8;
                }

                if (pInfo->fileOffset == 0xFFFFFFFF) {
                    if (chunkLocalOffset + 8 > chunkSize) {
                        return FS_INVALID_FILE;  /* Not enough data in the chunk. */
                    }

                    pInfo->fileOffset = ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+7] << 56) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+6] << 48) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+5] << 40) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+4] << 32) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+3] << 24) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+2] << 16) | ((fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+1] << 8) | (fs_uint64)pExtraData[extraDataOffset+chunkLocalOffset+0];
                    chunkLocalOffset += 8;
                }
            }

            extraDataOffset += chunkSize;
        }
    }

    return FS_SUCCESS;
}

static const char* fs_zip_get_file_path_by_record_offset(fs_zip* pZip, size_t offset, size_t* pLength)
{
    fs_uint16 length;
    const char* pCentralDirectoryRecord;

    FS_ZIP_ASSERT(pLength != NULL);

    *pLength = 0;

    pCentralDirectoryRecord = (const char*)FS_ZIP_OFFSET_PTR(pZip->pCentralDirectory, offset);

    /* Check that we're not going to overflow the central directory. */
    if (offset + 46 > pZip->centralDirectorySize) {   /* 46 is the offset of the file path. */
        return NULL;    /* Look like an invalid central directory. */
    }

    /* Grab the length of the file. */
    length = ((fs_uint16)pCentralDirectoryRecord[29] << 8) | pCentralDirectoryRecord[28];

    /* Now we can move to the file path, again making sure we have enough room for the file path. */
    if (offset + 46 + length > pZip->centralDirectorySize) {
        return NULL;    /* Looks like an invalid central directory. */
    }

    /* We now how enough information to get the file path. */
    *pLength = length;
    return pCentralDirectoryRecord + 46;
}

static fs_result fs_zip_find_file_by_path(fs_zip* pZip, const fs_allocation_callbacks* pAllocationCallbacks, const char* pFilePath, size_t filePathLen, size_t* pFileIndex)
{
    fs_result result;
    fs_path_iterator pathIterator;
    fs_zip_cd_node* pCurrentNode;
    char  pFilePathCleanStack[1024];
    char* pFilePathCleanHeap = NULL;
    char* pFilePathClean;
    int filePathCleanLen;

    FS_ZIP_ASSERT(pZip       != NULL);
    FS_ZIP_ASSERT(pFilePath  != NULL);
    FS_ZIP_ASSERT(pFileIndex != NULL);

    if (filePathLen == 0) {
        return FS_INVALID_ARGS; /* The path is empty. */
    }

    /* Skip past the root item if any. */
    if (pFilePath[0] == '/' || pFilePath[0] == '\\') {
        pFilePath += 1;
        if (filePathLen > 0) {
            filePathLen -= 1;
        }
    }

    /* The path must be clean of any special directories. We'll have to clean the path with fs_path_normalize(). */
    filePathCleanLen = fs_path_normalize(pFilePathCleanStack, sizeof(pFilePathCleanStack), pFilePath, filePathLen, FS_NO_ABOVE_ROOT_NAVIGATION);
    if (filePathCleanLen < 0) {
        return FS_DOES_NOT_EXIST;
    }

    if (filePathCleanLen > (int)sizeof(pFilePathCleanStack)) {
        pFilePathCleanHeap = (char*)fs_malloc(filePathCleanLen + 1, pAllocationCallbacks);
        if (pFilePathCleanHeap == NULL) {
            return FS_OUT_OF_MEMORY;
        }

        fs_path_normalize(pFilePathCleanHeap, filePathCleanLen + 1, pFilePath, filePathLen, FS_NO_ABOVE_ROOT_NAVIGATION); /* <-- This should never fail. */
        pFilePathClean = pFilePathCleanHeap;
    } else {
        pFilePathClean = pFilePathCleanStack;
    }

    /* Start at the root node. */
    pCurrentNode = pZip->pCDRootNode;

    result = fs_result_from_errno(fs_path_first(pFilePathClean, (size_t)filePathCleanLen, &pathIterator));
    if (result == FS_SUCCESS) {
        /* Reset the error code for safety. The loop below will be setting it to a proper value. */
        result = FS_DOES_NOT_EXIST;
        for (;;) {
            fs_zip_cd_node* pChildNode;
            
            pChildNode = fs_zip_cd_node_find_child(pCurrentNode, pathIterator.pFullPath + pathIterator.segmentOffset, pathIterator.segmentLength);
            if (pChildNode == NULL) {
                result = FS_DOES_NOT_EXIST;
                break;
            }

            pCurrentNode = pChildNode;

            result = fs_result_from_errno(fs_path_next(&pathIterator));
            if (result != FS_SUCCESS) {
                /* We've reached the end. The file we're on must be the file index. */
                *pFileIndex = pCurrentNode->iFile;

                result = FS_SUCCESS;
                break;
            }
        }
    } else {
        result = FS_DOES_NOT_EXIST;
    }

    fs_free(pFilePathCleanHeap, pAllocationCallbacks);
    return result;
}

static fs_result fs_zip_get_file_info_by_path(fs_zip* pZip, const fs_allocation_callbacks* pAllocationCallbacks, const char* pFilePath, size_t filePathLen, fs_zip_file_info* pInfo)
{
    fs_result result;
    size_t iFile;

    FS_ZIP_ASSERT(pZip      != NULL);
    FS_ZIP_ASSERT(pFilePath != NULL);
    FS_ZIP_ASSERT(pInfo     != NULL);

    result = fs_zip_find_file_by_path(pZip, pAllocationCallbacks, pFilePath, filePathLen, &iFile);
    if (result != FS_SUCCESS) {
        return result;  /* Most likely the file could not be found. */
    }

    return fs_zip_get_file_info_by_record_offset(pZip, pZip->pIndex[iFile].offsetInBytes, pInfo);
}

static int fs_zip_qsort_compare(void* pUserData, const void* a, const void* b)
{
    fs_zip* pZip = (fs_zip*)pUserData;
    const fs_zip_index* pZipIndex0 = (const fs_zip_index*)a;
    const fs_zip_index* pZipIndex1 = (const fs_zip_index*)b;
    size_t fileNameLen0;
    const char* pFileName0;
    size_t fileNameLen1;
    const char* pFileName1;
    int compareResult;

    FS_ZIP_ASSERT(pZip != NULL);

    pFileName0 = fs_zip_get_file_path_by_record_offset(pZip, pZipIndex0->offsetInBytes, &fileNameLen0);
    if (pFileName0 == NULL) {
        pFileName0 = "";    /* File couldn't be found. Just treat it as an empty string. */
    }

    pFileName1 = fs_zip_get_file_path_by_record_offset(pZip, pZipIndex1->offsetInBytes, &fileNameLen1);
    if (pFileName1 == NULL) {
        pFileName1 = "";    /* File couldn't be found. Just treat it as an empty string. */
    }

    compareResult = strncmp(pFileName0, pFileName1, FS_ZIP_MIN(fileNameLen0, fileNameLen1));
    if (compareResult == 0 && fileNameLen0 != fileNameLen1) {
        /* The strings are the same up to the length of the shorter string. The shorter string is considered to be less than the longer string. */
        compareResult = (fileNameLen0 < fileNameLen1) ? -1 : 1;
    }

    return compareResult;
}

static void fs_zip_cd_node_build(fs_zip* pZip, fs_zip_cd_node** ppRunningChildrenPointer, fs_zip_cd_node* pNode)
{
    size_t iFile;
    size_t iChild;

    FS_ZIP_ASSERT(pZip  != NULL);
    FS_ZIP_ASSERT(pNode != NULL);
    FS_ZIP_ASSERT(pNode->_descendantRangeEnd <= pZip->fileCount);
    FS_ZIP_ASSERT(ppRunningChildrenPointer != NULL);

    pNode->childCount = 0;
    pNode->pChildren  = *ppRunningChildrenPointer;

    /*
    We need to loop through our file range and add any direct children first. Then once that's
    done, we iterate over each child node and fill them out recursively.
    */
    for (iFile = pNode->_descendantRangeBeg; iFile < pNode->_descendantRangeEnd; iFile += 1) {
        const char* pFullFilePath;
        size_t fullFilePathLen;
        const char* pShortFilePath;
        size_t shortFilePathLen;
        fs_path_iterator shortFilePathIterator;

        pFullFilePath = fs_zip_get_file_path_by_record_offset(pZip, pZip->pIndex[iFile].offsetInBytes, &fullFilePathLen);
        if (pFullFilePath == NULL) {
            continue;   /* Should never happen. Just ignore the file if we couldn't find it by the given offset. */
        }

        /* If the full file path length is equal to the descendant prefix length, it means it's a leaf node. */
        FS_ZIP_ASSERT(fullFilePathLen >= pNode->_descendantPrefixLen);
        if (fullFilePathLen == pNode->_descendantPrefixLen) {
            continue;
        }

        /* The short file path is simply the full file path without the descendant prefix. */
        pShortFilePath   = pFullFilePath   + pNode->_descendantPrefixLen;
        shortFilePathLen = fullFilePathLen - pNode->_descendantPrefixLen;

        /* Make sure we're not sitting on a path separator. */
        if (pShortFilePath[0] == '\\' || pShortFilePath[0] == '/') {
            pShortFilePath   += 1;
            shortFilePathLen -= 1;
        }

        /*
        Now we need to check if we need to add a child. Because this main file listing is sorted,
        we need only check the last child item. If it's not equal, we have a new child item.
        */
        if (fs_path_first(pShortFilePath, shortFilePathLen, &shortFilePathIterator) == FS_SUCCESS) {
            if (pNode->childCount == 0 || pNode->pChildren[pNode->childCount-1].nameLen != shortFilePathIterator.segmentLength || strncmp(pNode->pChildren[pNode->childCount-1].pName, shortFilePathIterator.pFullPath + shortFilePathIterator.segmentOffset, FS_ZIP_MIN(pNode->pChildren[pNode->childCount-1].nameLen, shortFilePathIterator.segmentLength)) != 0) {
                /* Child doesn't exist. Need to add it to the list. */
                pNode->pChildren[pNode->childCount].iFile                = iFile;
                pNode->pChildren[pNode->childCount].pName                = shortFilePathIterator.pFullPath + shortFilePathIterator.segmentOffset;
                pNode->pChildren[pNode->childCount].nameLen              = shortFilePathIterator.segmentLength;
                pNode->pChildren[pNode->childCount]._descendantRangeBeg  = iFile;
                pNode->pChildren[pNode->childCount]._descendantRangeEnd  = pNode->_descendantRangeEnd;
                pNode->pChildren[pNode->childCount]._descendantPrefixLen = (fullFilePathLen - shortFilePathLen) + shortFilePathIterator.segmentLength;

                /* Update the end range for the previous child if we have one. */
                if (pNode->childCount > 0) {
                    pNode->pChildren[pNode->childCount-1]._descendantRangeEnd = iFile;
                }

                pNode->childCount         += 1;
                *ppRunningChildrenPointer += 1;
            }
        } else {
            /*
            Couldn't get the first segment. This probably means we found an explicit directory
            listing. We just ignore it.
            */
            if (pNode->childCount > 0) {
                pNode->pChildren[pNode->childCount-1]._descendantRangeEnd = iFile;
            }
        }
    }

    /* We've initialized each of the child nodes. We now need to recursively process them. */
    for (iChild = 0; iChild < pNode->childCount; iChild += 1) {
        fs_zip_cd_node_build(pZip, ppRunningChildrenPointer, &pNode->pChildren[iChild]);
    }
}


static fs_result fs_init_zip(fs* pFS, const void* pBackendConfig, fs_stream* pStream)
{
    fs_zip* pZip;
    
    fs_result result;
    fs_uint32 sig;
    int eocdPositionFromEnd;
    fs_uint16 cdRecordCount16;
    fs_uint64 cdRecordCount64;
    fs_uint32 cdSizeInBytes32;
    fs_uint64 cdSizeInBytes64;
    fs_uint32 cdOffset32;
    fs_uint64 cdOffset64;

    /* No need for a backend config. Maybe use this later for passwords if we ever add support for that? */
    (void)pBackendConfig;

    if (pStream == NULL) {
        return FS_INVALID_OPERATION;    /* Most likely the FS is being opened without a stream. */
    }
    
    pZip = (fs_zip*)fs_get_backend_data(pFS);
    FS_ZIP_ASSERT(pZip != NULL);
    
    /*
    The correct way to load a Zip file is to read from the central directory. The end of the
    central directory is the first thing we need to find and is sitting at the end of the file. The
    most efficient way to find this is to look for the end of central directory signature. The
    EOCD record is at least 22 bytes, but may be larger if there is a comment. The maximum size
    will be 22 + 65535.

    The way we'll do it is we'll first assume there is no comment and try reading from byte -22
    starting from the end. If the first 4 bytes are equal to the EOCD signature we'll treat that as
    the start of the EOCD and read from there. If this fails it probably means there's a comment in
    which case we'll go to byte -(22 + 65535) and scan from there.
    */
    result = fs_stream_seek(pStream, -22, FS_SEEK_END);
    if (result != FS_SUCCESS) {
        return result;  /* Failed to seek to our EOCD. This cannot be a valid Zip file. */
    }
    
    result = fs_stream_read(pStream, &sig, 4, NULL);
    if (result != FS_SUCCESS) {
        return result;
    }

    if (sig == FS_ZIP_EOCD_SIGNATURE) {
        /* Found the EOCD. It's at position -22. */
        eocdPositionFromEnd = -22;
    } else {
        /*
        The EOCD record is not located at position -22. There might be a comment which means the
        EOCD signature is sitting further up the file. The comment has a maximum if 65535
        characters, so we'll start searching from -(22 + 65535).
        */
        result = fs_stream_seek(pStream, -(22 + 65535), FS_SEEK_END);
        if (result != FS_SUCCESS) {
            /*
            We failed the seek, but it most likely means we were just trying to seek to far back in
            which case we can just fall back to a seek to position 0.
            */
            result = fs_stream_seek(pStream, 0, FS_SEEK_SET);
            if (result != FS_SUCCESS) {
                return result;
            }
        }

        /*
        We now need to scan byte-by-byte until we find the signature. We could allocate this on the
        stack, but that takes a little bit too much stack space than I feel comfortable with. We
        could also allocate a buffer on the heap, but that's just needlessly inefficient. Instead
        we'll run in a loop and read into a 4K stack allocated buffer.
        */
        {
            unsigned char buffer[4096];
            size_t bufferCursor;
            size_t bufferSize;
            size_t totalBytesScanned;

            totalBytesScanned = 0;
            bufferCursor = 0;
            for (;;) {
                size_t bufferRemaining;
                fs_bool32 foundEOCD = FS_FALSE;

                result = fs_stream_read(pStream, buffer + bufferCursor, sizeof(buffer) - bufferCursor, &bufferSize);
                if (result != FS_SUCCESS) {
                    return FS_INVALID_FILE;  /* If we get here it most likely means we've reached the end of the file. In any case, we're can't continue. */
                }

                /* Make sure we account for the offset when determining the buffer size. */
                bufferSize += bufferCursor;
                if (bufferSize < 4) {
                    return FS_INVALID_FILE;  /* Didn't read enough data. Not even enough to read a full signature. */
                }

                for (; bufferCursor <= (bufferSize - 4); bufferCursor += 1) {
                    /* Is it safe to do unaligned access like this on all platforms? Safer to do a byte-by-byte comparison? */
                    if (*(fs_uint32*)(buffer + bufferCursor) == FS_ZIP_EOCD_SIGNATURE) {
                        /* The signature has been found. */
                        foundEOCD = FS_TRUE;
                        break;
                    }
                }

                totalBytesScanned += bufferCursor;

                if (foundEOCD) {
                    /*
                    We found the EOCD. A complication here is that the stream's cursor won't
                    be sitting in the correct location because we were reading in chunks.
                    */
                    eocdPositionFromEnd = -(22 + 65535) + (int)totalBytesScanned;   /* Safe cast to int. */

                    result = fs_stream_seek(pStream, eocdPositionFromEnd + 4, FS_SEEK_END);  /* +4 so go just past the signatures. */
                    if (result != FS_SUCCESS) {
                        return result;
                    }

                    /* Just setting the signature here to keep the state of our local variables consistent. */
                    sig = FS_ZIP_EOCD_SIGNATURE;

                    /* Get out of the chunk loop. */
                    break;
                }

                /*
                Getting here means we didn't find the signature in this chunk. We need to move the
                cursor back and read another chunk.
                */
                bufferRemaining = bufferSize - bufferCursor;
                FS_ZIP_MOVE_MEMORY(buffer, buffer + bufferCursor, bufferRemaining);
                bufferCursor = bufferRemaining;
            }
        }
    }

    /*
    Getting here means we must have found the EOCD record. We can now parse it. The EOCD will give
    us information that we could use to determine if it's a Zip64 record.

    We're ignoring the disk properties. Split Zip files are not being supported here.
    */
    result = fs_stream_seek(pStream, 2 + 2 + 2, FS_SEEK_CUR);  /* Skip past disk stuff. */
    if (result != FS_SUCCESS) {
        return FS_INVALID_FILE;
    }

    result = fs_stream_read(pStream, &cdRecordCount16, 2, NULL);
    if (result != FS_SUCCESS) {
        return FS_INVALID_FILE;
    }

    result = fs_stream_read(pStream, &cdSizeInBytes32, 4, NULL);
    if (result != FS_SUCCESS) {
        return FS_INVALID_FILE;
    }

    result = fs_stream_read(pStream, &cdOffset32, 4, NULL);
    if (result != FS_SUCCESS) {
        return FS_INVALID_FILE;
    }

    /*
    The last part will be the comment. We don't care about this, and this is the last part of the
    file so we just leave it.
    */

    /*
    We'll now need to decide if we need to read some Zip64 information. To determine this we just
    need to look at the content of the record count, size and offset.
    */
    if (cdRecordCount16 == 0xFFFF || cdSizeInBytes32 == 0xFFFFFFFF || cdOffset32 == 0xFFFFFFFF) {
        /*
        It's a Zip64 file. We need to find the EOCD64 locator which will be 20 bytes before the EOCD
        that we just read. If we can't find the EOCD64 locator we'll just abort.
        */
        fs_uint64 eocd64SizeInBytes;
        fs_int64 eocd64OffsetInBytes;

        result = fs_stream_seek(pStream, eocdPositionFromEnd - 20, FS_SEEK_END);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        result = fs_stream_read(pStream, &sig, 4, NULL);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        if (sig != FS_ZIP_EOCD64_LOCATOR_SIGNATURE) {
            /* TODO: We can try falling back to a method that scans for the EOCD64. Would work just like the regular EOCD that we just read. */
            return FS_INVALID_FILE;  /* Couldn't find the EOCD64 locator. Abort. */
        }

        /* We don't use the next 4 bytes so skip it. */
        result = fs_stream_seek(pStream, 4, FS_SEEK_CUR);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        /* The next 8 bytes contains the offset to the EOCD64. */
        result = fs_stream_read(pStream, &eocd64OffsetInBytes, 8, NULL);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        /*
        The next 4 bytes contains the number of disks. We're not supporting split Zip files, so we
        don't need to care about this. Just seek straight to the EOCD64 record.
        */
        result = fs_stream_seek(pStream, eocd64OffsetInBytes, FS_SEEK_SET);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }


        /* Getting here means we've found the EOCD64. We can now parse it. */
        result = fs_stream_read(pStream, &sig, 4, NULL);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        if (sig != FS_ZIP_EOCD64_SIGNATURE) {
            return FS_INVALID_FILE;  /* Couldn't find the EOCD64. Abort. */
        }

        /* Getting here means we've found the EOCD64 locator. The next 8 bytes contains the size of the EOCD64 minus 12. */
        result = fs_stream_read(pStream, &eocd64SizeInBytes, 8, NULL);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        /* The EOCD64 must be at least 36 bytes. */
        if (eocd64SizeInBytes < 36) {
            return FS_INVALID_FILE;
        }

        /* We can skip past everything up to the record count, which is 20 bytes. */
        result = fs_stream_seek(pStream, 20, FS_SEEK_CUR);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        /* The next three items are the record count, size in bytes and offset, which are all 8 bytes. */
        result = fs_stream_read(pStream, &cdRecordCount64, 8, NULL);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        result = fs_stream_read(pStream, &cdSizeInBytes64, 8, NULL);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }

        result = fs_stream_read(pStream, &cdOffset64, 8, NULL);
        if (result != FS_SUCCESS) {
            return FS_INVALID_FILE;
        }
        
        if (cdRecordCount64 > FS_SIZE_MAX) {
            return FS_TOO_BIG;  /* Too many records. Will never fit what we need in memory. */
        }
        if (cdSizeInBytes64 > FS_SIZE_MAX) {
            return FS_TOO_BIG;  /* Central directory is too big to fit into memory. */
        }

        pZip->fileCount = (size_t)cdRecordCount64;  /* Safe cast. Checked above. */
        
    } else {
        /* It's a 32-bit archive. */
        pZip->fileCount = cdRecordCount16;
        pZip->centralDirectorySize = cdSizeInBytes32;

        cdSizeInBytes64 = cdSizeInBytes32;
        cdOffset64 = cdOffset32;
    }

    /* We need to seek to the start of the central directory and read it's contents. */
    result = fs_stream_seek(pStream, cdOffset64, FS_SEEK_SET);
    if (result != FS_SUCCESS) {
        return FS_INVALID_FILE;
    }

    /* At this point we'll be sitting on the central directory. */

    /*
    We don't parse the central directory here. Instead we just allocate a block of memory and read
    straight into that. Then when we need to load a file we just iterate over the central
    directory.
    */
    if (cdSizeInBytes64 > FS_SIZE_MAX) {
        return FS_TOO_BIG;   /* The size of the central directory is too big. */
    }

    pZip->centralDirectorySize = (size_t)cdSizeInBytes64;


    pZip->pHeap = fs_malloc(FS_ZIP_ALIGN(pZip->centralDirectorySize, FS_SIZEOF_PTR) + (sizeof(*pZip->pIndex) * pZip->fileCount), fs_get_allocation_callbacks(pFS));
    if (pZip->pHeap == NULL) {
        return FS_OUT_OF_MEMORY;
    }

    pZip->pCentralDirectory =                FS_ZIP_OFFSET_PTR(pZip->pHeap, 0);
    pZip->pIndex            = (fs_zip_index*)FS_ZIP_OFFSET_PTR(pZip->pHeap, FS_ZIP_ALIGN(pZip->centralDirectorySize, FS_SIZEOF_PTR));
    pZip->pCDRootNode       = NULL; /* <-- This will be set later. */

    result = fs_stream_read(pStream, pZip->pCentralDirectory, pZip->centralDirectorySize, NULL);
    if (result != FS_SUCCESS) {
        return FS_INVALID_FILE;
    }

    /* Build the index. It needs to be sorted by name. We'll treat this as case-sensitive. */
    {
        fs_memory_stream cdStream;
        size_t iFile;
        size_t nodeUpperBoundCount;

        result = fs_memory_stream_init_readonly(pZip->pCentralDirectory, pZip->centralDirectorySize, &cdStream);
        if (result != FS_SUCCESS) {
            fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
            return result;
        }

        for (iFile = 0; iFile < pZip->fileCount; iFile += 1) {
            size_t fileOffset;
            fs_uint16 fileNameLen;
            fs_uint16 extraLen;
            fs_uint16 commentLen;

            result = fs_memory_stream_tell(&cdStream, &fileOffset);
            if (result != FS_SUCCESS) {
                fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
                return result;
            }

            pZip->pIndex[iFile].offsetInBytes = fileOffset;


            /*
            We need to seek to the next item. To do this we need to retrieve the lengths of the
            variable-length fields. These start from offset 28.
            */
            result = fs_memory_stream_seek(&cdStream, 28, FS_SEEK_CUR);
            if (result != FS_SUCCESS) {
                fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
                return result;
            }

            result = fs_memory_stream_read(&cdStream, &fileNameLen, 2, NULL);
            if (result != FS_SUCCESS) {
                fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
                return result;
            }

            result = fs_memory_stream_read(&cdStream, &extraLen, 2, NULL);
            if (result != FS_SUCCESS) {
                fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
                return result;
            }

            result = fs_memory_stream_read(&cdStream, &commentLen, 2, NULL);
            if (result != FS_SUCCESS) {
                fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
                return result;
            }

            /* We have the necessary information we need to move past this record. */
            result = fs_memory_stream_seek(&cdStream, fileOffset + 46 + fileNameLen + extraLen + commentLen, FS_SEEK_SET);
            if (result != FS_SUCCESS) {
                fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
                return result;
            }
        }

        /*
        TODO: Look at some real-world Zip archives from various archivers (7zip, Windows, etc.) and
        check how the sorting looks before our explicit sort. If most real-world archives are already
        mostly sorted, it might be more efficient to just do a simple insertion sort.
        */
        fs_sort(pZip->pIndex, pZip->fileCount, sizeof(fs_zip_index), fs_zip_qsort_compare, pZip);

        /* Testing. */
        #if 0
        {
            size_t i;
            for (i = 0; i < pZip->fileCount; i += 1) {
                size_t nameLen;
                const char* pName = fs_zip_get_file_path_by_record_offset(pZip, pZip->pIndex[i].offsetInBytes, &nameLen);

                printf("File name = %.*s\n", (int)nameLen, pName);
            }
        }
        #endif

        /*
        We're going to build an accelerated data structure for the central directory. Nothing over
        the top - just a simple tree based on directory names.

        It's just a graph. Each node in the graph is either a directory or a file. Leaf nodes can
        possibly be files or an empty directory which means a flag is required to indicate whether
        or not the node is a directory. Sub-folders and files are just child nodes. Children are
        sorted by name to allow for fast lookups.

        The items in the central directory has already been sorted thanks to the index that we
        constructed above. If we just iterate linearly based on that index everything should be
        sorted naturally.

        The graph is constructed in two passes. The first pass simply counts the number of nodes so
        we can allocate a single block of memory. The second pass fills the data.
        */

        /*
        The first pass is just to count the number of nodes so we can allocate some memory in one
        chunk. We start the count at one to accommodate for the root node. This pass is not
        necessarily calculating an exact count, but instead it calculates an upper bound count. The
        reason for this is how directories are handled. Sometimes they are listed explicitly, but I
        have seen cases where they're not. If we could guarantee all folders were explicitly listed
        we would be able to avoid this pass.

        We can take advantage of the fact that the file listing has been sorted. For each entry we
        just compare the path with the previous one, and for every segment in the new path that's
        different we increment the counter (it should always be at least one since the file name
        itself should always be different).
        */
        {
            const char* pPrevPath;
            size_t prevPathLen;
            
            /* Consider the root directory to be the previous path. */
            pPrevPath = "";
            prevPathLen = 0;

            /* Start the count at 1 to account for the root node. */
            nodeUpperBoundCount = 1;

            for (iFile = 0; iFile < pZip->fileCount; iFile += 1) {
                const char* pFilePath;
                size_t filePathLen;

                pFilePath = fs_zip_get_file_path_by_record_offset(pZip, pZip->pIndex[iFile].offsetInBytes, &filePathLen);
                if (pFilePath == NULL) {
                    continue;   /* Just skip the file if we can't get the name. Should never happen. */
                }

                /*
                Now that we have the file path all we need to do is compare is to the previous path
                and increment the counter for every segment in the current path that is different
                to the previous path. We'll need to use a path iterator for each of these.
                */
                {
                    fs_path_iterator nextIterator;
                    fs_path_iterator prevIterator;

                    fs_path_first(pFilePath, filePathLen, &nextIterator);   /* <-- This should never fail. */

                    if (fs_path_first(pPrevPath, prevPathLen, &prevIterator) == FS_SUCCESS) {
                        /*
                        First just move the next iterator forward until we reach the end of the previous
                        iterator, or if the segments differ between the two.
                        */
                        for (;;) {
                            if (fs_path_iterators_compare(&nextIterator, &prevIterator) != 0) {
                                break;  /* Iterators don't match. */
                            }

                            /* Getting here means the segments match. We need to move to the next one. */
                            if (fs_path_next(&nextIterator) != FS_SUCCESS) {
                                break;  /* We reached the end of the next iterator before the previous. The only difference will be the file name. */
                            }

                            if (fs_path_next(&prevIterator) != FS_SUCCESS) {
                                break;  /* We reached the end of the prev iterator. Get out of the loop. */
                            }
                        }
                    }

                    /* Increment the counter to account for the segment that the next iterator is currently sitting on. */
                    nodeUpperBoundCount += 1;

                    /* Now we need to increment the counter for every new segment. */
                    while (fs_path_next(&nextIterator) == FS_SUCCESS) {
                        nodeUpperBoundCount += 1;
                    }
                }

                /* Getting here means we're done with the count for this item. Move to the next one. */
                pPrevPath = pFilePath;
                prevPathLen = filePathLen;
            }
        }

        /*
        Now that we've got the count we can go ahead and resize our heap allocation. It's important
        to remember to update our pointers here.
        */
        {
            void* pNewHeap = fs_realloc(pZip->pHeap, FS_ZIP_ALIGN(pZip->centralDirectorySize, FS_SIZEOF_PTR) + (sizeof(*pZip->pIndex) * pZip->fileCount) + (sizeof(*pZip->pCDRootNode) * nodeUpperBoundCount), fs_get_allocation_callbacks(pFS));
            if (pNewHeap == NULL) {
                fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
                return FS_OUT_OF_MEMORY;
            }

            pZip->pHeap = pNewHeap;
            pZip->pCentralDirectory =                  FS_ZIP_OFFSET_PTR(pZip->pHeap, 0);
            pZip->pIndex            = (fs_zip_index*  )FS_ZIP_OFFSET_PTR(pZip->pHeap, FS_ZIP_ALIGN(pZip->centralDirectorySize, FS_SIZEOF_PTR));
            pZip->pCDRootNode       = (fs_zip_cd_node*)FS_ZIP_OFFSET_PTR(pZip->pHeap, FS_ZIP_ALIGN(pZip->centralDirectorySize, FS_SIZEOF_PTR) + (sizeof(*pZip->pIndex) * pZip->fileCount));
        }

        /*
        Memory has been allocated so we can now fill it out. This is slightly tricky because we want
        to do it in a single pass with a single memory allocation. Each node will hold a pointer to
        an array which will contain their children. The size of this array is unknown at this point
        so we need to come up with a system that allows us to fill each node in order.

        Fortunately our file listing is sorted which gives us a good start. We want to fill out
        higher level nodes first and then move down to leaf nodes. We're going to run through the
        file listing in sorted order. For the current file path, we need to look at it's directory
        structure. For each segment of the directory there will be a node. For each of these
        segments we'll run an inner loop that adds child nodes for each file that shares the same
        prefix.

        To put simply, for each node, we need to attach all of it's children before the child nodes
        themselves have been filled with their children. We can do this recursively. The first node
        we're filling is the root node.
        */
        {
            fs_zip_cd_node* pRunningChildrenPointer = &pZip->pCDRootNode[1];

            /* The root node needs to be set up first. */
            pZip->pCDRootNode->pName                = "";
            pZip->pCDRootNode->nameLen              = 0;
            pZip->pCDRootNode->_descendantRangeBeg  = 0;
            pZip->pCDRootNode->_descendantRangeEnd  = pZip->fileCount;
            pZip->pCDRootNode->_descendantPrefixLen = 0;

            fs_zip_cd_node_build(pZip, &pRunningChildrenPointer, pZip->pCDRootNode);
        }
    }

    return FS_SUCCESS;
}

static void fs_uninit_zip(fs* pFS)
{
    fs_zip* pZip = (fs_zip*)fs_get_backend_data(pFS);
    FS_ZIP_ASSERT(pZip != NULL);

    fs_free(pZip->pHeap, fs_get_allocation_callbacks(pFS));
    return;
}

static fs_result fs_ioctl_zip(fs* pFS, int op, void* pArg)
{
    fs_zip* pZip = (fs_zip*)fs_get_backend_data(pFS);
    FS_ZIP_ASSERT(pZip != NULL);

    (void)pZip;
    (void)op;
    (void)pArg;

    return FS_NOT_IMPLEMENTED;
}

static fs_result fs_info_zip(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo)
{
    fs_result result;
    fs_zip* pZip;
    fs_zip_file_info info;

    (void)openMode;
    
    pZip = (fs_zip*)fs_get_backend_data(pFS);
    FS_ZIP_ASSERT(pZip != NULL);

    result = fs_zip_get_file_info_by_path(pZip, fs_get_allocation_callbacks(pFS), pPath, (size_t)-1, &info);
    if (result != FS_SUCCESS) {
        return result;  /* Probably not found. */
    }

    pInfo->size      = info.uncompressedSize;
    pInfo->directory = info.directory;

    return FS_SUCCESS;
}


typedef struct fs_iterator_zip
{
    fs_iterator iterator;
    fs_zip* pZip;
    fs_zip_cd_node* pDirectoryNode;
    size_t iChild;
} fs_iterator_zip;

typedef struct fs_file_zip
{
    fs_stream* pStream;                       /* Duplicated from the main file system stream. Freed with fs_stream_delete_duplicate(). */
    fs_zip_file_info info;
    fs_uint64 absoluteCursorUncompressed;
    fs_uint64 absoluteCursorCompressed;         /* The position of the cursor in the compressed data. */
    fs_zip_deflate_decompressor decompressor; /* Only used for compressed files. */
    size_t cacheCap;                            /* The capacity of the cache. Never changes. */
    size_t cacheSize;                           /* The number of valid bytes in the cache. Can be less than the capacity, but never more. Will be less when holding the tail end fo the file data. */
    size_t cacheCursor;                         /* The cursor within the cache. The cache size minus the cursor defines how much data remains in the cache. */
    unsigned char* pCache;                      /* Cache must be at least 32K. Stores uncompressed data. Stored at the end of the struct. */
    size_t compressedCacheCap;                  /* The capacity of the compressed cache. Never changes. */
    size_t compressedCacheSize;                 /* The number of valid bytes in the compressed cache. Can be less than the capacity, but never more. Will be less when holding the tail end fo the file data. */
    size_t compressedCacheCursor;               /* The cursor within the compressed cache. The compressed cache size minus the cursor defines how much data remains in the compressed cache. */
    unsigned char* pCompressedCache;            /* Only used for compressed files. */
} fs_file_zip;

static size_t fs_file_alloc_size_zip(fs* pFS)
{
    (void)pFS;
    return sizeof(fs_file_zip) + FS_ZIP_CACHE_SIZE_IN_BYTES + FS_ZIP_COMPRESSED_CACHE_SIZE_IN_BYTES;
}

static fs_result fs_file_open_zip(fs* pFS, fs_stream* pStream, const char* pPath, int openMode, fs_file* pFile)
{
    fs_zip* pZip;
    fs_file_zip* pZipFile;
    fs_result result;

    pZip = (fs_zip*)fs_get_backend_data(pFS);
    FS_ZIP_ASSERT(pZip != NULL);

    pZipFile = (fs_file_zip*)fs_file_get_backend_data(pFile);
    FS_ZIP_ASSERT(pZipFile != NULL);

    /* Write mode is currently unsupported. */
    if ((openMode & FS_WRITE) != 0) {
        return FS_INVALID_OPERATION;
    }

    pZipFile->pStream = pStream;

    /* We need to find the file info by it's path. */
    result = fs_zip_get_file_info_by_path(pZip, fs_get_allocation_callbacks(pFS), pPath, (size_t)-1, &pZipFile->info);
    if (result != FS_SUCCESS) {
        return result;  /* Probably not found. */
    }

    /* We can't be trying to open a directory. */
    if (pZipFile->info.directory) {
        return FS_IS_DIRECTORY;
    }

    /* Validate the compression method. We're only supporting Store and Deflate. */
    if (pZipFile->info.compressionMethod != FS_ZIP_COMPRESSION_METHOD_STORE && pZipFile->info.compressionMethod != FS_ZIP_COMPRESSION_METHOD_DEFLATE) {
        return FS_INVALID_FILE;
    }

    /* Make double sure the cursor is at the start. */
    pZipFile->absoluteCursorUncompressed = 0;
    pZipFile->cacheCap = FS_ZIP_CACHE_SIZE_IN_BYTES;

    /*
    We allocated memory for a compressed cache, even when the file is not compressed. Make use
    of this memory if the file is not compressed.
    */
    if (pZipFile->info.compressionMethod == FS_ZIP_COMPRESSION_METHOD_STORE) {
        pZipFile->cacheCap          += FS_ZIP_COMPRESSED_CACHE_SIZE_IN_BYTES;
        pZipFile->compressedCacheCap = 0;
    } else {
        pZipFile->compressedCacheCap = FS_ZIP_COMPRESSED_CACHE_SIZE_IN_BYTES;
    }

    pZipFile->pCache           = (unsigned char*)FS_ZIP_OFFSET_PTR(pZipFile, sizeof(fs_file_zip));
    pZipFile->pCompressedCache = (unsigned char*)FS_ZIP_OFFSET_PTR(pZipFile, sizeof(fs_file_zip) + pZipFile->cacheCap);

    /*
    We need to move the file offset forward so that it's pointing to the first byte of the actual
    data. It's currently sitting at the top of the local header which isn't really useful for us.
    To move forward we need to get the length of the file path and the extra data and seek past
    the local header.
    */
    {
        fs_uint16 fileNameLen;
        fs_uint16 extraLen;

        result = fs_stream_seek(pZipFile->pStream, pZipFile->info.fileOffset + 26, FS_SEEK_SET);
        if (result != FS_SUCCESS) {
            return result;
        }

        result = fs_stream_read(pZipFile->pStream, &fileNameLen, 2, NULL);
        if (result != FS_SUCCESS) {
            return result;
        }

        result = fs_stream_read(pZipFile->pStream, &extraLen, 2, NULL);
        if (result != FS_SUCCESS) {
            return result;
        }

        pZipFile->info.fileOffset += (fs_uint32)30 + fileNameLen + extraLen;
    }


    /* Initialize the decompressor if necessary. */
    if (pZipFile->info.compressionMethod == FS_ZIP_COMPRESSION_METHOD_DEFLATE) {
        result = fs_zip_deflate_decompressor_init(&pZipFile->decompressor);
        if (result != FS_SUCCESS) {
            return result;
        }
    }


    return FS_SUCCESS;
}

static fs_result fs_file_open_handle_zip(fs* pFS, void* hBackendFile, fs_file* pFile)
{
    (void)pFS;
    (void)hBackendFile;
    (void)pFile;

    return FS_NOT_IMPLEMENTED;
}

static void fs_file_close_zip(fs_file* pFile)
{
    /* Nothing to do. */
    (void)pFile;
}


static fs_result fs_file_read_zip_store(fs* pFS, fs_file_zip* pZipFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_result result;
    fs_uint64 bytesRemainingInFile;
    size_t bytesRead;

    FS_ZIP_ASSERT(pZipFile   != NULL);
    FS_ZIP_ASSERT(pBytesRead != NULL);

    (void)pFS;

    bytesRemainingInFile = pZipFile->info.uncompressedSize - pZipFile->absoluteCursorUncompressed;
    if (bytesRemainingInFile == 0) {
        return FS_AT_END;   /* Nothing left to read. Must return FS_AT_END. */
    }

    if (bytesToRead > bytesRemainingInFile) {
        bytesToRead = (size_t)bytesRemainingInFile;
    }

    bytesRead = 0;

    /* Read from the cache first. */
    {
        size_t bytesRemainingInCache = pZipFile->cacheSize - pZipFile->cacheCursor;
        size_t bytesToReadFromCache = bytesToRead;
        if (bytesToReadFromCache > bytesRemainingInCache) {
            bytesToReadFromCache = bytesRemainingInCache;
        }

        FS_ZIP_COPY_MEMORY(pDst, pZipFile->pCache + pZipFile->cacheCursor, bytesToReadFromCache);
        pZipFile->cacheCursor += bytesToReadFromCache;

        bytesRead = bytesToReadFromCache;
    }

    if (bytesRead < bytesToRead) {
        /*
        There's more data to read. If there's more data remaining than the cache capacity, we
        simply load some data straight into the output buffer. Any remainder we load into the
        cache and then read from that.
        */
        size_t bytesRemainingToRead = bytesToRead - bytesRead;
        size_t bytesToReadFromArchive;

        result = fs_stream_seek(pZipFile->pStream, pZipFile->info.fileOffset + (pZipFile->absoluteCursorUncompressed + bytesRead), FS_SEEK_SET);
        if (result != FS_SUCCESS) {
            return result;
        }

        if (bytesRemainingToRead > pZipFile->cacheCap) {
            size_t bytesReadFromArchive;

            bytesToReadFromArchive = (bytesRemainingToRead / pZipFile->cacheCap) * pZipFile->cacheCap;

            result = fs_stream_read(pZipFile->pStream, FS_ZIP_OFFSET_PTR(pDst, bytesRead), bytesToReadFromArchive, &bytesReadFromArchive);
            if (result != FS_SUCCESS) {
                return result;
            }

            bytesRead += bytesReadFromArchive;
            bytesRemainingToRead -= bytesReadFromArchive; 
        }

        /*
        At this point we should have less than the cache capacity remaining to read. We need to
        read into the cache, and then read any leftover from it.
        */
        if (bytesRemainingToRead > 0) {
            FS_ZIP_ASSERT(bytesRemainingToRead < pZipFile->cacheCap);

            result = fs_stream_read(pZipFile->pStream, pZipFile->pCache, (size_t)FS_ZIP_MIN(pZipFile->cacheCap, (pZipFile->info.uncompressedSize - (pZipFile->absoluteCursorUncompressed + bytesRead))), &pZipFile->cacheSize); /* Safe cast to size_t because reading will be clamped to bytesToRead. */
            if (result != FS_SUCCESS) {
                return result;
            }

            pZipFile->cacheCursor = 0;

            FS_ZIP_COPY_MEMORY(FS_ZIP_OFFSET_PTR(pDst, bytesRead), pZipFile->pCache + pZipFile->cacheCursor, bytesRemainingToRead);
            pZipFile->cacheCursor += bytesRemainingToRead;

            bytesRead += bytesRemainingToRead;
        }
    }

    pZipFile->absoluteCursorUncompressed += bytesRead;

    /* We're done. */
    *pBytesRead = bytesRead;
    return FS_SUCCESS;
}

static fs_result fs_file_read_zip_deflate(fs* pFS, fs_file_zip* pZipFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_result result;
    fs_uint64 uncompressedBytesRemainingInFile;
    size_t uncompressedBytesRead;

    FS_ZIP_ASSERT(pZipFile != NULL);
    FS_ZIP_ASSERT(pBytesRead != NULL);

    (void)pFS;

    uncompressedBytesRemainingInFile = pZipFile->info.uncompressedSize - pZipFile->absoluteCursorUncompressed;
    if (uncompressedBytesRemainingInFile == 0) {
        return FS_AT_END;   /* Nothing left to read. Must return FS_AT_END. */
    }

    if (bytesToRead > uncompressedBytesRemainingInFile) {
        bytesToRead = (size_t)uncompressedBytesRemainingInFile;
    }

    uncompressedBytesRead = 0;


    /*
    The way reading works for deflate is that we need to read from the cache until it's exhausted,
    and then refill it and read from it again. We need to do this until we've read the requested
    number of bytes.
    */
    for (;;) {
        /* Read from the cache first. */
        size_t bytesRemainingInCache = pZipFile->cacheSize - pZipFile->cacheCursor;
        size_t bytesToReadFromCache = bytesToRead - uncompressedBytesRead;
        if (bytesToReadFromCache > bytesRemainingInCache) {
            bytesToReadFromCache = bytesRemainingInCache;
        }

        FS_ZIP_COPY_MEMORY(FS_ZIP_OFFSET_PTR(pDst, uncompressedBytesRead), pZipFile->pCache + pZipFile->cacheCursor, bytesToReadFromCache);
        pZipFile->cacheCursor += bytesToReadFromCache;

        uncompressedBytesRead += bytesToReadFromCache;

        /* If we've read the requested number of bytes we can stop. */
        if (uncompressedBytesRead == bytesToRead) {
            break;
        }

        
        /*
        Getting here means we've exchausted the cache but still have more data to read. We now need
        to refill the cache and read from it again.

        This needs to be run in a loop because we may need to read multiple times to get enough input
        data to fill the entire output cache, which must be at least 32KB.
        */
        pZipFile->cacheCursor = 0;
        pZipFile->cacheSize   = 0;

        for (;;) {
            size_t compressedBytesRead;
            size_t compressedBytesToRead;
            int decompressFlags = FS_ZIP_DEFLATE_FLAG_HAS_MORE_INPUT;    /* The default stance is that we have more input available. */
            fs_result decompressResult;

            /* If we've already read the entire compressed file we need to set the flag to indicate there is no more input. */
            if (pZipFile->absoluteCursorCompressed == pZipFile->info.compressedSize) {
                decompressFlags &= ~FS_ZIP_DEFLATE_FLAG_HAS_MORE_INPUT;
            }

            /*
            We need only lock while we read the compressed data into our cache. We don't need to keep
            the archive locked while we do the decompression phase.

            We need only read more input data from the stream if we've run out of data in the
            compressed cache.
            */
            if (pZipFile->compressedCacheSize == 0) {
                FS_ZIP_ASSERT(pZipFile->compressedCacheCursor == 0); /* The cursor should never go past the size. */

                /* Make sure we're positioned correctly in the stream before we read. */
                result = fs_stream_seek(pZipFile->pStream, pZipFile->info.fileOffset + pZipFile->absoluteCursorCompressed, FS_SEEK_SET);
                if (result != FS_SUCCESS) {
                    return result;
                }

                /*
                Read the compressed data into the cache. The number of compressed bytes we read needs
                to be clamped to the number of bytes remaining in the file and the number of bytes
                remaining in the cache.
                */
                compressedBytesToRead = (size_t)FS_ZIP_MIN(pZipFile->compressedCacheCap - pZipFile->compressedCacheCursor, (pZipFile->info.compressedSize - pZipFile->absoluteCursorCompressed));

                result = fs_stream_read(pZipFile->pStream, pZipFile->pCompressedCache + pZipFile->compressedCacheCursor, compressedBytesToRead, &compressedBytesRead);
                /*
                We'll inspect the result later after we've escaped from the locked section just to
                keep the lock as small as possible.
                */

                pZipFile->absoluteCursorCompressed += compressedBytesRead;

                /* If we've reached the end of the compressed data, we need to set a flag which we later pass through to the decompressor. */
                if (result == FS_AT_END && compressedBytesRead < compressedBytesToRead) {
                    decompressFlags &= ~FS_ZIP_DEFLATE_FLAG_HAS_MORE_INPUT;
                }

                if (result != FS_SUCCESS && result != FS_AT_END) {
                    return result;  /* Failed to read the compressed data. */
                }

                pZipFile->compressedCacheSize += compressedBytesRead;
            }


            /*
            At this point we should have the compressed data. Here is where we decompress it into
            the cache. We need to set up a few parameters here. The input buffer needs to start from
            the current cursor position of the compressed cache. The input size is the number of
            bytes in the compressed cache between the cursor and the end of the cache. The output
            buffer is from the current cursor position.
            */
            {
                size_t inputBufferSize = pZipFile->compressedCacheSize - pZipFile->compressedCacheCursor;
                size_t outputBufferSize = pZipFile->cacheCap - pZipFile->cacheSize;

                decompressResult = fs_zip_deflate_decompress(&pZipFile->decompressor, pZipFile->pCompressedCache + pZipFile->compressedCacheCursor, &inputBufferSize, pZipFile->pCache, pZipFile->pCache + pZipFile->cacheSize, &outputBufferSize, decompressFlags);
                if (decompressResult < 0) {
                    return FS_ERROR; /* Failed to decompress the data. */
                }

                /* Move our input cursors forward since we've just consumed some input. */
                pZipFile->compressedCacheCursor += inputBufferSize;

                /* We've just generated some uncompressed data, so push out the size of the cache to accommodate it. */
                pZipFile->cacheSize += outputBufferSize;

                /*
                If the compressed cache has been fully exhausted we need to reset it so more data
                can be read from the stream.
                */
                if (pZipFile->compressedCacheCursor == pZipFile->compressedCacheSize) {
                    pZipFile->compressedCacheCursor = 0;
                    pZipFile->compressedCacheSize   = 0;
                }

                /*
                We need to inspect the result of the decompression to determine how to continue. If
                we've reached the end we need only break from the inner loop.
                */
                if (decompressResult == FS_NEEDS_MORE_INPUT) {
                    continue;   /* Do another round of reading and decompression. */
                } else {
                    break;      /* We've reached the end of the compressed data or the output buffer is full. */
                }
            }
        }
    }

    pZipFile->absoluteCursorUncompressed += uncompressedBytesRead;

    /* We're done. */
    *pBytesRead = uncompressedBytesRead;
    return FS_SUCCESS;
}

static fs_result fs_file_read_zip(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead)
{
    fs_file_zip* pZipFile;

    pZipFile = (fs_file_zip*)fs_file_get_backend_data(pFile);
    FS_ZIP_ASSERT(pZipFile != NULL);

    if (pZipFile->info.compressionMethod == FS_ZIP_COMPRESSION_METHOD_STORE) {
        return fs_file_read_zip_store(fs_file_get_fs(pFile), pZipFile, pDst, bytesToRead, pBytesRead);
    } else if (pZipFile->info.compressionMethod == FS_ZIP_COMPRESSION_METHOD_DEFLATE) {
        return fs_file_read_zip_deflate(fs_file_get_fs(pFile), pZipFile, pDst, bytesToRead, pBytesRead);
    } else {
        return FS_INVALID_FILE;  /* Should never get here. */
    }
}

static fs_result fs_file_write_zip(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten)
{
    /* Write not supported. */
    (void)pFile;
    (void)pSrc;
    (void)bytesToWrite;
    (void)pBytesWritten;
    return FS_NOT_IMPLEMENTED;
}

static fs_result fs_file_seek_zip(fs_file* pFile, fs_int64 offset, fs_seek_origin origin)
{
    fs_file_zip* pZipFile;
    fs_int64 newSeekTarget;
    fs_uint64 newAbsoluteCursor;

    pZipFile = (fs_file_zip*)fs_file_get_backend_data(pFile);
    FS_ZIP_ASSERT(pZipFile != NULL);

    if (origin == FS_SEEK_SET) {
        newSeekTarget = 0;
    } else if (origin == FS_SEEK_CUR) {
        newSeekTarget = pZipFile->absoluteCursorUncompressed;
    } else if (origin == FS_SEEK_END) {
        newSeekTarget = pZipFile->info.uncompressedSize;
    } else {
        FS_ZIP_ASSERT(!"Invalid seek origin.");
        return FS_INVALID_ARGS;
    }

    newSeekTarget += offset;
    if (newSeekTarget < 0) {
        return FS_BAD_SEEK;  /* Trying to seek before the start of the file. */
    }
    if ((fs_uint64)newSeekTarget > pZipFile->info.uncompressedSize) {
        return FS_BAD_SEEK;  /* Trying to seek beyond the end of the file. */
    }

    newAbsoluteCursor = (fs_uint64)newSeekTarget;

    /*
    We can do fast seeking if we are moving within the cache. Otherwise we just move the cursor and
    clear the cache. The next time we read, it'll see that the cache is empty which will trigger a
    fresh read of data from the archive stream.
    */
    if (newAbsoluteCursor > pZipFile->absoluteCursorUncompressed) {
        /* Moving forward. */
        fs_uint64 delta = newAbsoluteCursor - pZipFile->absoluteCursorUncompressed;
        if (delta <= (pZipFile->cacheSize - pZipFile->cacheCursor)) {
            pZipFile->cacheCursor += (size_t)delta; /* Safe cast. */
            pZipFile->absoluteCursorUncompressed = newAbsoluteCursor;
            return FS_SUCCESS;
        } else {
            /* Seeking beyond the cache. Fall through. */
        }
    } else {
        /* Moving backward. */
        fs_uint64 delta = pZipFile->absoluteCursorUncompressed - newAbsoluteCursor;
        if (delta <= pZipFile->cacheCursor) {
            pZipFile->cacheCursor -= (size_t)delta;
            pZipFile->absoluteCursorUncompressed = newAbsoluteCursor;
            return FS_SUCCESS;
        } else {
            /* Seeking beyond the cache. Fall through. */
        }
    }

    /* Getting here means we're seeking beyond the cache. Just clear it. The next read will read in fresh data. */
    pZipFile->cacheSize   = 0;
    pZipFile->cacheCursor = 0;

    /*
    Seeking is more complicated for compressed files. We need to actually read to the seek point.
    There is no seek table to accelerate this.
    */
    if (pZipFile->info.compressionMethod != FS_ZIP_COMPRESSION_METHOD_STORE) {
        pZipFile->compressedCacheCursor = 0;
        pZipFile->compressedCacheSize   = 0;

        /*
        When seeking backwards we need to move everything back to the start and then just
        read-and-discard until we reach the end.
        */
        if (pZipFile->absoluteCursorUncompressed > newAbsoluteCursor) {
            pZipFile->absoluteCursorUncompressed = 0;
            pZipFile->absoluteCursorCompressed   = 0;

            /* The decompressor needs to be reset. */
            fs_zip_deflate_decompressor_init(&pZipFile->decompressor);
        }

        /* Now we just keep reading until we get to the seek point. */
        while (pZipFile->absoluteCursorUncompressed < newAbsoluteCursor) {  /* <-- absoluteCursorUncompressed will be incremented by fs_file_read_zip(). */
            fs_uint8 temp[4096];
            fs_uint64 bytesToRead;
            size_t bytesRead;
            fs_result result;

            bytesToRead = newAbsoluteCursor - pZipFile->absoluteCursorUncompressed;
            if (bytesToRead > sizeof(temp)) {
                bytesToRead = sizeof(temp);
            }
            
            bytesRead = 0;
            result = fs_file_read_zip(pFile, temp, (size_t)bytesToRead, &bytesRead);    /* Safe cast to size_t because the bytes to read will be clamped to sizeof(temp). */
            if (result != FS_SUCCESS) {
                return result;
            }

            if (bytesRead == 0) {
                return FS_BAD_SEEK;  /* Trying to seek beyond the end of the file. */
            }
        }
    }

    /* Make sure the absolute cursor is set to the new position. */
    pZipFile->absoluteCursorUncompressed = newAbsoluteCursor;

    return FS_SUCCESS;
}

static fs_result fs_file_tell_zip(fs_file* pFile, fs_int64* pCursor)
{
    fs_file_zip* pZipFile = (fs_file_zip*)fs_file_get_backend_data(pFile);

    FS_ZIP_ASSERT(pZipFile != NULL);
    FS_ZIP_ASSERT(pCursor  != NULL);

    *pCursor = pZipFile->absoluteCursorUncompressed;
    return FS_SUCCESS;
}

static fs_result fs_file_flush_zip(fs_file* pFile)
{
    /* Nothing to do. */
    (void)pFile;
    return FS_SUCCESS;
}

static fs_result fs_file_info_zip(fs_file* pFile, fs_file_info* pInfo)
{
    fs_file_zip* pZipFile = (fs_file_zip*)fs_file_get_backend_data(pFile);

    FS_ZIP_ASSERT(pZipFile != NULL);
    FS_ZIP_ASSERT(pInfo    != NULL);

    pInfo->size      = pZipFile->info.uncompressedSize;
    pInfo->directory = FS_FALSE; /* An opened file should never be a directory. */
    
    return FS_SUCCESS;
}

static fs_result fs_file_duplicate_zip(fs_file* pFile, fs_file* pDuplicatedFile)
{
    fs_file_zip* pZipFile;
    fs_file_zip* pDuplicatedZipFile;

    pZipFile = (fs_file_zip*)fs_file_get_backend_data(pFile);
    FS_ZIP_ASSERT(pZipFile != NULL);

    pDuplicatedZipFile = (fs_file_zip*)fs_file_get_backend_data(pDuplicatedFile);
    FS_ZIP_ASSERT(pDuplicatedZipFile != NULL);

    /* We should be able to do this with a simple memcpy. */
    FS_ZIP_COPY_MEMORY(pDuplicatedZipFile, pZipFile, fs_file_alloc_size_zip(fs_file_get_fs(pFile)));

    return FS_SUCCESS;
}

static void fs_iterator_zip_init(fs_zip* pZip, fs_zip_cd_node* pChild, fs_iterator_zip* pIterator)
{
    fs_zip_file_info info;

    FS_ZIP_ASSERT(pIterator != NULL);
    FS_ZIP_ASSERT(pChild    != NULL);
    FS_ZIP_ASSERT(pZip      != NULL);

    pIterator->pZip = pZip;

    /* Name. */
    fs_zip_strncpy_s((char*)pIterator + sizeof(*pIterator), pChild->nameLen + 1, pChild->pName, pChild->nameLen);
    pIterator->iterator.pName   = (const char*)pIterator + sizeof(*pIterator);
    pIterator->iterator.nameLen = pChild->nameLen;

    /* Info. */
    FS_ZIP_ZERO_OBJECT(&pIterator->iterator.info);

    if (pChild->childCount > 0) {
        /* The node has children. Must be a directory. */
        pIterator->iterator.info.directory = FS_TRUE;
    } else {
        /* The node does not have children. Could still be a directory. */
        fs_zip_get_file_info_by_record_offset(pZip, pZip->pIndex[pChild->iFile].offsetInBytes, &info);
        pIterator->iterator.info.directory = info.directory;

        if (!pIterator->iterator.info.directory) {
            pIterator->iterator.info.size = info.uncompressedSize;
        }
    }
}


/*
We use a minimum allocation size for iterators as an attempt to avoid the need for internal
reallocations during realloc().
*/
#define FS_ZIP_MIN_ITERATOR_ALLOCATION_SIZE 1024

FS_API fs_iterator* fs_first_zip(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen)
{
    fs_zip* pZip;
    fs_iterator_zip* pIterator;
    fs_path_iterator directoryPathIterator;
    fs_zip_cd_node* pCurrentNode;
    char  pDirectoryPathCleanStack[1024];
    char* pDirectoryPathCleanHeap = NULL;
    char* pDirectoryPathClean;
    int directoryPathCleanLen;

    pZip = (fs_zip*)fs_get_backend_data(pFS);
    FS_ZIP_ASSERT(pZip != NULL);

    if (pDirectoryPath == NULL) {
        pDirectoryPath = "";
    }

    /* Skip past any leading slash. */
    if (pDirectoryPath[0] == '/' || pDirectoryPath[0] == '\\') {
        pDirectoryPath += 1;
        if (directoryPathLen > 0) {
            directoryPathLen -= 1;
        }
    }

    /* The path must be clean of any special directories. We'll have to clean the path with fs_path_normalize(). */
    directoryPathCleanLen = fs_path_normalize(pDirectoryPathCleanStack, sizeof(pDirectoryPathCleanStack), pDirectoryPath, directoryPathLen, FS_NO_ABOVE_ROOT_NAVIGATION);
    if (directoryPathCleanLen < 0) {
        return NULL;
    }

    if (directoryPathCleanLen > (int)sizeof(pDirectoryPathCleanStack)) {
        pDirectoryPathCleanHeap = (char*)fs_malloc(directoryPathCleanLen + 1, fs_get_allocation_callbacks(pFS));
        if (pDirectoryPathCleanHeap == NULL) {
            return NULL;    /* Out of memory. */
        }

        fs_path_normalize(pDirectoryPathCleanHeap, directoryPathCleanLen + 1, pDirectoryPath, directoryPathLen, FS_NO_ABOVE_ROOT_NAVIGATION);   /* <-- This should never fail. */
        pDirectoryPathClean = pDirectoryPathCleanHeap;
    } else {
        pDirectoryPathClean = pDirectoryPathCleanStack;
    }

    /* Always start from the root node. */
    pCurrentNode = pZip->pCDRootNode;

    /*
    All we need to do is find the node the corresponds to the specified directory path. To do this
    we just iterate over each segment in the path and get the children one after the other.
    */
    if (fs_result_from_errno(fs_path_first(pDirectoryPathClean, directoryPathCleanLen, &directoryPathIterator)) == FS_SUCCESS) {
        for (;;) {
            /* Try finding the child node. If this cannot be found, the directory does not exist. */
            fs_zip_cd_node* pChildNode;

            pChildNode = fs_zip_cd_node_find_child(pCurrentNode, directoryPathIterator.pFullPath + directoryPathIterator.segmentOffset, directoryPathIterator.segmentLength);
            if (pChildNode == NULL) {
                fs_free(pDirectoryPathCleanHeap, fs_get_allocation_callbacks(pFS));
                return NULL;    /* Does not exist. */
            }

            pCurrentNode = pChildNode;
            
            /* Go to the next segment if we have one. */
            if (fs_result_from_errno(fs_path_next(&directoryPathIterator)) != FS_SUCCESS) {
                break;  /* Nothing left in the directory path. We have what we're looking for. */
            }
        }
    } else {
        /*
        We failed to initialize the path iterator which can only mean we were given an empty path
        in which case it should be treated as the root directory. The node will already be set to
        the root node at this point so there's nothing more do to.
        */
        FS_ZIP_ASSERT(pCurrentNode == pZip->pCDRootNode);
    }

    /* The heap allocation of the clean path is no longer needed, if we have one. */
    fs_free(pDirectoryPathCleanHeap, fs_get_allocation_callbacks(pFS));

    /* If the current node does not have any children, there is no first item and therefore nothing to return. */
    if (pCurrentNode->childCount == 0) {
        return NULL;
    }

    /*
    Now that we've found the node we have enough information to allocate the iterator. We allocate
    room for a copy of the name so we can null terminate it.
    */
    pIterator = (fs_iterator_zip*)fs_realloc(NULL, FS_ZIP_MAX(sizeof(*pIterator) + pCurrentNode->pChildren[0].nameLen + 1, FS_ZIP_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pFS));
    if (pIterator == NULL) {
        return NULL;
    }

    fs_iterator_zip_init(pZip, &pCurrentNode->pChildren[0], pIterator);

    /* Internal variables for iteration. */
    pIterator->pDirectoryNode = pCurrentNode;
    pIterator->iChild         = 0;

    return (fs_iterator*)pIterator;
}

FS_API fs_iterator* fs_next_zip(fs_iterator* pIterator)
{
    fs_iterator_zip* pIteratorZip = (fs_iterator_zip*)pIterator;
    fs_iterator_zip* pNewIteratorZip;

    if (pIteratorZip == NULL) {
        return NULL;
    }

    /* All we're doing is going to the next child. If there's nothing left we just free the iterator and return null. */
    pIteratorZip->iChild += 1;
    if (pIteratorZip->iChild >= pIteratorZip->pDirectoryNode->childCount) {
        fs_free(pIteratorZip, fs_get_allocation_callbacks(pIterator->pFS));
        return NULL;    /* Nothing left. */
    }

    /* Getting here means there's another child to iterate. */
    pNewIteratorZip = (fs_iterator_zip*)fs_realloc(pIteratorZip, FS_ZIP_MAX(sizeof(*pIteratorZip) + pIteratorZip->pDirectoryNode->pChildren[pIteratorZip->iChild].nameLen + 1, FS_ZIP_MIN_ITERATOR_ALLOCATION_SIZE), fs_get_allocation_callbacks(pIterator->pFS));
    if (pNewIteratorZip == NULL) {
        fs_free(pIteratorZip, fs_get_allocation_callbacks(pIterator->pFS));
        return NULL;    /* Out of memory. */
    }

    fs_iterator_zip_init(pNewIteratorZip->pZip, &pNewIteratorZip->pDirectoryNode->pChildren[pNewIteratorZip->iChild], pNewIteratorZip);

    return (fs_iterator*)pNewIteratorZip;
}

FS_API void fs_free_iterator_zip(fs_iterator* pIterator)
{
    fs_free(pIterator, fs_get_allocation_callbacks(pIterator->pFS));
}


fs_backend fs_zip_backend =
{
    fs_alloc_size_zip,
    fs_init_zip,
    fs_uninit_zip,
    fs_ioctl_zip,
    NULL,   /* remove */
    NULL,   /* rename */
    NULL,   /* mkdir */
    fs_info_zip,
    fs_file_alloc_size_zip,
    fs_file_open_zip,
    fs_file_open_handle_zip,
    fs_file_close_zip,
    fs_file_read_zip,
    fs_file_write_zip,
    fs_file_seek_zip,
    fs_file_tell_zip,
    fs_file_flush_zip,
    fs_file_info_zip,
    fs_file_duplicate_zip,
    fs_first_zip,
    fs_next_zip,
    fs_free_iterator_zip
};
const fs_backend* FS_ZIP = &fs_zip_backend;
/* END fs_zip.c */

#endif  /* fs_zip_c */
