#pragma once
// Cancellation terminates this isolated worker process; only its private
// temporary directory is affected. No console globals or ROM bytes are linked.
inline bool Mk64Extraction3DSShouldCancel() { return false; }
void Mk64DesktopProgress(const char* path);
