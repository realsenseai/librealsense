// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
#include "auto-complete.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <cmath>

using namespace std;


vector<string> auto_complete::search(const string& word) const
{
    vector<string> finds;

    for (auto it = _dictionary.begin(); it != _dictionary.end(); ++it)
    {
        it = find_if(it, _dictionary.end(), [&](string elemnt) -> bool {
            return (elemnt.compare(0, word.size(), word) == 0) ? true : false;
        });

        if (it != _dictionary.end())
        {
            if (it->compare(word) == 0)
                continue;

            finds.push_back(it->substr(word.size(), it->size()));
        }
        else
        {
            break;
        }
    }

    return finds;
}

auto_complete::auto_complete(set<string> dictionary, bool bypass)
    : _dictionary(dictionary), _history_words_index(0), _num_of_chars2_in_line(0), _tab_index(0), _is_first_time_up_arrow(true), _bypass(bypass)
{}

string auto_complete::chars2_queue_to_string() const
{
    auto temp_chars2_queue(_chars2_queue);
    temp_chars2_queue.push_back('\0');
    auto word = string(temp_chars2_queue.data());

    return word;
}

string auto_complete::get_last_word(const string& line) const
{
    auto index = line.find_last_of(' ');
    if (index > line.size())
        return line;

        return line.substr(index + 1);
}

void auto_complete::backspace(const int num_of_backspaces)
{
    for (auto i = 0; i < num_of_backspaces; ++i)
    {
        if (_num_of_chars2_in_line == 0)
            break;

        cout << '\b';

        if (_num_of_chars2_in_line != 0)
            --_num_of_chars2_in_line;

        if (!_chars2_queue.empty())
        {
            _chars2_queue.pop_back();
        }
    }
    // Clear from cursor to end of line (removes leftover characters)
    cout << "\033[K";
}

void auto_complete::handle_special_key(const vector<uint8_t>& chars)
{
    auto up_arrow_key = vector<uint8_t>(UP_ARROW_KEY);
    auto down_arrow_key = vector<uint8_t>(DOWN_ARROW_KEY);
    if (up_arrow_key == chars && _history_words.size()) // Up arrow key
    {
        if (_history_words_index != 0 && !_is_first_time_up_arrow)
            --_history_words_index;

        _finds_vec.clear();
        _tab_index = 0;
        backspace(_num_of_chars2_in_line);
        cout << _history_words[_history_words_index];

        for (auto c : _history_words[_history_words_index])
        {
            _chars2_queue.push_back(c);
            ++_num_of_chars2_in_line;
        }

        if (_is_first_time_up_arrow)
        {
            _is_first_time_up_arrow = false;
        }
    }
    else if (down_arrow_key == chars && _history_words.size()) // Down arrow key
    {
        if ((_history_words_index + 1) < _history_words.size())
            ++_history_words_index;

        _finds_vec.clear();
        _tab_index = 0;
        backspace(_num_of_chars2_in_line);
        cout << _history_words[_history_words_index];

        for (auto c : _history_words[_history_words_index])
        {
            _chars2_queue.push_back(c);
            ++_num_of_chars2_in_line;
        }
    }
}

char auto_complete::getch_nolock(std::function <bool()> stop)
{
#if defined(_WIN32)
    auto ch = static_cast<char>(_getch_nolock());
    if (ch < 0)
    {
        vector<uint8_t> vec_ch = {static_cast<uint8_t>(_getch_nolock())};
        handle_special_key(vec_ch);
        return -1;
    }

    return ch;
#else
    auto fd = fileno(stdin);

    struct termios old_settings, new_settings;
    tcgetattr(fd, &old_settings);
    new_settings = old_settings;
    new_settings.c_lflag &= (~ICANON & ~ECHO);

    tcsetattr(fd, TCSANOW, &new_settings);
    fd_set fds;
    struct timeval tv;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    while(!stop())
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        auto res = select(fd+1, &fds, NULL, NULL, &tv );
        if((res > 0) && (FD_ISSET(fd, &fds)))
        {
            uint8_t ch[10];
            auto num_of_chars = read(fd, ch, 10 );
            tcsetattr(fd, TCSAFLUSH, &old_settings );
            if (num_of_chars == 1)
                return ch[0];

            //TODO: Arrow keys
            vector<uint8_t> vec_ch;
            for (auto i = 0; i < num_of_chars; ++i)
                vec_ch.push_back(ch[i]);

            handle_special_key(vec_ch);
            return -1;
        }
        tcsetattr(fd, TCSAFLUSH, &old_settings );
        return res;
    }
    return -1;
#endif
}


string auto_complete::get_line(std::function <bool()> to_stop)
{
    if (_bypass)
    {
        string res;
        std::getline(std::cin, res);
        return res;
    }

    while (!to_stop())
    {
        auto ch = getch_nolock();

        if (ch == -1)
        {
            cout.clear();
            cin.clear();
            fflush(nullptr);
            continue;
        }

        if (ch == '\t')
        {
            if (_finds_vec.empty())
            {
                _finds_vec = search(get_last_word(chars2_queue_to_string()));
            }


            if (_finds_vec.empty() || _tab_index == _finds_vec.size())
                continue;


            if (_tab_index != 0)
            {
                backspace(int(_finds_vec[_tab_index - 1].length()));
            }

            auto vec_word = _finds_vec[_tab_index++];
            cout << vec_word;

            for (auto c : vec_word)
            {
                _chars2_queue.push_back(c);
                ++_num_of_chars2_in_line;
            }
        }
        else if (ch == ' ')
        {
            _finds_vec.clear();
            _tab_index = 0;
            _chars2_queue.push_back(' ');
            ++_num_of_chars2_in_line;

            cout << ch;
        }
        else if( ch == BACKSPACE || ch == BACKSPACE_ALT )
        {
            _finds_vec.clear();
            _tab_index = 0;

            backspace(1);
        }
        else if (ch == NEW_LINE)
        {
            _finds_vec.clear();
            _tab_index = 0;

            cout << '\n';
            cout << ch;
            auto str = chars2_queue_to_string();

            if (str.compare(""))
            {
                _history_words.push_back(str);
                _history_words_index = int(_history_words.size()) - 1;
            }
            _is_first_time_up_arrow = true;
        }
        else if (isprint(ch))
        {
            _finds_vec.clear();
            _tab_index = 0;

            cout << ch;
            _chars2_queue.push_back(ch);
            ++_num_of_chars2_in_line;
        }
        else if(ch != 0)
            getch_nolock(); // TODO

        if (ch == NEW_LINE)
        {
            auto ret_str = chars2_queue_to_string();
            _chars2_queue.clear();
            _num_of_chars2_in_line = 0;
            return ret_str;
        }

        cout.clear();
        cin.clear();
        fflush(nullptr);
    }
    _chars2_queue.clear();
    _num_of_chars2_in_line = 0;
    cout.clear();
    cin.clear();
    fflush(nullptr);
    return std::string{};
}
