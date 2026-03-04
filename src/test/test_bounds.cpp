/*
 * This file is part of jASTERIX.
 *
 * jASTERIX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * jASTERIX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with jASTERIX.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "catch.hpp"
#include "jasterix.h"
#include "logger.h"
#include "test_jasterix.h"

using namespace std;
using namespace nlohmann;

namespace
{

// Decode inline binary data and capture num_errors / num_records.
void decode_malformed(jASTERIX::jASTERIX& jasterix,
                      const char* data, unsigned int size,
                      size_t& out_errors, size_t& out_records)
{
    out_errors = 0;
    out_records = 0;

    jasterix.decodeData(data, size,
                        [&](unique_ptr<json> /*json_data*/, size_t /*num_frames*/,
                            size_t num_records, size_t num_errors)
                        {
                            out_records = num_records;
                            out_errors = num_errors;
                        },
                        false);  // not abortable
}

}  // anonymous namespace

// ─── Test 1: FixedBytesItemParser — truncated fixed-length item ───
// CAT002 FSPEC 0xD4 selects items 010, 000, 030, 050.
// Item 010 is 2 bytes (FixedBytes). Buffer only has 1 byte after FSPEC.
// Data: 02 00 06 d4 00
//       CAT LEN  FSPEC  1-byte (need 2 for item 010)
TEST_CASE("Bounds: FixedBytes truncated", "[bounds]")
{
    loginf << "bounds test: FixedBytes truncated start" << logendl;

    jASTERIX::jASTERIX jasterix(definition_path, false, false, false);
    REQUIRE(jasterix.hasCategory(2));
    auto cat = jasterix.category(2);
    REQUIRE(cat->hasEdition("1.0"));
    cat->setCurrentEdition("1.0");
    cat->setCurrentMapping("");

    // LEN=6 means 3 bytes content, but FSPEC=0xD4 needs items 010(2)+000(1)+030(3)+050(var)
    // Only 1 byte of content after FSPEC → item 010 read overflows
    const char data[] = {0x02, 0x00, 0x06, char(0xd4), 0x00};
    size_t errors = 0, records = 0;

    decode_malformed(jasterix, data, sizeof(data), errors, records);

    loginf << "bounds test: FixedBytes truncated errors=" << errors
           << " records=" << records << logendl;
    REQUIRE(errors > 0);
}

// ─── Test 2: Record-level truncation at item start ───
// CAT002 FSPEC 0x04 selects only item 050. Buffer ends exactly at the FSPEC,
// so the Record-level bounds check fires before dispatching to any item parser.
// Verifies graceful early exit (partial record parsed, no crash).
// Data: 02 00 04 04
//       CAT LEN  FSPEC (no data bytes at all)
TEST_CASE("Bounds: Record-level truncation", "[bounds]")
{
    loginf << "bounds test: Record-level truncation start" << logendl;

    jASTERIX::jASTERIX jasterix(definition_path, false, false, false);
    REQUIRE(jasterix.hasCategory(2));
    auto cat = jasterix.category(2);
    REQUIRE(cat->hasEdition("1.0"));
    cat->setCurrentEdition("1.0");
    cat->setCurrentMapping("");

    const char data[] = {0x02, 0x00, 0x04, 0x04};
    size_t errors = 0, records = 0;

    decode_malformed(jasterix, data, sizeof(data), errors, records);

    loginf << "bounds test: Record-level truncation errors=" << errors
           << " records=" << records << logendl;

    // Record-level check stops parsing early (no throw), so records >= 1 and no crash.
    REQUIRE(records >= 1);
}

// ─── Test 3: ExtendableBitsItemParser — FSPEC FX=1 at buffer end ───
// CAT002 FSPEC byte 0xFF has FX=1 (extension bit set), meaning another
// FSPEC byte should follow, but the buffer ends.
// Data: 02 00 04 ff
//       CAT LEN  FSPEC(FX=1, no next byte)
TEST_CASE("Bounds: ExtendableBits FX overflow", "[bounds]")
{
    loginf << "bounds test: ExtendableBits FX overflow start" << logendl;

    jASTERIX::jASTERIX jasterix(definition_path, false, false, false);
    REQUIRE(jasterix.hasCategory(2));
    auto cat = jasterix.category(2);
    REQUIRE(cat->hasEdition("1.0"));
    cat->setCurrentEdition("1.0");
    cat->setCurrentMapping("");

    const char data[] = {0x02, 0x00, 0x04, char(0xff)};
    size_t errors = 0, records = 0;

    decode_malformed(jasterix, data, sizeof(data), errors, records);

    loginf << "bounds test: ExtendableBits FX overflow errors=" << errors
           << " records=" << records << logendl;
    REQUIRE(errors > 0);
}

