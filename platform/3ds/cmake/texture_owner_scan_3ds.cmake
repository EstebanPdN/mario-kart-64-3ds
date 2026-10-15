# Bound searches by populated entries, not the full 512-entry capacity, while
# retaining the exact same shared owners and holes. No new allocation or GPU reuse.
function(mk64_owner_scan_replace old new)
    string(FIND "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}" "${old}" found)
    if(found LESS 0)
        message(FATAL_ERROR "Texture owner scan anchor changed: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" replaced "${SPAGHETTIKART_3DS_INTERPRETER_TEXT}")
    set(SPAGHETTIKART_3DS_INTERPRETER_TEXT "${replaced}" PARENT_SCOPE)
endfunction()
mk64_owner_scan_replace("    sMk64BlendedTextureOwners3DS;"
    "    sMk64BlendedTextureOwners3DS;\nstatic size_t sMk64BlendedTextureOwnerEnd3DS = 0;")
mk64_owner_scan_replace("    for (const auto& entry : sMk64BlendedTextureOwners3DS) {\n        if (entry.data == data) return entry.resource;"
    "    for (size_t i = 0; i < sMk64BlendedTextureOwnerEnd3DS; ++i) {\n        const auto& entry = sMk64BlendedTextureOwners3DS[i];\n        if (entry.data == data) return entry.resource;")
mk64_owner_scan_replace("    for (auto& entry : sMk64BlendedTextureOwners3DS) {\n        if (entry.data == data || entry.data == nullptr) {"
    "    for (size_t i = 0; i < sMk64BlendedTextureOwnerEnd3DS; ++i) {\n        auto& entry = sMk64BlendedTextureOwners3DS[i];\n        if (entry.data == data || entry.data == nullptr) {")
mk64_owner_scan_replace("    return false;\n}\n\nstatic void Mk64UnregisterBlendedTextureOwner3DS"
    "    if (sMk64BlendedTextureOwnerEnd3DS < sMk64BlendedTextureOwners3DS.size()) {\n        sMk64BlendedTextureOwners3DS[sMk64BlendedTextureOwnerEnd3DS++] = {data, resource};\n        return true;\n    }\n    return false;\n}\n\nstatic void Mk64UnregisterBlendedTextureOwner3DS")
mk64_owner_scan_replace("    for (auto& entry : sMk64BlendedTextureOwners3DS) {\n        if (entry.data == data) {\n            entry = {};"
    "    for (size_t i = 0; i < sMk64BlendedTextureOwnerEnd3DS; ++i) {\n        auto& entry = sMk64BlendedTextureOwners3DS[i];\n        if (entry.data == data) {\n            entry = {};\n            while (sMk64BlendedTextureOwnerEnd3DS && !sMk64BlendedTextureOwners3DS[sMk64BlendedTextureOwnerEnd3DS-1].data)\n                --sMk64BlendedTextureOwnerEnd3DS;")
mk64_owner_scan_replace("    for (auto& owner : sMk64BlendedTextureOwners3DS) owner = {};"
    "    for (auto& owner : sMk64BlendedTextureOwners3DS) owner = {};\n    sMk64BlendedTextureOwnerEnd3DS = 0;")
