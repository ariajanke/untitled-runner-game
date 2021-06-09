/****************************************************************************

    Copyright 2021 Aria Janke

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*****************************************************************************/

#include "Log.hpp"

#include <vector>

#include <cassert>

namespace {

using RtError = std::runtime_error;
using InvArg  = std::invalid_argument;

LogReceiver * s_global_line_receiver = nullptr;

} // end of <anonymous> namespace


LogStream::LogStream
    (LogReceiver * rec, std::ostream * strm, int assigned_line)
:
    m_strm_ptr(strm),
    m_receiver(rec),
    m_assigned_line(assigned_line)
{}

LogStream::~LogStream() {
    if (std::uncaught_exceptions() > 0) return;
    LogReceiverAttorney::update_log_for_line(*m_receiver, m_assigned_line);
}

// ----------------------------------------------------------------------------

LogLine::LogLine(int assigned_line):
    m_assigned_line(verify_line_assignment(assigned_line))
{}

LogStream LogLine::operator () () const {
    if (!s_global_line_receiver) {
        throw RtError("Global line receiver has not been assigned.");
    }
    auto * strm = LogReceiverAttorney::get_log_line_stream_pointer
        (*s_global_line_receiver, m_assigned_line);
    return LogStream(s_global_line_receiver, strm, m_assigned_line);
}

/* private static */ int LogLine::verify_line_assignment(int line) {
    if (line >= 0 && line < k_max_log_lines) return line;
    throw InvArg("Line assigment must be between zero and the maximum ("
                 + std::to_string(k_max_log_lines) + ").");
}

// ----------------------------------------------------------------------------

void LogReceiver::setup_log_receiver()
    { m_streams.resize(k_max_log_lines); }

/* protected */ LogReceiver::~LogReceiver() {}

// ----------------------------------------------------------------------------

/* private static */ void LogReceiverAttorney::update_log_for_line
    (LogReceiver & inst, int line)
{
    assert(!inst.m_streams.empty());
    auto & stream = inst.m_streams.at(std::size_t(line));
    inst.on_log(line, stream.str());
    stream.str("");
}

/* private static */ std::ostream * LogReceiverAttorney::get_log_line_stream_pointer
    (LogReceiver & inst, int line)
{ return &inst.m_streams.at(std::size_t(line)); }

// ----------------------------------------------------------------------------

void assign_global_log_line_receiver(LogReceiver * ptr) {
    if (!s_global_line_receiver) {
        s_global_line_receiver = ptr;
        return;
    }
    throw RtError("assign_global_log_line_receiver: Global receiver pointer "
                  "may not be assigned more than once.");
}
