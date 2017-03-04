#include "../runtime.h"
#include "vector.h"

VectorAllocator<UnsafeBaseExpressionRef> TemporaryRefVector::s_allocator;
VectorAllocator<SortKey> SortKeyVector::s_allocator;
VectorAllocator<size_t> IndexVector::s_allocator;
