#ifndef TOZ3_COMMON_PACKET_H_
#define TOZ3_COMMON_PACKET_H_

#include "type_base.h"

namespace P4::ToZ3 {

P4Z3Instance *execute_packet_method(Z3Visitor *visitor, const IR::Method *method);

}  // namespace P4::ToZ3

#endif  // TOZ3_COMMON_PACKET_H_
