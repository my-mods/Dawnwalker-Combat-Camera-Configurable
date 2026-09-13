#pragma once
#include "Policy.hpp"
#include <string>
namespace CombatCamera {
bool start(std::wstring& error);
void configure(Settings);
void deactivate();
void stop();
}
