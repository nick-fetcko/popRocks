/*
==================================================================

						CConsole Header Code

==================================================================
*/

//#ifdef _DEBUG

#pragma once

#include <functional>
#include <map>
#include <string>
#include <sstream>
#include <mutex>
#include <optional>

#include <windows.h>

#define BUFFERSIZE 256 /* Size of input buffer */

enum MessageTypes	{MSG_NORMAL, MSG_DIAG, MSG_ALERT, MSG_ERROR};
enum ConsoleColors	{BLACK, DARKBLUE, DARKGREEN, DARKCYAN, DARKRED, DARKMAGENTA, DARKYELLOW, DEFAULT, GREY, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE};
enum SpecialChars {BACKSPACE = 0, RETURN = 3, CARRIAGERETURN = 7};

///////////////////////////////////////////////////////////////
//
//	Class CConsole
//		Controls debug console initializing and IO
//
///////////////////////////////////////////////////////////////
class CConsole {
public:
	using Command = std::function<void(const std::vector<std::wstring> &)>;

//=============================================================
// Public Variables
//=============================================================	
	static CConsole Console;

//=============================================================
// Public Functions
//=============================================================	
	CConsole();

	void OnInit();
	void OnCleanup();

	void Print(std::string message, unsigned messageType);
	void Print(std::wstring message, unsigned messageType);
	void Read(bool initial = false);

	void AddCommands(std::map<std::wstring, Command> &&commands);
	void SetOnClose(std::function<void()> &&f);

	const std::function<void()> &GetOnClose() const;

private:
//=============================================================
// Private Variables
//=============================================================
	std::function<void()> onClose;

	void ProcessBuffer();
	void ClearBuffer();

	HANDLE in = nullptr;
	HANDLE out = nullptr;

	wchar_t buffer[BUFFERSIZE] = { 0 };
	unsigned bufferPos = 0;
	const wchar_t *writeChars = L"\b \b\n > \r";

	DWORD numEvents = 0;
	DWORD numRead = 0;
	DWORD numWritten = 0;

	std::map<std::wstring, Command> commands;

	std::mutex mutex;

	// Keep track of when we're processing commands
	// because the mutex will already be locked.
	//
	// Any log statements printed during the comamnds
	// themselves don't need to lock again.
	std::atomic<bool> processingCommands = false;
};

//#endif