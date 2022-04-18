#pragma once

#include <string_view>
#include <streambuf>
#include <istream>
#include <memory>
#include <cstring>

enum class Seek {
    Current,
    End,
    Set
};

struct MemoryBuffer {
    MemoryBuffer() {}
    MemoryBuffer(const std::vector<uint8_t>& new_data) : data(new_data) {}

    void seek(const size_t pos, const Seek seek_type) {
        switch(seek_type) {
            case Seek::Current:
                position += pos;
                break;
            case Seek::End:
                position = size() - pos;
                break;
            case Seek::Set:
                position = pos;
                break;
        }
    }

    template<typename T>
    void write(const T& t) {
        size_t end = position + sizeof(T);
        if(end > data.size())
            data.resize(end);

        memcpy(data.data() + position, &t, sizeof(T));

        position = end;
    }



    size_t size() const {
        return data.size();
    }

    size_t current_position() const {
        return position;
    }

    std::vector<uint8_t> data;

private:
    size_t position = 0;
};

template<>
void MemoryBuffer::write<std::vector<uint8_t>>(const std::vector<uint8_t>& t) {
    size_t end = position + (sizeof(uint8_t) * t.size());
    if(end > data.size())
        data.resize(end);

    data.insert(data.begin() + position, t.begin(), t.end());
    position = end;
}

struct MemorySpan {
    MemorySpan(const MemoryBuffer& new_buffer) : buffer(new_buffer) {}

    std::istream read_as_stream() {
        auto char_data = cast_data<char>();
        mem = std::make_unique<membuf>(char_data, char_data + size());
        return std::istream(mem.get());
    }

    template<typename T>
    void read(T* t) {
        *t = *reinterpret_cast<const T*>(buffer.data.data() + position);
        position += sizeof(T);
    }

    template<typename T>
    void read(T* t, const size_t size) {
        *t = *reinterpret_cast<const T*>(buffer.data.data() + position);
        position += size;
    }

    template<typename T>
    void read_structures(std::vector<T>* vector, const size_t count) {
        vector->resize(count);
        for(size_t i = 0; i < count; i++)
            read(&vector->at(i));
    }

    template<typename T>
    void read_array(T* array, const size_t count) {
        for(size_t i = 0; i < count; i++)
            read((array + i));
    }

    void seek(const size_t pos, const Seek seek_type) {
        switch(seek_type) {
            case Seek::Current:
                position += pos;
                break;
            case Seek::End:
                position = buffer.size() - pos;
                break;
            case Seek::Set:
                position = pos;
                break;
        }
    }

    size_t size() const {
        return buffer.data.size();
    }

    size_t current_position() const {
        return position;
    }

private:
    const MemoryBuffer& buffer;

    struct membuf : std::streambuf {
        inline membuf(char* begin, char* end) {
            this->setg(begin, begin, end);
        }
    };

    template<typename T>
    T* cast_data() {
        return (T*)(buffer.data.data());
    }

    std::unique_ptr<membuf> mem;

    size_t position = 0;
};

void write_buffer_to_file(const MemoryBuffer& buffer, std::string_view path);