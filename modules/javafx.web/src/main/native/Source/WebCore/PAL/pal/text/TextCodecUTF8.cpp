/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextCodecUTF8.h"

#include "TextCodecASCIIFastPath.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace PAL {

using namespace WTF::Unicode;

const int nonCharacter = -1;

void TextCodecUTF8::registerEncodingNames(EncodingNameRegistrar registrar)
{
    // From https://encoding.spec.whatwg.org.
    registrar("UTF-8"_s, "UTF-8"_s);
    registrar("utf8"_s, "UTF-8"_s);
    registrar("unicode-1-1-utf-8"_s, "UTF-8"_s);

    // Additional aliases that originally were present in the encoding
    // table in WebKit on Macintosh, and subsequently added by
    // TextCodecICU. Perhaps we can prove some are not used on the web
    // and remove them.
    registrar("unicode11utf8"_s, "UTF-8"_s);
    registrar("unicode20utf8"_s, "UTF-8"_s);
    registrar("x-unicode20utf8"_s, "UTF-8"_s);
}

std::unique_ptr<TextCodecUTF8> TextCodecUTF8::codec()
{
        return makeUnique<TextCodecUTF8>();
}

void TextCodecUTF8::registerCodecs(TextCodecRegistrar registrar)
{
    registrar("UTF-8"_s, [] {
        return codec();
    });
}

