#include "math.h"

inline static int32_t roundReal32ToUInt32(real32 val) {
	uint32_t res = (uint32_t) (val + 0.5f);
	return res;
}

inline static int32_t roundReal32ToInt32(real32 val) {
	int32_t res = (int32_t) (val + 0.5f);
	return res;
}

inline static int32_t floorReal32ToInt32(real32 val) {
	return (int32_t)floorf(val);
}

inline static int32_t truncateReal32ToInt32(real32 val) {
	return (int32_t)(val);
}

inline static real32 sin(real32 angle) {
	
}


inline static real32 cos(real32 angle) {
	
}

inline static real32 atan2(real32 y, real32 x) {
	
}
