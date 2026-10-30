#pragma once

#include <string>

// Speech output through Prism only (project rule: no other speech library or
// fallback). Prism runs on its own worker thread so a slow screen reader
// never stalls the game thread.
void SpeechStart();
void SpeechStop();

// interrupt = true drops anything queued and cuts off current speech; use it
// for focus changes so stale selections are never read out.
void Speak(const std::wstring& text, bool interrupt);