static inline uint8_t nonASCIISequenceLength(uint8_t firstByte)
{
    static const uint8_t lengths[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    return lengths[firstByte];
}

static inline int decodeNonASCIISequence(const uint8_t* sequence, uint8_t& length)
{
    ASSERT(!isASCII(sequence[0]));
    if (length == 2) {
        ASSERT(sequence[0] >= 0xC2);
        ASSERT(sequence[0] <= 0xDF);
        if (sequence[1] < 0x80 || sequence[1] > 0xBF) {
            length = 1;
            return nonCharacter;
        }
        return ((sequence[0] << 6) + sequence[1]) - 0x00003080;
    }
    if (length == 3) {
        ASSERT(sequence[0] >= 0xE0);
        ASSERT(sequence[0] <= 0xEF);
        switch (sequence[0]) {
        case 0xE0:
            if (sequence[1] < 0xA0 || sequence[1] > 0xBF) {
                length = 1;
                return nonCharacter;
            }
            break;
        case 0xED:
            if (sequence[1] < 0x80 || sequence[1] > 0x9F) {
                length = 1;
                return nonCharacter;
            }
            break;
        default:
            if (sequence[1] < 0x80 || sequence[1] > 0xBF) {
                length = 1;
                return nonCharacter;
            }
        }
        if (sequence[2] < 0x80 || sequence[2] > 0xBF) {
            length = 2;
            return nonCharacter;
        }
        return ((sequence[0] << 12) + (sequence[1] << 6) + sequence[2]) - 0x000E2080;
    }
    ASSERT(length == 4);
    ASSERT(sequence[0] >= 0xF0);
    ASSERT(sequence[0] <= 0xF4);
    switch (sequence[0]) {
    case 0xF0:
        if (sequence[1] < 0x90 || sequence[1] > 0xBF) {
            length = 1;
            return nonCharacter;
        }
        break;
    case 0xF4:
        if (sequence[1] < 0x80 || sequence[1] > 0x8F) {
            length = 1;
            return nonCharacter;
        }
        break;
    default:
        if (sequence[1] < 0x80 || sequence[1] > 0xBF) {
            length = 1;
            return nonCharacter;
        }
    }
    if (sequence[2] < 0x80 || sequence[2] > 0xBF) {
        length = 2;
        return nonCharacter;
    }
    if (sequence[3] < 0x80 || sequence[3] > 0xBF) {
        length = 3;
        return nonCharacter;
    }
    return ((sequence[0] << 18) + (sequence[1] << 12) + (sequence[2] << 6) + sequence[3]) - 0x03C82080;
}

static inline UChar* appendCharacter(UChar* destination, int character)
{
    ASSERT(character != nonCharacter);
    ASSERT(!U_IS_SURROGATE(character));
    if (U_IS_BMP(character))
        *destination++ = character;
    else {
        *destination++ = U16_LEAD(character);
        *destination++ = U16_TRAIL(character);
    }
    return destination;
}

void TextCodecUTF8::consumePartialSequenceByte()
{
    --m_partialSequenceSize;
    memmove(m_partialSequence, m_partialSequence + 1, m_partialSequenceSize);
}

bool TextCodecUTF8::handlePartialSequence(LChar*& destination, std::span<const uint8_t>& source, bool flush)
{
    ASSERT(m_partialSequenceSize);
    do {
        if (isASCII(m_partialSequence[0])) {
            *destination++ = m_partialSequence[0];
            consumePartialSequenceByte();
            continue;
        }
        auto count = nonASCIISequenceLength(m_partialSequence[0]);
        if (!count)
            return true;

        // Copy from `source` until we have `count` bytes.
        if (count > m_partialSequenceSize && !source.empty()) {
            size_t additionalBytes = std::min<size_t>(count - m_partialSequenceSize, source.size());
            memcpy(m_partialSequence + m_partialSequenceSize, source.data(), additionalBytes);
            source = source.subspan(additionalBytes);
            m_partialSequenceSize += additionalBytes;
        }

        // If we still don't have `count` bytes, fill the rest with zeros (any
        // other lead byte would do), so we can run `decodeNonASCIISequence` to
        // tell if the chunk that we have is valid. These bytes are not part of
        // the partial sequence, so don't increment `m_partialSequenceSize`.
        bool partialSequenceIsTooShort = false;
        if (count > m_partialSequenceSize) {
            partialSequenceIsTooShort = true;
            memset(m_partialSequence + m_partialSequenceSize, 0, count - m_partialSequenceSize);
        }

        int character = decodeNonASCIISequence(m_partialSequence, count);
        if (partialSequenceIsTooShort) {
            ASSERT(character == nonCharacter);
            ASSERT(count <= m_partialSequenceSize);
            // If we're not at the end, and the partial sequence that we have is
            // incomplete but otherwise valid, a non-character is not an error.
            if (!flush && count == m_partialSequenceSize)
                return false;
        }

        if (!isLatin1(character))
            return true;

        m_partialSequenceSize -= count;
        *destination++ = character;
    } while (m_partialSequenceSize);

    return false;
}

void TextCodecUTF8::handlePartialSequence(UChar*& destination, std::span<const uint8_t>& source, bool flush, bool stopOnError, bool& sawError)
{
    ASSERT(m_partialSequenceSize);
    do {
        if (isASCII(m_partialSequence[0])) {
            *destination++ = m_partialSequence[0];
            consumePartialSequenceByte();
            continue;
        }
        auto count = nonASCIISequenceLength(m_partialSequence[0]);
        if (!count) {
            sawError = true;
            if (stopOnError)
                return;
            *destination++ = replacementCharacter;
            consumePartialSequenceByte();
            continue;
        }

        // Copy from `source` until we have `count` bytes.
        if (count > m_partialSequenceSize && !source.empty()) {
            size_t additionalBytes = std::min<size_t>(count - m_partialSequenceSize, source.size());
            memcpy(m_partialSequence + m_partialSequenceSize, source.data(), additionalBytes);
            source = source.subspan(additionalBytes);
            m_partialSequenceSize += additionalBytes;
        }

        // If we still don't have `count` bytes, fill the rest with zeros (any
        // other lead byte would do), so we can run `decodeNonASCIISequence` to
        // tell if the chunk that we have is valid. These bytes are not part of
        // the partial sequence, so don't increment `m_partialSequenceSize`.
        bool partialSequenceIsTooShort = false;
        if (count > m_partialSequenceSize) {
            partialSequenceIsTooShort = true;
            memset(m_partialSequence + m_partialSequenceSize, 0, count - m_partialSequenceSize);
        }

        int character = decodeNonASCIISequence(m_partialSequence, count);
        if (partialSequenceIsTooShort) {
            ASSERT(character == nonCharacter);
            ASSERT(count <= m_partialSequenceSize);
            // If we're not at the end, and the partial sequence that we have is
            // incomplete but otherwise valid, a non-character is not an error.
            if (!flush && count == m_partialSequenceSize)
                return;
        }

        if (character == nonCharacter) {
            sawError = true;
            if (stopOnError)
                return;
            *destination++ = replacementCharacter;
            m_partialSequenceSize -= count;
            memmove(m_partialSequence, m_partialSequence + count, m_partialSequenceSize);
            continue;
        }

        m_partialSequenceSize -= count;
        if (std::exchange(m_shouldStripByteOrderMark, false) && character == byteOrderMark)
            continue;
        destination = appendCharacter(destination, character);
    } while (m_partialSequenceSize);
}

String TextCodecUTF8::decode(std::span<const uint8_t> bytes, bool flush, bool stopOnError, bool& sawError)
{
    // Each input byte might turn into a character.
    // That includes all bytes in the partial-sequence buffer because
    // each byte in an invalid sequence will turn into a replacement character.
    size_t bufferSize = m_partialSequenceSize + bytes.size();
    if (bufferSize > std::numeric_limits<unsigned>::max()) {
        sawError = true;
        return { };
    }
    StringBuffer<LChar> buffer(bufferSize);

    auto source = bytes;
    auto* alignedEnd = WTF::alignToMachineWord(bytes.data() + bytes.size());
    LChar* destination = buffer.characters();

    do {
        if (m_partialSequenceSize) {
            // Explicitly copy destination and source pointers to avoid taking pointers to the
            // local variables, which may harm code generation by disabling some optimizations
            // in some compilers.
            LChar* destinationForHandlePartialSequence = destination;
            if (handlePartialSequence(destinationForHandlePartialSequence, source, flush)) {
                goto upConvertTo16Bit;
            }
            destination = destinationForHandlePartialSequence;
            if (m_partialSequenceSize)
                break;
        }

        while (!source.empty()) {
            if (isASCII(source.front())) {
                // Fast path for ASCII. Most UTF-8 text will be ASCII.
                if (WTF::isAlignedToMachineWord(source.data())) {
                    while (source.data() < alignedEnd) {
                        auto chunk = *reinterpret_cast_ptr<const WTF::MachineWord*>(source.data());
                        if (!WTF::containsOnlyASCII<LChar>(chunk))
                            break;
                        copyASCIIMachineWord(destination, source.data());
                        source = source.subspan(sizeof(WTF::MachineWord));
                        destination += sizeof(WTF::MachineWord);
                    }
                    if (source.empty())
                        break;
                    if (!isASCII(source.front()))
                        continue;
                }
                *destination++ = source.front();
                source = source.subspan(1);
                continue;
            }
            auto count = nonASCIISequenceLength(source.front());
            int character;
            if (!count)
                character = nonCharacter;
            else {
                if (count > source.size()) {
                    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(source.size() < static_cast<ptrdiff_t>(sizeof(m_partialSequence)));
                    ASSERT(!m_partialSequenceSize);
                    m_partialSequenceSize = source.size();
                    memcpy(m_partialSequence, source.data(), m_partialSequenceSize);
                    source = { };
                    break;
                }
                character = decodeNonASCIISequence(source.data(), count);
            }
            if (character == nonCharacter) {
                sawError = true;
                if (stopOnError)
                    break;

                goto upConvertTo16Bit;
            }
            if (!isLatin1(character))
                goto upConvertTo16Bit;

            source = source.subspan(count);
            *destination++ = character;
        }
    } while (m_partialSequenceSize);

    buffer.shrink(destination - buffer.characters());
    if (flush)
        m_partialSequenceSize = 0;
    if (flush || buffer.length())
        m_shouldStripByteOrderMark = false;
    return String::adopt(WTFMove(buffer));

upConvertTo16Bit:
    StringBuffer<UChar> buffer16(bufferSize);

    UChar* destination16 = buffer16.characters();

    // Copy the already converted characters
    for (LChar* converted8 = buffer.characters(); converted8 < destination;)
        *destination16++ = *converted8++;

    do {
        if (m_partialSequenceSize) {
            // Explicitly copy destination and source pointers to avoid taking pointers to the
            // local variables, which may harm code generation by disabling some optimizations
            // in some compilers.
            UChar* destinationForHandlePartialSequence = destination16;
            handlePartialSequence(destinationForHandlePartialSequence, source, flush, stopOnError, sawError);
            destination16 = destinationForHandlePartialSequence;
            if (m_partialSequenceSize)
                break;
        }

        while (!source.empty()) {
            if (isASCII(source.front())) {
                // Fast path for ASCII. Most UTF-8 text will be ASCII.
                if (WTF::isAlignedToMachineWord(source.data())) {
                    while (source.data() < alignedEnd) {
                        auto chunk = *reinterpret_cast_ptr<const WTF::MachineWord*>(source.data());
                        if (!WTF::containsOnlyASCII<LChar>(chunk))
                            break;
                        copyASCIIMachineWord(destination16, source.data());
                        source = source.subspan(sizeof(WTF::MachineWord));
                        destination16 += sizeof(WTF::MachineWord);
                    }
                    if (source.empty())
                        break;
                    if (!isASCII(source.front()))
                        continue;
                }
                *destination16++ = source.front();
                source = source.subspan(1);
                continue;
            }
            auto count = nonASCIISequenceLength(source.front());
            int character;
            if (!count)
                character = nonCharacter;
            else {
                if (count > source.size()) {
                    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(source.size() < static_cast<ptrdiff_t>(sizeof(m_partialSequence)));
                    ASSERT(!m_partialSequenceSize);
                    m_partialSequenceSize = source.size();
                    memcpy(m_partialSequence, source.data(), m_partialSequenceSize);
                    source = { };
                    break;
                }
                character = decodeNonASCIISequence(source.data(), count);
            }
            if (character == nonCharacter) {
                sawError = true;
                if (stopOnError)
                    break;
                *destination16++ = replacementCharacter;
                source = source.subspan(count ? count : 1);
                continue;
            }
            source = source.subspan(count);
            if (character == byteOrderMark && destination16 == buffer16.characters() && std::exchange(m_shouldStripByteOrderMark, false))
                continue;
            destination16 = appendCharacter(destination16, character);
        }
    } while (m_partialSequenceSize);

    buffer16.shrink(destination16 - buffer16.characters());
    if (flush)
        m_partialSequenceSize = 0;
    if (flush || buffer16.length())
        m_shouldStripByteOrderMark = false;
    return String::adopt(WTFMove(buffer16));
}

Vector<uint8_t> TextCodecUTF8::encodeUTF8(StringView string)
{
    // The maximum number of UTF-8 bytes needed per UTF-16 code unit is 3.
    // BMP characters take only one UTF-16 code unit and can take up to 3 bytes (3x).
    // Non-BMP characters take two UTF-16 code units and can take up to 4 bytes (2x).
    Vector<uint8_t> bytes(WTF::checkedProduct<size_t>(string.length(), 3));
    size_t bytesWritten = 0;
    for (auto character : string.codePoints())
        U8_APPEND_UNSAFE(bytes.data(), bytesWritten, character);
    bytes.shrink(bytesWritten);
    return bytes;
}

Vector<uint8_t> TextCodecUTF8::encode(StringView string, UnencodableHandling) const
{
    return encodeUTF8(string);
}

} // namespace PAL
