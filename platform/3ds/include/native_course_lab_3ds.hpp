#pragma once
namespace Fast { class Interpreter; }
extern "C" void Mk64NativeLuigiLabBoot();
extern "C" bool Mk64NativeLuigiLabDispatch(Fast::Interpreter*,const void*);
