# Retain only the inputs which actually determine CPU Z/W. Apply after native
# geometry's insertion; preserve its small-load bypass and arithmetic order.
mk64_static_replace(
    "#include \"original_fog_3ds.hpp\""
    "#include \"original_fog_3ds.hpp\"\n#include \"position_cache_3ds.hpp\""
    "compact position cache include")
mk64_static_replace(
    "::mk64_3ds::BeginFast3DPositionBlock("
    "::mk64_3ds::BeginFast3DCompactPositionBlock("
    "compact position cache call")
