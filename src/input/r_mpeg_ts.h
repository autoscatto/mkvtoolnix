/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the MPEG TS demultiplexer module

   Written by Massimo Callegari <massimocallegari@yahoo.it>
   and Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_MPEG_TS_H
#define __R_MPEG_TS_H

#include "common/common_pch.h"

#include "common/byte_buffer.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

/* TS packet header */
typedef struct {
  unsigned char sync_byte;
  unsigned char pid_msb:5, transport_priority:1, payload_unit_start_indicator:1, transport_error_indicator:1;
  unsigned char pid_lsb;
  unsigned char continuity_counter:4, adaptation_field_control:2, transport_scrambling_control:2;
} mpeg_ts_packet_header_t;

/* Adaptation field */
typedef struct {
  unsigned char adaptation_field_length;
  unsigned char adaptation_field_extension_flag:1, transport_private_data_flag:1, splicing_point_flag:1, opcr_flag:1,
                pcr_flag:1, elementary_stream_priority_indicator:1, random_access_indicator:1, discontinuity_indicator:1;
} mpeg_ts_adaptation_field_t;

/* CRC */
typedef struct {
  unsigned char crc_3msb, crc_2msb, crc_1msb, crc_lsb;
} mpeg_ts_crc_t;

/* PAT header */
typedef struct {
  unsigned char table_id;
  unsigned char section_length_msb:4, reserved:2, zero:1, section_syntax_indicator:1;
  unsigned char section_length_lsb;
  unsigned char transport_stream_id_msb;
  unsigned char transport_stream_id_lsb;
  unsigned char current_next_indicator:1, version_number:5, reserved2:2;
  unsigned char section_number;
  unsigned char last_section_number;
} mpeg_ts_pat_t;

/* PAT section */
typedef struct {
  unsigned char program_number_msb;
  unsigned char program_number_lsb;
  unsigned char pid_msb:5, reserved3:3;
  unsigned char pid_lsb;
} mpeg_ts_pat_section_t;

/* PMT header */
typedef struct {
  unsigned char table_id;
  unsigned char section_length_msb:4, reserved:2, zero:1, section_syntax_indicator:1;
  unsigned char section_length_lsb;
  unsigned char program_number_msb;
  unsigned char program_number_lsb;
  unsigned char current_next_indicator:1, version_number:5, reserved2:2;
  unsigned char section_number;
  unsigned char last_section_number;
  unsigned char pcr_pid_msb:5, reserved3:3;
  unsigned char pcr_pid_lsb;
  unsigned char program_info_length_msb:4, reserved4:4;
  unsigned char program_info_length_lsb:8;
} mpeg_ts_pmt_t;

/* PMT descriptor */
typedef struct {
  unsigned char tag;
  unsigned char length;
} mpeg_ts_pmt_descriptor_t;

/* PMT pid info */
typedef struct {
  unsigned char stream_type;
  unsigned char pid_msb:5, reserved:3;
  unsigned char pid_lsb;
  unsigned char es_info_length_msb:4, reserved2:4;
  unsigned char es_info_length_lsb;
} mpeg_ts_pmt_pid_info_t;

/* PES header */
typedef struct
{
  unsigned char packet_start_code_prefix_2msb;
  unsigned char packet_start_code_prefix_1msb;
  unsigned char packet_start_code_prefix_lsb;
  unsigned char stream_id;
  unsigned char PES_packet_length_msb;
  unsigned char PES_packet_length_lsb;
  unsigned char original_or_copy:1, copyright:1, data_alignment_indicator:1, PES_priority:1, PES_scrambling_control:2, onezero:2;
  unsigned char PES_extension:1, PES_CRC:1, additional_copy_info:1, DSM_trick_mode:1, ES_rate:1, ESCR:1, PTS_DTS:2;
  unsigned char PES_header_data_length;
  unsigned char marker_bit_2msb:1, PTS_4msb:3, fill_in:4;
  unsigned char PTS_3msb;
  unsigned char marker_bit_1msb:1, PTS_2msb:7;
  unsigned char PTS_1msb;
  unsigned char marker_bit_lsb:1, PTS_lsb:7;
  unsigned char stuffing;
} mpeg_ts_pes_header_t;

struct mpeg_ts_track_t {
  bool processed;
  char type;                   //can be PAT_TYPE, PMT_TYPE, ES_VIDEO_TYPE, ES_AUDIO_TYPE, ES_SUBT_TYPE, ES_UNKNOWN
  uint32_t fourcc;
  uint16_t pid;
  bool data_ready;
  int payload_size;       // size of the current PID payload in bytes
  byte_buffer_c payload;       // buffer with the current PID payload
  unsigned char continuity_counter; // check for PID continuity

  int ptzr;                    // the actual packetizer instance

  int64_t timecode;
  //int64_t timecode_offset;

  // video related parameters
  bool v_interlaced;
  int v_version, v_width, v_height, v_dwidth, v_dheight;
  double v_frame_rate, v_aspect_ratio;
  memory_cptr v_avcc;
  unsigned char *raw_seq_hdr;
  int raw_seq_hdr_size;

  // audio related parameters
  int a_channels, a_sample_rate, a_bits_per_sample, a_bsid;

  mpeg_ts_track_t()
    : ptzr(-1)
    //, timecode_offset(-1)
    , v_interlaced(false)
    , v_version(0)
    , v_width(0)
    , v_height(0)
    , v_dwidth(0)
    , v_dheight(0)
    , v_frame_rate(0)
    , v_aspect_ratio(0)
    , raw_seq_hdr(NULL)
    , raw_seq_hdr_size(0)
    , a_channels(0)
    , a_sample_rate(0)
    , a_bits_per_sample(0)
    , a_bsid(0)
  {
  }
};

typedef counted_ptr<mpeg_ts_track_t> mpeg_ts_track_ptr;

class mpeg_ts_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  int64_t bytes_processed, size;
  bool PAT_found, PMT_found;
  int16_t PMT_pid;
  int es_to_process;
  int64_t global_timecode_offset;

  uint8_t input_status;         // can be INPUT_PROBE, INPUT_IDENTIFY, INPUT_READ
  int track_buffer_ready;

  bool file_done;

  std::vector<mpeg_ts_track_ptr> tracks;

protected:
  static int potential_packet_sizes[];
  static int detected_packet_size;

public:
  mpeg_ts_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~mpeg_ts_reader_c();

  static bool probe_file(mm_io_c *io, uint64_t size);
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual int get_progress();
  virtual void create_packetizer(int64_t tid);
  virtual void create_packetizers();

  virtual int new_stream_v_mpeg_1_2(unsigned char *buf, unsigned int length, mpeg_ts_track_ptr &track);
  virtual int new_stream_v_avc(unsigned char *buf, unsigned int length, mpeg_ts_track_ptr &track);
  virtual int new_stream_a_mpeg(unsigned char *buf, unsigned int length, mpeg_ts_track_ptr &track);
  virtual int new_stream_a_ac3(unsigned char *buf, unsigned int length, mpeg_ts_track_ptr &track);
  virtual int new_stream_a_truehd(unsigned char *buf, unsigned int length, mpeg_ts_track_ptr &track);
  virtual bool parse_packet(int id, unsigned char *buf);

  static uint32_t calculate_crc32(unsigned char *data, int len);

private:
  int parse_pat(unsigned char *pat);
  int parse_pmt(unsigned char *pmt);
  file_status_e finish();
  int send_to_packetizer(int tid);
};

#endif  // __R_MPEG_TS_H
