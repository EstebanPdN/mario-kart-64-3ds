// Resolve the immutable handler precedence at compile time. Commands themselves
// remain live: operands, resource addresses and ucode switches are read on every
// execution, including self-modifying and multiword display lists.
static constexpr auto Mk64BuildDispatch3DS() {
    std::array<std::array<GfxOpcodeHandlerFunc, 256>, ucode_max + 1> tables{};
    for (std::size_t ucode = 0; ucode < tables.size(); ++ucode) {
        for (std::size_t op = 0; op < 256; ++op) {
            const auto opcode = static_cast<int8_t>(op);
            if (otrHandlers.contains(opcode)) {
                tables[ucode][op] = otrHandlers.at(opcode).second;
            } else if (rdpHandlers.contains(opcode)) {
                tables[ucode][op] = rdpHandlers.at(opcode).second;
            } else if (ucode < ucode_handlers.size() &&
                       ucode_handlers[ucode]->contains(opcode)) {
                tables[ucode][op] = ucode_handlers[ucode]->at(opcode).second;
            }
        }
    }
    return tables;
}
static constexpr auto sMk64Dispatch3DS = Mk64BuildDispatch3DS();

static GfxOpcodeHandlerFunc Mk64OpcodeHandler3DS(int8_t opcode) {
    const auto ucode = static_cast<std::size_t>(ucode_handler_index);
    return sMk64Dispatch3DS[ucode < ucode_max ? ucode : ucode_max]
                            [static_cast<uint8_t>(opcode)];
}
