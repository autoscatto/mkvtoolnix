/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   AAC demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "error.h"
#include "r_aac.h"
#include "p_aac.h"

#define PROBESIZE 8192

int
aac_reader_c::probe_file(mm_io_c *io,
                         int64_t size) {
  unsigned char buf[PROBESIZE];
  aac_header_t aacheader;
  int pos;

  if (size < PROBESIZE)
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(buf, PROBESIZE) != PROBESIZE)
      io->setFilePointer(0, seek_beginning);
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  if (parse_aac_adif_header(buf, PROBESIZE, &aacheader))
    return 1;
  pos = find_aac_header(buf, PROBESIZE, &aacheader, false);
  if ((pos < 0) || ((pos + aacheader.bytes) >= PROBESIZE)) {
    pos = find_aac_header(buf, PROBESIZE, &aacheader, true);
    if ((pos < 0) || ((pos + aacheader.bytes) >= PROBESIZE))
      return 0;
    pos = find_aac_header(&buf[pos + aacheader.bytes], PROBESIZE - pos -
                          aacheader.bytes, &aacheader, true);
    if (pos != 0)
      return 0;
  }
  pos = find_aac_header(&buf[pos + aacheader.bytes], PROBESIZE - pos -
                        aacheader.bytes, &aacheader, false);
  if (pos != 0)
    return 0;

  return 1;
}

#define INITCHUNKSIZE 16384
#define SINITCHUNKSIZE "16384"

aac_reader_c::aac_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {
  int adif, i;

  try {
    io = new mm_file_io_c(ti.fname);
    size = io->get_size();
    chunk = (unsigned char *)safemalloc(INITCHUNKSIZE);
    if (io->read(chunk, INITCHUNKSIZE) != INITCHUNKSIZE)
      throw error_c("aac_reader: Could not read " SINITCHUNKSIZE " bytes.");
    io->setFilePointer(0, seek_beginning);
    if (parse_aac_adif_header(chunk, INITCHUNKSIZE, &aacheader)) {
      throw error_c("aac_reader: ADIF header files are not supported.");
      adif = 1;
    } else {
      if (find_aac_header(chunk, INITCHUNKSIZE, &aacheader, emphasis_present)
          != 0)
        throw error_c("aac_reader: No valid AAC packet found in the first "
                      SINITCHUNKSIZE " bytes.\n");
      guess_adts_version();
      adif = 0;
    }
    bytes_processed = 0;
    ti.id = 0;                 // ID for this track.

    for (i = 0; i < ti.aac_is_sbr.size(); i++)
      if ((ti.aac_is_sbr[i] == 0) || (ti.aac_is_sbr[i] == -1)) {
        aacheader.profile = AAC_PROFILE_SBR;
        break;
      }
  } catch (...) {
    throw error_c("aac_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the AAC demultiplexer.\n", ti.fname.c_str());
}

aac_reader_c::~aac_reader_c() {
  delete io;
  safefree(chunk);
}

void
aac_reader_c::create_packetizer(int64_t) {
  generic_packetizer_c *aacpacketizer;

  if (NPTZR() != 0)
    return;
  if (aacheader.profile != AAC_PROFILE_SBR)
    mxwarn("AAC files may contain HE-AAC / AAC+ / SBR AAC audio. "
           "This can NOT be detected automatically. Therefore you have to "
           "specifiy '--aac-is-sbr 0' manually for this input file if the "
           "file actually contains SBR AAC. The file will be muxed in the "
           "WRONG way otherwise. Also read mkvmerge's documentation.\n");

  aacpacketizer = new aac_packetizer_c(this, aacheader.id, aacheader.profile,
                                       aacheader.sample_rate,
                                       aacheader.channels, ti,
                                       emphasis_present);
  add_packetizer(aacpacketizer);
  if (aacheader.profile == AAC_PROFILE_SBR)
    aacpacketizer->set_audio_output_sampling_freq(aacheader.sample_rate * 2);
  mxinfo(FMT_TID "Using the AAC output module.\n", ti.fname.c_str(),
         (int64_t)0);
}

// Try to guess if the MPEG4 header contains the emphasis field (2 bits)
void
aac_reader_c::guess_adts_version() {
  int pos;
  aac_header_t aacheader;

  emphasis_present = false;

  // Due to the checks we do have an ADTS header at 0.
  find_aac_header(chunk, INITCHUNKSIZE, &aacheader, emphasis_present);
  if (aacheader.id != 0)        // MPEG2
    return;

  // Now make some sanity checks on the size field.
  if (aacheader.bytes > 8192) {
    emphasis_present = true;    // Looks like it's borked.
    return;
  }

  // Looks ok so far. See if the next ADTS is right behind this packet.
  pos = find_aac_header(&chunk[aacheader.bytes], INITCHUNKSIZE -
                        aacheader.bytes, &aacheader, emphasis_present);
  if (pos != 0) {               // Not ok - what do we do now?
    emphasis_present = true;
    return;
  }
}

file_status_e
aac_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread;

  nread = io->read(chunk, 4096);
  if (nread <= 0) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  memory_c mem(chunk, nread, false);
  PTZR0->process(mem);
  bytes_processed += nread;

  return FILE_STATUS_MOREDATA;
}

int
aac_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
aac_reader_c::identify() {
  mxinfo("File '%s': container: AAC\nTrack ID 0: audio (AAC)\n",
         ti.fname.c_str());
}
