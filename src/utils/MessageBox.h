#pragma once

namespace Utils
{
	/// Shows a critical error message box with an error icon.
	/// Also logs the error to stderr.
	void ErrorMessageBox(const char* title, const char* message);

	/// Shows a critical error message box with an error icon and default "Error" title.
	/// Also logs the error to stderr.
	void ErrorMessageBox(const char* message);

	/// Shows a warning message box with a warning icon.
	/// Also logs the warning to stderr.
	void WarningMessageBox(const char* title, const char* message);

	/// Shows a warning message box with a warning icon and default "Warning" title.
	/// Also logs the warning to stderr.
	void WarningMessageBox(const char* message);

} // namespace Utils
