#pragma once
static bool Mk64Reverse3DSCanStart(Player* player, struct Controller* controller) {
    if (!Mk64NewItems3DSEnabled()) return func_800388B0(controller) < -0x31;
    return !(controller->button & A_BUTTON) &&
        ((player->kartProps & BACK_UP) ||
         ((controller->button & B_BUTTON) && player->speed * 12.0f <= 0.5f));
}
static bool Mk64Reverse3DSShouldStop(struct Controller* controller) {
    if (!Mk64NewItems3DSEnabled())
        return func_800388B0(controller) >= -0x1D || !(controller->button & B_BUTTON);
    return (controller->button & A_BUTTON) || !(controller->button & B_BUTTON);
}
