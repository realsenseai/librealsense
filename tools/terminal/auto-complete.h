// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once
#include <set>
#include <string>
#include <vector>
#include <functional>
#include <stdint.h>

#if defined(_WIN32)
    #include <conio.h>
    #define NEW_LINE '\r'
    #define BACKSPACE '\b'
    #define BACKSPACE_ALT 8  // alternative for backspace on some terminals
    #define BACKSLASH_ZERO '\0'
    #define UP_ARROW_KEY {72}
    #define DOWN_ARROW_KEY {80}
#else
    #include <unistd.h>
    #include <termios.h>
    #define NEW_LINE '\n'
    #define BACKSPACE 127
    #define BACKSPACE_ALT 8 // alternative for backspace on some terminals
    #define BACKSLASH_ZERO ' '
    #define UP_ARROW_KEY {27, 91, 65}
    #define DOWN_ARROW_KEY {27, 91, 66}
#endif

class auto_complete
{
public:
    explicit auto_complete(std::set<std::string> dictionary, bool bypass = false);
    std::string get_line(std::function<bool()> to_stop = []() {return false;}); // to_stop predicate can be used to stop waiting for input if the camera disconnected

private:
    void handle_special_key(const std::vector<uint8_t>& chars);
    char getch_nolock(std::function <bool()> stop = [](){return false;});
    void backspace(const int num_of_backspaces);
    std::string chars2_queue_to_string() const;
    std::string get_last_word(const std::string& line) const;
    std::vector<std::string> search(const std::string& word) const;

    std::set<std::string> _dictionary;
    std::vector<std::string> _history_words;
    unsigned _history_words_index;
    std::vector<char> _chars2_queue;
    int _num_of_chars2_in_line;
    unsigned _tab_index;
    std::vector<std::string> _finds_vec;
    bool _is_first_time_up_arrow;
    bool _bypass;
};
