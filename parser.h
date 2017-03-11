#ifndef PARSER_H
#define PARSER_H

#include <iostream>
#include <string>
#include <map>
#include <mutex>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <condition_variable>
#include <queue>
#include <atomic>

//#define DEBUG true

using namespace std;

#ifdef DEBUG
atomic_int BLOCKS(0),BLOCKS_IN(0),BLOCKS_OUT(0),MAPS_IN(0),MAPS_OUT(0), NOT_MESS(0);
#endif // DEBUG


class parser
{
private:
    atomic<bool>               a_q_blocks_empty{ false };
    atomic<bool>               a_is_last_block { false };
    atomic<bool>               a_ready_to_process { false };
    mutex                      fin_map_mut;
    unordered_map<string, int> fin_map;
    mutex                      q_blocks_mut;
    condition_variable         block_cond;
    queue<vector<string>>      q_blocks;

    int TOTAL_TH       = thread::hardware_concurrency();
    int MAKE_BLOCKS_TH = 1;
    int PROC_BLOCKS_TH = TOTAL_TH - 1;
    size_t block_size  = 256;


    void save(const string& file_dest, const unordered_map<string, int>& imap_)
    {
        ofstream fout(file_dest);

        map<string, int> imap(imap_.begin(), imap_.end());

        for (const auto& s : imap)
            fout << s.first << " - " << s.second << '\n';

        fout.close();
    }

    inline auto add_map(unordered_map<string, int> new_map)
    {
        lock_guard<mutex> l(fin_map_mut);
            for (const auto& e : new_map)
                fin_map[e.first] += e.second;
    };

    inline auto add_map(unordered_map<string, int>& dest_map, unordered_map<string, int> src_map)
    {
        for (const auto& e : src_map)
            dest_map[e.first] += e.second;
    }

    auto parse_str_new_map(vector<string>& l_block)
    {
        unordered_map<string, int> local_map;
        //local_map.reserve(res);
        for(auto& line : l_block)
        {
            transform(line.begin(), line.end(), line.begin(), ::tolower);
            istringstream stream(line);
            string word;
            while (stream >> word)
            {
                int len = word.length();
                int beg_ = -1, end_ = len;
                while (!isalpha(word[++beg_]) && beg_ < len);
                if (beg_ == len)
                    continue;
                while (!isalpha(word[--end_]));
                auto ibeg = word.begin();
                ++local_map[{ibeg + beg_, ibeg + end_ + 1}];
            }
        }
        return local_map;
    }

    void make_blocks(const string& file_name)
    {
        ifstream fin(file_name);
        vector<string> block;
        block.reserve(block_size);
        string line;
        size_t l_count = 0;

        while (getline(fin, line))
        {
            block.emplace_back(move(line));
            if (++l_count == block_size)
            {
                #ifdef DEBUG
                BLOCKS++;
                #endif // DEBUG
                l_count = 0;
                {
                    lock_guard<mutex> lg(q_blocks_mut);
                        q_blocks.push(move(block));
                }
                block_cond.notify_one();
            }
        }
        //last block
        if (l_count != 0)
        {
            #ifdef DEBUG
            BLOCKS++;
            #endif // DEBUG
            lock_guard<mutex> lg(q_blocks_mut);
                q_blocks.push(move(block));
                block_cond.notify_one();
        }
        a_is_last_block.store(true);
    }

    void process_blocks(int id)
    {
        bool have_data = false;
        do
        {
            if(!have_data)
            {
                unique_lock<mutex> ull(q_blocks_mut);
                    block_cond.wait(ull, [&]() { return !q_blocks.empty(); });
                ull.unlock();
                have_data = true;
            }
            else
            {
                unique_lock<mutex> ull(q_blocks_mut);
                    have_data = !q_blocks.empty();
                    if(have_data)
                    {
                            auto l_block = move(q_blocks.front());
                            q_blocks.pop();
                        ull.unlock();
                        #ifdef DEBUG
                        BLOCKS_IN++;
                        #endif // DEBUG
                        add_map(parse_str_new_map(l_block));
                    }
            }
        }
        while (have_data || !a_is_last_block);
    }

public:
    void word_count(const string& file_name, const string& file_dest)
    {
        thread t_make(make_blocks, this, file_name);

        vector<thread> t_process_blocks;
        for (int i = 0; i < PROC_BLOCKS_TH; ++i)
            t_process_blocks.emplace_back(&parser::process_blocks, this, i);

        t_make.join();
        t_process_blocks.emplace_back(&parser::process_blocks, this, 3);

        for (auto& t : t_process_blocks)
            t.join();

        save(file_dest, fin_map);

    #ifdef DEBUG
        bool b = BLOCKS_IN + BLOCKS_OUT == BLOCKS;
        cout << "BLOCKS = " << BLOCKS
        << "\nBLOCKS_IN = " << BLOCKS_IN
        << "\nBLOCKS_OUT = " << BLOCKS_OUT
        << "\n------------------------------------  correct = " << b << endl;
    #endif // DEBUG
    }
};

#endif // PARSER_H
