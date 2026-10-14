#pragma once
// Recording UI facade for the small public ImGui API used by the production
// menu. The engine cvar types and client interface are real project headers.
namespace ImGui
{
bool Begin(const char *, bool *);
void End();
bool RadioButton(const char *, bool);
void TextWrapped(const char *, ...);
}
