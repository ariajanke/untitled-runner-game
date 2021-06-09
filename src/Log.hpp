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

#pragma once

#include <sstream>
#include <vector>

// gonna scrap it
// interface should look like this:
//
// // the "log" static variable should not allow the following:
// static auto log = make_log_line(1);
// log = make_log_line(2);
//
// // this should be what usage looks like
// static /* const */ auto log = make_rt_log_line(1);
// log() << "something updated at runtime";

class LogReceiver;

class LogStream {
public:
    static constexpr const int k_unassigned_line = -1;
    LogStream(LogReceiver *, std::ostream *, int assigned_line);
    ~LogStream();

    // this should be far more like ostream
    template <typename T>
    const LogStream & operator << (const T & obj) const {
        (*m_strm_ptr) << obj;
        return *this;
    }

private:
    std::ostream * m_strm_ptr = nullptr;
    LogReceiver * m_receiver = nullptr;
    int m_assigned_line = k_unassigned_line;
};

class LogLine {
public:
    static constexpr const int k_unassigned_line = LogStream::k_unassigned_line;
    explicit LogLine(int assigned_line);
    LogStream operator () () const;

private:
    static int verify_line_assignment(int);

    int m_assigned_line = k_unassigned_line;
};

class LogReceiverAttorney;

class LogReceiver {
public:
    friend class LogReceiverAttorney;
    void setup_log_receiver();
    void clear_log_for_next_frame();
protected:
    virtual ~LogReceiver();
    virtual void on_log(int line, std::string && contents) = 0;

private:
    std::vector<std::stringstream> m_streams;
    std::vector<bool> m_hit_on_this_frame;
};

class LogReceiverAttorney {
    friend class LogLine;
    friend class LogStream;
    static void update_log_for_line(LogReceiver &, int line);
    static std::ostream * get_log_line_stream_pointer(LogReceiver &, int line);
};

static constexpr const int k_max_log_lines = 20;
// not mt safe
void assign_global_log_line_receiver(LogReceiver *);
// not mt safe
inline const LogLine make_log_line(int i) { return LogLine(i); }
