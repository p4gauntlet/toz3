#include "util.h"

namespace TOZ3 {

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

}  // namespace TOZ3
