#include "MessageBox.h"
#include <Windows.h>
#include <print>

namespace Utils
{
	void ErrorMessageBox(const char* title, const char* message)
	{
		MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR | MB_TOPMOST);
		std::println(stderr, "[ERROR] {} - {}", title, message);
	}

	void ErrorMessageBox(const char* message)
	{
		ErrorMessageBox("Error", message);
	}

	void WarningMessageBox(const char* title, const char* message)
	{
		MessageBoxA(nullptr, message, title, MB_OK | MB_ICONWARNING | MB_TOPMOST);
		std::println(stderr, "[WARNING] {} - {}", title, message);
	}

	void WarningMessageBox(const char* message)
	{
		WarningMessageBox("Warning", message);
	}
} // namespace Utils
