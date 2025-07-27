#pragma once

#include <cstddef>

namespace az {

void *aligned_alloc(size_t alignment, size_t size);
void aligned_free(void *ptr);

}  // namespace az