// ─── Test 4: ExtendableItemParser — extend=1 at buffer end ───
// CAT002 FSPEC 0x04 selects only item 050 (extendable).
// One byte 0x93 = 10010011b, bit 0 (extend) = 1 → expects another byte, but buffer ends.
// Data: 02 00 05 04 93
//       CAT LEN  FSPEC 050[0](extend=1, no next octet)
TEST_CASE("Bounds: ExtendableItem extend overflow", "[bounds]")
{
    loginf << "bounds test: ExtendableItem extend overflow start" << logendl;

    jASTERIX::jASTERIX jasterix(definition_path, false, false, false);
    REQUIRE(jasterix.hasCategory(2));
    auto cat = jasterix.category(2);
    REQUIRE(cat->hasEdition("1.0"));
    cat->setCurrentEdition("1.0");
    cat->setCurrentMapping("");

    const char data[] = {0x02, 0x00, 0x05, 0x04, char(0x93)};
    size_t errors = 0, records = 0;

    decode_malformed(jasterix, data, sizeof(data), errors, records);

    loginf << "bounds test: ExtendableItem extend overflow errors=" << errors
           << " records=" << records << logendl;
    REQUIRE(errors > 0);
}

// ─── Test 5: RepetetiveItemParser — sub-item overflow ───
// CAT048 item 250 (Mode S MB Data) is repetitive (8 bytes per sub-item).
// FSPEC: 0x01 0x20 (FX=1 in byte 1, FRN10=250 in byte 2, FX=0).
// Provide REP=3 but no sub-item data → sub-item parser throws on bounds check.
// Data: 30 00 06 01 20 03
//       CAT LEN  FSPEC(2B)  REP=3 (no sub-item data)
TEST_CASE("Bounds: Repetitive sub-item overflow", "[bounds]")
{
    loginf << "bounds test: Repetitive sub-item overflow start" << logendl;

    jASTERIX::jASTERIX jasterix(definition_path, false, false, false);
    REQUIRE(jasterix.hasCategory(48));
    auto cat = jasterix.category(48);
    REQUIRE(cat->hasEdition("1.15"));
    cat->setCurrentEdition("1.15");
    cat->setCurrentMapping("");

    const char data[] = {0x30, 0x00, 0x06, 0x01, 0x20, 0x03};
    size_t errors = 0, records = 0;

    decode_malformed(jasterix, data, sizeof(data), errors, records);

    loginf << "bounds test: Repetitive sub-item overflow errors=" << errors
           << " records=" << records << logendl;
    REQUIRE(errors > 0);
}

// ─── Test 6: CompoundItemParser — sub-item truncated ───
// CAT048 item 130 (Radar Plot Characteristics) is compound.
// FSPEC byte 1: 0x02 = only FRN7 (item 130) set, FX=0.
// Compound sub-FSPEC byte: 0x80 = sub-FRN1 (SRL, 1-byte FixedBitField) selected.
// Buffer ends before SRL data → FixedBitFieldItemParser throws inside compound.
// Data: 30 00 05 02 80
//       CAT LEN  FSPEC  130-sub-FSPEC(SRL selected, no data)
TEST_CASE("Bounds: Compound sub-item truncated", "[bounds]")
{
    loginf << "bounds test: Compound sub-item truncated start" << logendl;

    jASTERIX::jASTERIX jasterix(definition_path, false, false, false);
    REQUIRE(jasterix.hasCategory(48));
    auto cat = jasterix.category(48);
    REQUIRE(cat->hasEdition("1.15"));
    cat->setCurrentEdition("1.15");
    cat->setCurrentMapping("");

    const char data[] = {0x30, 0x00, 0x05, 0x02, char(0x80)};
    size_t errors = 0, records = 0;

    decode_malformed(jasterix, data, sizeof(data), errors, records);

    loginf << "bounds test: Compound sub-item truncated errors=" << errors
           << " records=" << records << logendl;
    REQUIRE(errors > 0);
}

// ─── Test 7: Unparsed bytes in data block ───
// CAT002 FSPEC 0x40 selects only item 000 (1 byte). Record = FSPEC(1) + item(1) = 2 bytes.
// Block content should be 2 bytes → LEN should be 5. But we set LEN=7, claiming 4 bytes
// of content. Parser decodes 2 bytes, leaving 2 unparsed.
// This doesn't throw, but should decode without crashing. Records should still parse.
// Data: 02 00 07 40 01 00 00
//       CAT LEN  FSPEC 000  (2 extra bytes)
TEST_CASE("Bounds: unparsed bytes in data block", "[bounds]")
{
    loginf << "bounds test: unparsed bytes start" << logendl;

    jASTERIX::jASTERIX jasterix(definition_path, false, false, false);
    REQUIRE(jasterix.hasCategory(2));
    auto cat = jasterix.category(2);
    REQUIRE(cat->hasEdition("1.0"));
    cat->setCurrentEdition("1.0");
    cat->setCurrentMapping("");

    const char data[] = {0x02, 0x00, 0x07, 0x40, 0x01, 0x00, 0x00};
    size_t errors = 0, records = 0;

    decode_malformed(jasterix, data, sizeof(data), errors, records);

    loginf << "bounds test: unparsed bytes errors=" << errors
           << " records=" << records << logendl;

    // The record itself parses fine (1 record), the extra bytes are just logged as a warning.
    // No crash is the main assertion. Records should have been decoded.
    REQUIRE(records >= 1);
}
