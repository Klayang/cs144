#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
    long unsigned int capacity = output.available_capacity();
    if (first_index <= current_index) {
        if (first_index + data.length() > current_index) {
            int start = current_index - first_index;
            int len = min(data.length() - start, capacity);
            output.push(data.substr(start, len));
            current_index += len;
            update_buffer(output);
        }
    }
    else if (first_index < current_index + capacity) {
        int gap = first_index - current_index;
        buffer_data(first_index, data.substr(0, min(data.length(), capacity - gap)));
    }

    if (is_last_substring) hasTouchedLast = true;
    if (hasTouchedLast && buffered_string_indices.empty()) output.close();
}

uint64_t Reassembler::bytes_pending() const
{
    // Your code here.
    return number_of_buffered_bytes;
}

void Reassembler::update_buffer(Writer& output) {
    auto itr_index = buffered_string_indices.begin();
    auto itr_data = buffered_strings.begin();
    for (; itr_index != buffered_string_indices.end();) {
        if (current_index < *itr_index) break;
        else if (current_index < *itr_index + (*itr_data).length()) {
            string data = *itr_data;

            int start = current_index - *itr_index;
            int len = data.length() - start;
            number_of_buffered_bytes -= (*itr_data).length();
            output.push(data.substr(start, len));
            current_index += len;

            itr_index = buffered_string_indices.erase(itr_index);
            itr_data = buffered_strings.erase(itr_data);
        }
        else {
            number_of_buffered_bytes -= (*itr_data).length();
            itr_index = buffered_string_indices.erase(itr_index);
            itr_data = buffered_strings.erase(itr_data);
        }
    }
}

void Reassembler::buffer_data(long unsigned int first_index, string data) {
    auto itr_index = buffered_string_indices.begin();
    auto itr_data = buffered_strings.begin();
    for (; itr_index != buffered_string_indices.end(); ++itr_index, ++itr_data) {
        long unsigned int start_index = *itr_index;
        string current_str = *itr_data;
        if (first_index < start_index) {
            itr_index = buffered_string_indices.insert(itr_index, first_index);
            long unsigned int gap = start_index - first_index;
            if (data.length() <= gap) {
                itr_data = buffered_strings.insert(itr_data, data.substr(0, data.length()));
                number_of_buffered_bytes += data.length();
                data = "";
                break;
            }
            else {
                itr_data = buffered_strings.insert(itr_data, data.substr(0, gap));
                data = data.substr(gap, data.length() - gap);
                number_of_buffered_bytes += gap;
                first_index = start_index;
            }
        }
        if (first_index + data.length() <= start_index + current_str.length()) {
            data = "";
            break;
        }
        if (start_index + current_str.length() > first_index) {
            long unsigned int gap = start_index + current_str.length() - first_index;
            data = data.substr(gap, data.length() - gap);
            first_index += gap;
        }
    }
    if (data.length() != 0) {
        buffered_string_indices.push_back(first_index);
        buffered_strings.push_back(data);
        number_of_buffered_bytes += data.length();
    }
}