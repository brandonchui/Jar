/// Common utilities module for shared shader code.
/// This module provides PBR functions and bindless descriptor helpers.

module common;

__include "common/pbr";

#ifdef ENABLE_BINDLESS
__include "common/bindless-helpers";
#endif
