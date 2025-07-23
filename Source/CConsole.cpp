/*
==================================================================

						CConsole Main Code

==================================================================
*/

#include "CConsole.h"

#include <iostream>
#include <iomanip>

#include "SDL.h"
#include "Utils.hpp"

CConsole CConsole::Console;

// https://discourse.libsdl.org/t/detect-console-window-close-windows/20557/4
BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType) {
	if (dwCtrlType == CTRL_CLOSE_EVENT) {
		if (auto onClose = CConsole::Console.GetOnClose(); onClose)
			onClose();
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////
//
//	CConsole::CConsole()
//		Constructor
//
///////////////////////////////////////////////////////////////
CConsole::CConsole() {
}

///////////////////////////////////////////////////////////////
//
//	CConsole::OnInit()
//		Initializes the console window and attaches it to a
//		HANDLE.
//
///////////////////////////////////////////////////////////////
void CConsole::OnInit() {
	AllocConsole();
	AttachConsole(ATTACH_PARENT_PROCESS);

	SetConsoleTitleA("Debug Console");

	in = GetStdHandle(STD_INPUT_HANDLE);
	out = GetStdHandle(STD_OUTPUT_HANDLE);

	bufferPos = 0;

	SetConsoleCtrlHandler(ConsoleHandlerRoutine, true);

	Print("CConsole::OnInit(): Console initialized", MSG_DIAG);
}

///////////////////////////////////////////////////////////////
//
//	CConsole::OnCleanup()
//		Frees the console
//
///////////////////////////////////////////////////////////////
void CConsole::OnCleanup() {
	FreeConsole();
}

///////////////////////////////////////////////////////////////
//
//	CConsole::Print(std::string, unsigned)
//		Prints a message to the console, with messageType setting
//		the color and formatting of the output
//
///////////////////////////////////////////////////////////////
void CConsole::Print(std::string message, unsigned messageType) {
	Print(std::wstring(message.begin(), message.end()), messageType);
}

///////////////////////////////////////////////////////////////
//
//	CConsole::Print(std::string, unsigned)
//		Prints a message to the console, with messageType setting
//		the color and formatting of the output
//
///////////////////////////////////////////////////////////////
void CConsole::Print(std::wstring message, unsigned messageType) {
	std::unique_lock lock(mutex, std::defer_lock);

	if (!processingCommands)
		lock.lock();

	unsigned char color;
	switch(messageType) {
		case MSG_DIAG:
			color = DARKCYAN;
			break;
		case MSG_ALERT:
			color = DARKYELLOW;
			break;
		case MSG_ERROR:
			color = DARKRED;
			break;
		case MSG_NORMAL:
		default:
			color = GREY;
			break;
	}
	SetConsoleTextAttribute(out, color);

	std::wstringstream messageStream;
	messageStream << L'\r' << std::setw(2) << std::setfill(L'0') << SDL_GetTicks() / 1000 / 60 << ':' << std::setw(2) << std::setfill(L'0') << (SDL_GetTicks() / 1000)%60 << " - " << message;
	std::wstring newMessage = messageStream.str();

	DWORD numWritten;
	WriteConsoleW(out, newMessage.c_str(), static_cast<DWORD>(newMessage.length()), &numWritten, NULL);

	SetConsoleTextAttribute(out, WHITE);
	WriteConsoleW(out, &writeChars[RETURN], 4, &numWritten, NULL);
	WriteConsoleW(out, buffer, bufferPos, &numWritten, NULL);
}

///////////////////////////////////////////////////////////////
//
//	CConsole::Read(bool initial)
//		Reads from the input buffer and parses the input. Must
//		be called every frame or else input will not be scraped.
//
///////////////////////////////////////////////////////////////
void CConsole::Read(bool initial) {
	std::unique_lock lock(mutex);

	/*
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT);
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), WHITE);
	DWORD numRead;
	CONSOLE_READCONSOLE_CONTROL control;

	ReadConsole(GetStdHandle(STD_INPUT_HANDLE), &buffer[bufferPos], 1, &numRead, &control);
	if(numRead > 0) {
		if(buffer[bufferPos] == '\n') {
			// check commands

			bufferPos = 0;
		}
		else
			bufferPos++;
	}
	*/

	//SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
	//SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), WHITE);

	/*
	if(initial) {
		//WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), &writeChars[RETURN], 4, &numWritten, NULL);
	}
	*/
	GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE), &numEvents);

	if(numEvents == 0)
		return;

	INPUT_RECORD *records = new INPUT_RECORD[numEvents];

	ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), records, numEvents, &numRead);
	for(DWORD i = 0; i < numRead; i++) {
		switch(records[i].EventType) {
			case KEY_EVENT: {
				if(records[i].Event.KeyEvent.bKeyDown) {
					switch(records[i].Event.KeyEvent.uChar.UnicodeChar) {
						/*
						case L'`':
							ClearBuffer();
							WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), &writeChars[CARRIAGERETURN], 1, NULL, NULL);
							//CApp::App->Pause();
							ShowWindow(GetConsoleWindow(), SW_HIDE);
							break;
						*/
						case L'\b':
							if(bufferPos > 0) {
								buffer[bufferPos-1] = L'\0';
								bufferPos -= 1;
								WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), &writeChars[BACKSPACE], 3, &numWritten, NULL);
							}
							break;
						case L'\r':
							WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), &writeChars[RETURN], 4, &numWritten, NULL);
							ProcessBuffer();
							break;

						default:
							if(bufferPos < BUFFERSIZE && records[i].Event.KeyEvent.uChar.UnicodeChar != L'\0') {
								buffer[bufferPos] = records[i].Event.KeyEvent.uChar.UnicodeChar;

								WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), &buffer[bufferPos], 1, &numWritten, NULL);

								bufferPos++;
							}
							else if(bufferPos >= BUFFERSIZE)
								MessageBoxA(NULL, "Console Buffer is full!", "Error!", MB_OK | MB_ICONERROR | MB_TOPMOST);
					}
				}

				break;
			}
		}
	}

	delete[] records;
}

void CConsole::AddCommands(std::map<std::wstring, Command> &&commands) {
	this->commands.merge(commands);
}

void CConsole::SetOnClose(std::function<void()> &&f) {
	onClose = std::move(f);
}

const std::function<void()> &CConsole::GetOnClose() const {
	return onClose;
}

///////////////////////////////////////////////////////////////
//
//	CConsole::ProcessBuffer()
//		Processes what is currently in the input buffer (after
//		a return hit) and executes the approriate command.
//
///////////////////////////////////////////////////////////////
void CConsole::ProcessBuffer() {
	auto split = Fetcko::Utils::Split<wchar_t>(buffer, L' ');

	ClearBuffer();

	if (!split.empty()) {
		processingCommands = true;

		if (auto iter = commands.find(split[0]); iter != commands.end())
			iter->second(split);

		processingCommands = false;
	}

	// Generic commands below
}

///////////////////////////////////////////////////////////////
//
//	CConsole::ClearBuffer()
//		Flushes the inputbuffer.
//
///////////////////////////////////////////////////////////////
void CConsole::ClearBuffer() {
	memset(buffer, 0, bufferPos * sizeof(wchar_t));

	bufferPos = 0;
}