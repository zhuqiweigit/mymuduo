#pragma once
#include <vector>
#include <string>
#include <algorithm>

class Buffer{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize):
        buffer_(kCheapPrepend + initialSize), readIndex_(kCheapPrepend), writeIndex_(kCheapPrepend){

    }

    size_t readableBytes()const{
        return writeIndex_ - readIndex_;
    }

    size_t writeableBytes()const{
        return buffer_.size() - writeIndex_;
    }

    size_t prependableBytes()const{
        return readIndex_;
    }

    const char* peek()const{
        return begin() + readIndex_;
    }

    void retrieve(size_t len){
        if(len < readableBytes()){
            readIndex_ += len;
        }else{
            retrieveAll();
        }
    }

    std::string retrieveAsString(size_t len){
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes());
    }

    void ensureWriteableBytes(size_t len){
        if(writeableBytes() < len){
            makeSpace(len);
        }
    }

    void append(const char* data, size_t len){
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writeIndex_ += len;
    }


    char* beginWrite(){
        return begin() + writeIndex_;
    }

    const char* beginRead()const{
        return begin() + readIndex_;
    }

    size_t readFd(int fd, int* saveErrno);

    size_t writeFd(int fd, int* saveErrno);

    void retrieveAll(){
        readIndex_ = writeIndex_ = kCheapPrepend;
    }

private:

    void makeSpace(size_t len){
        if(writeableBytes() + prependableBytes() - kCheapPrepend < len){
            buffer_.resize(writeIndex_ + len);
        }else{
            size_t readable = readableBytes();
            std::copy(buffer_.begin() + readIndex_, buffer_.begin() + writeIndex_, buffer_.begin() + kCheapPrepend);
            readIndex_ = kCheapPrepend;
            writeIndex_ = readIndex_ + readable;
        }
    }

    char* begin(){
        return &*buffer_.begin();
    }

    const char* begin()const{
        return &*buffer_.begin();
    }
    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
};