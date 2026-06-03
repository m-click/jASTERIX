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

#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

struct pcap_pkthdr;
struct pcap;

namespace jASTERIX
{
// reads ASTERIX payload bytes out of a PCAP file using libpcap.
// strips the link layer (Ethernet / Linux SLL / SLL2 / raw IP), the IP header and
// the UDP/TCP header, leaving the contiguous application payload (raw/netto ASTERIX).
// the header-stripping logic is ported from COMPASS' PacketSniffer.
class PcapReader
{
  public:
    // ip_protocol | ip_version | source_ip | source_port | destination_ip | destination_port
    using Signature = std::tuple<uint8_t, unsigned int, std::string, unsigned int, std::string,
                                 unsigned int>;

    // accumulated payload for one signature (network stream)
    struct StreamData
    {
        size_t            packets = 0;  // number of packets contributing
        size_t            bytes   = 0;  // total payload bytes seen (may exceed data.size() if capped)
        std::vector<char> data;         // accumulated payload bytes (possibly capped)
    };

    PcapReader();
    ~PcapReader();

    bool open(const std::string& filename);
    bool isOpen() const { return pcap_file_ != nullptr; }
    void close();

    // analyze path: single pass over the file, accumulating payload per signature.
    // each signature's stored data is capped at max_bytes_per_signature (stats stay exact).
    std::map<Signature, StreamData> readPerSignature(
        size_t max_bytes_per_signature = std::numeric_limits<size_t>::max());

    // decode path (chunked streaming): append up to ~max_bytes of payload from all
    // signatures, in capture order, into out. position is preserved across calls.
    // sets eof to true when the file end is reached. returns false on read error.
    bool readNextChunk(std::vector<char>& out, size_t max_bytes, bool& eof);

    bool hasUnknownHeaders() const;

    const std::set<int>& unknownLinkTypes() const { return unknown_link_types_; }
    const std::set<int>& unknownEtherTypes() const { return unknown_ether_types_; }
    const std::set<int>& unknownIPProtocols() const { return unknown_ip_protocols_; }

    size_t numPacketsRead() const { return num_packets_read_; }
    size_t numPacketsDropped() const { return num_packets_dropped_; }

    static std::string signatureToString(const Signature& signature);

  private:
    enum class Mode
    {
        PerSignature,  // accumulate into data_per_signature_
        Chunk          // accumulate into chunk_buffer_
    };

    // called by the libpcap packet handler
    void digestPacket(const struct pcap_pkthdr* pkthdr, const unsigned char* packet);
    void digestEther(int ether_type, const struct pcap_pkthdr* pkthdr,
                     const unsigned char* packet, unsigned long data_offs);
    void addPayload(const Signature& signature, const unsigned char* data, size_t len);

    friend void pcapReaderPacketHandler(unsigned char*, const struct pcap_pkthdr*,
                                        const unsigned char*);

    pcap* pcap_file_       = nullptr;
    int   link_layer_type_ = -1;
    bool  reached_eof_     = false;

    Mode   mode_                   = Mode::Chunk;
    size_t max_bytes_per_sig_      = std::numeric_limits<size_t>::max();

    std::map<Signature, StreamData> data_per_signature_;  // used in PerSignature mode

    std::vector<char> chunk_buffer_;  // used in Chunk mode
    size_t            chunk_max_bytes_ = std::numeric_limits<size_t>::max();
    bool              chunk_full_      = false;

    size_t num_packets_read_    = 0;
    size_t num_packets_dropped_ = 0;

    std::set<int> unknown_link_types_;
    std::set<int> unknown_ether_types_;
    std::set<int> unknown_ip_protocols_;
};
}  // namespace jASTERIX
