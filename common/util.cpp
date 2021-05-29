#include "util.h"

#include <algorithm>
#include <fstream>

namespace TOZ3 {

cstring get_max_bv_val(uint64_t bv_width) {
    big_int max_return = pow((big_int)2, bv_width) - 1;
    return Util::toString(max_return, 0, false);
}

cstring infer_name(const IR::Annotations *annots, cstring default_name) {
    // This function is a bit of a hacky way to infer the true name of a
    // declaration. Since there are a couple of passes that rename but add
    // annotations we can infer the original name from the annotation.
    // not sure if this generalizes but this is as close we can get for now
    for (const auto *anno : annots->annotations) {
        // there is an original name in the form of an annotation
        if (anno->name.name == "name") {
            for (const auto *token : anno->body) {
                // the full name can be a bit more convoluted
                // we only need the last bit after the dot
                // so hack it out
                cstring full_name = token->text;
                // find the last dot
                const char *last_dot =
                    full_name.findlast(static_cast<int>('.'));
                // there is no dot in this string, just return the full name
                if (last_dot == nullptr) {
                    return full_name;
                }
                // otherwise get the index, remove the dot
                auto idx = (size_t)(last_dot - full_name + 1);
                return token->text.substr(idx);
            }
            // if the annotation is a member just get the root name
            if (const auto *member = anno->expr.to<IR::Member>()) {
                return member->member.name;
            }
        }
    }

    return default_name;
}

int exec(const char *cmd, std::stringstream &output) {
    TOZ3::Logger::log_msg(1, "Executing command %s", cmd);
    constexpr int chunk_size = 128;
    std::array<char, chunk_size> buffer{};

    auto *pipe = popen(cmd, "r");

    if (pipe == nullptr) {
        throw std::runtime_error("popen() failed!");
    }
    while (feof(pipe) == 0) {
        if (fgets(buffer.data(), chunk_size, pipe) != nullptr) {
            output << buffer.data();
        }
    }
    return pclose(pipe);
}

// https://stackoverflow.com/a/39097160/3215972
bool compare_files(const cstring &filename1, const cstring &filename2) {
    std::ifstream file1(filename1,
                        std::ifstream::ate |
                            std::ifstream::binary);  // open file at the end
    std::ifstream file2(filename2,
                        std::ifstream::ate |
                            std::ifstream::binary);  // open file at the end
    const std::ifstream::pos_type fileSize = file1.tellg();

    if (fileSize != file2.tellg()) {
        return false;  // different file size
    }

    file1.seekg(0);  // rewind
    file2.seekg(0);  // rewind

    std::istreambuf_iterator<char> begin1(file1);
    std::istreambuf_iterator<char> begin2(file2);

    return std::equal(begin1, std::istreambuf_iterator<char>(),
                      begin2);  // Second argument is end-of-range iterator
}

}  // namespace TOZ3
