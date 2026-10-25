#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstring>

uint8_t CoA::Read::U8(CDataStore* msg) {
    uint8_t value = 0;
    msg->Get(value);
    return value;
}

uint16_t CoA::Read::U16(CDataStore* msg) {
    uint16_t value = 0;
    msg->Get(value);
    return value;
}

uint32_t CoA::Read::U32(CDataStore* msg) {
    uint32_t value = 0;
    msg->Get(value);
    return value;
}

int32_t CoA::Read::I32(CDataStore* msg) {
    int32_t value = 0;
    msg->Get(value);
    return value;
}

uint64_t CoA::Read::U64(CDataStore* msg) {
    uint64_t value = 0;
    msg->Get(value);
    return value;
}

float CoA::Read::F32(CDataStore* msg) {
    float value = 0.0f;
    msg->Get(value);
    return value;
}

std::string CoA::Read::Str(CDataStore* msg) {
    std::string out;

    while (!CoA::Read::Failed(msg)) {
        uint8_t c = 0;
        msg->Get(c);

        if (CoA::Read::Failed(msg) || c == 0) {
            break;
        }

        out.push_back(static_cast<char>(c));
    }

    return out;
}

std::string CoA::Read::Name(CDataStore* msg) {
    uint32_t length = CoA::Read::U32(msg);

    if (length > CoA::Read::Remaining(msg)) {
        // What the getter does when the bytes are not there; checked first so a lying length allocates nothing.
        msg->m_read = msg->m_size + 1;
        return std::string();
    }

    std::string out(length, '\0');

    if (length) {
        msg->GetData(&out[0], length);
    }

    return out;
}

void CoA::Read::CStr(CDataStore* msg, char* out, uint32_t max) {
    memset(out, 0, max);
    msg->GetString(out, max);
    out[max - 1] = '\0';
}

void CoA::Read::Data(CDataStore* msg, void* out, uint32_t n) {
    memset(out, 0, n);
    msg->GetData(out, n);
}

// The original passes the source length by address where 0x778180 takes it by value, so its zlib stream gets a stack
// address as its input size; a well-formed stream stops at its end either way, and this passes the real length.
bool CoA::Read::Inflate(CDataStore* msg, std::vector<uint8_t>& out) {
    const uint32_t total = CoA::Read::Remaining(msg);
    std::vector<uint8_t> in(total);

    if (total) {
        msg->GetData(in.data(), total);
    }

    if (total < 4) {
        return false;
    }

    uint32_t size;
    memcpy(&size, in.data(), 4);
    out.resize(size);

    if (zlib_uncompress(out.data(), &size, in.data() + 4, total - 4) != 0) {
        return false;
    }

    out.resize(size);
    return true;
}
