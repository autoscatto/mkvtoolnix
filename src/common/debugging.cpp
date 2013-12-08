/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   debugging functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include "common/logger.h"
#include "common/strings/editing.h"

std::unordered_map<std::string, std::string> debugging_c::ms_debugging_options;
bool debugging_c::ms_send_to_logger = false;

bool
debugging_c::requested(const char *option,
                       std::string *arg) {
  auto options = split(option, "|");

  for (auto &current_option : options) {
    auto option_ptr = ms_debugging_options.find(current_option);

    if (ms_debugging_options.end() != option_ptr) {
      if (arg)
        *arg = option_ptr->second;
      return true;
    }
  }

  return false;
}

void
debugging_c::request(const std::string &options,
                     bool enable) {
  auto all_options = split(options);

  for (auto &one_option : all_options) {
    auto parts = split(one_option, "=", 2);
    if (!parts[0].size())
      continue;
    if (parts[0] == "!")
      ms_debugging_options.clear();
    else if (parts[0] == "to_logger")
      debugging_c::send_to_logger(true);
    else if (!enable)
      ms_debugging_options.erase(parts[0]);
    else
      ms_debugging_options[parts[0]] = 1 == parts.size() ? std::string("") : parts[1];
  }

  debugging_option_c::invalidate_cache();
}

void
debugging_c::init() {
  auto env_vars = std::vector<std::string>{ "MKVTOOLNIX_DEBUG", "MTX_DEBUG", balg::to_upper_copy(get_program_name()) + "_DEBUG" };

  for (auto &name : env_vars) {
    auto value = getenv(name.c_str());
    if (value)
      request(value);
  }
}

void
debugging_c::send_to_logger(bool enable) {
  ms_send_to_logger = enable;
}

void
debugging_c::output(std::string const &msg) {
  if (ms_send_to_logger)
    log_it(msg);
  else
    mxmsg(MXMSG_INFO, msg);
}

void
debugging_c::hexdump(const void *buffer_to_dump,
                     size_t length) {
  static auto s_fmt_line = boost::format{"Debug> %|1$08x|  "};
  static auto s_fmt_byte = boost::format{"%|1$02x| "};

  std::stringstream dump, ascii;
  auto buffer     = static_cast<const unsigned char *>(buffer_to_dump);
  auto buffer_idx = 0u;

  while (buffer_idx < length) {
    if ((buffer_idx % 16) == 0) {
      if (0 < buffer_idx) {
        dump << ' ' << ascii.str() << '\n';
        ascii.str("");
      }
      dump << (s_fmt_line % buffer_idx);

    } else if ((buffer_idx % 8) == 0) {
      dump  << ' ';
      ascii << ' ';
    }

    ascii << (((32 <= buffer[buffer_idx]) && (128 > buffer[buffer_idx])) ? static_cast<char>(buffer[buffer_idx]) : '.');
    dump  << (s_fmt_byte % static_cast<unsigned int>(buffer[buffer_idx]));

    ++buffer_idx;
  }

  if ((buffer_idx % 16) != 0)
    dump << std::string(3u * (16 - (buffer_idx % 16)) + ((buffer_idx % 8) ? 1 : 0), ' ');
  dump << ' ' << ascii.str() << '\n';

  debugging_c::output(dump.str());
}

// ------------------------------------------------------------

std::vector<debugging_option_c::option_c> debugging_option_c::ms_registered_options;

size_t
debugging_option_c::register_option(std::string const &option) {
  auto itr = brng::find_if(ms_registered_options, [&option](option_c const &opt) { return opt.m_option == option; });
  if (itr != ms_registered_options.end())
    return std::distance(ms_registered_options.begin(), itr);

  ms_registered_options.emplace_back(option);

  return ms_registered_options.size() - 1;
}

void
debugging_option_c::invalidate_cache() {
  for (auto &opt : ms_registered_options)
    opt.m_requested = boost::logic::indeterminate;
}
