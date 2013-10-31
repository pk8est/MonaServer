/*
Copyright 2013 Mona - mathieu.poux[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

This file is a part of Mona.
*/


#include "Mona/ServerApplication.h"
#include "Mona/Logs.h"
#include "Mona/Exceptions.h"
#include "Mona/HelpFormatter.h"
#if defined(_WIN32)
#include "Mona/WinService.h"
#include "Mona/WinRegistryKey.h"
#else
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <fstream>
#endif


using namespace std;

namespace Mona {



#if defined(_WIN32)
Event					TerminateSignal::_Terminate;
SERVICE_STATUS			TerminateSignal::_ServiceStatus;
SERVICE_STATUS_HANDLE	TerminateSignal::_ServiceStatusHandle = 0;
#endif

ServerApplication*		ServerApplication::_PThis(NULL);


#if defined(_WIN32)

BOOL TerminateSignal::ConsoleCtrlHandler(DWORD ctrlType) {
	switch (ctrlType) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		_Terminate.set();
		return TRUE;
	default:
		return FALSE;
	}
}

void TerminateSignal::ServiceControlHandler(DWORD control) {
	switch (control) {
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);
		_Terminate.set();
		return;
	case SERVICE_CONTROL_INTERROGATE:
		break;
	}
	SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);
}

void TerminateSignal::wait() {
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
	_Terminate.wait();
}

TerminateSignal::TerminateSignal() {
	memset(&_ServiceStatus, 0, sizeof(_ServiceStatus));
	_ServiceStatusHandle = RegisterServiceCtrlHandlerA("", ServiceControlHandler);
	if (!_ServiceStatusHandle) {
		FATAL_THROW("Cannot register service control handler");
		return;
	}
	_ServiceStatus.dwServiceType = SERVICE_WIN32;
	_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	_ServiceStatus.dwWin32ExitCode = 0;
	_ServiceStatus.dwServiceSpecificExitCode = 0;
	_ServiceStatus.dwCheckPoint = 0;
	_ServiceStatus.dwWaitHint = 0;
	SetServiceStatus(_ServiceStatusHandle, &_ServiceStatus);
}


void ServerApplication::ServiceMain(DWORD argc, LPTSTR* argv) {

	_PThis->setBool("application.runAsService", true);
	_PThis->_isInteractive = false;

	TerminateSignal terminateSignal;

	try {
		if (_PThis->init(argc, argv)) {
			TerminateSignal::_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(TerminateSignal::_ServiceStatusHandle, &TerminateSignal::_ServiceStatus);
			int rc = _PThis->main(terminateSignal);
			TerminateSignal::_ServiceStatus.dwWin32ExitCode = rc ? ERROR_SERVICE_SPECIFIC_ERROR : 0;
			TerminateSignal::_ServiceStatus.dwServiceSpecificExitCode = rc;
		}
	} catch (exception& ex) {
		FATAL(ex.what());
		TerminateSignal::_ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		TerminateSignal::_ServiceStatus.dwServiceSpecificExitCode = EXIT_SOFTWARE;
	} catch (...) {
		FATAL("Unknown error");
		TerminateSignal::_ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		TerminateSignal::_ServiceStatus.dwServiceSpecificExitCode = EXIT_SOFTWARE;
	}
	TerminateSignal::_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(TerminateSignal::_ServiceStatusHandle, &TerminateSignal::_ServiceStatus);
}


int ServerApplication::run(int argc, char** argv) {
	try {
		if (!hasConsole() && isService())
			return 0;
		if (!init(argc, argv))
			return EXIT_OK;
		Exception ex;
		if (hasArgument("registerService")) {
			bool success = false;
			EXCEPTION_TO_LOG(success = registerService(ex), "RegisterService")
			if (success)
				NOTE("The application has been successfully registered as a service");
			return EXIT_OK;
		}
		if (hasArgument("unregisterService")) {
			bool success = false;
			EXCEPTION_TO_LOG(success = unregisterService(ex), "UnregisterService")
			if (success)
				NOTE("The application is no more registered as a service");
			return EXIT_OK;
		}
		TerminateSignal terminateSignal;
		return main(terminateSignal);
	} catch (exception& ex) {
		FATAL(ex.what());
		return EXIT_SOFTWARE;
	} catch (...) {
		FATAL("Unknown error");
		return EXIT_SOFTWARE;
	}
}


bool ServerApplication::isService() {
	SERVICE_TABLE_ENTRY svcDispatchTable[2];
	svcDispatchTable[0].lpServiceName = "";
	svcDispatchTable[0].lpServiceProc = ServiceMain;
	svcDispatchTable[1].lpServiceName = NULL;
	svcDispatchTable[1].lpServiceProc = NULL; 
	return StartServiceCtrlDispatcherA(svcDispatchTable) != 0; 
}


bool ServerApplication::hasConsole() {
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	return hStdOut != INVALID_HANDLE_VALUE && hStdOut != NULL;
}


bool ServerApplication::registerService(Exception& ex) {
	string name, path;
	getString("application.baseName", name);
	getString("application.path", path);
	
	WinService service(name);
	if (_displayName.empty())
		service.registerService(ex,path);
	else
		service.registerService(ex, path, _displayName);
	if (ex)
		return false;
	if (_startup == "auto")
		service.setStartup(ex,WinService::AUTO_START);
	else if (_startup == "manual")
		service.setStartup(ex,WinService::MANUAL_START);
	if (!_description.empty())
		service.setDescription(ex, _description);
	return true;
}


bool ServerApplication::unregisterService(Exception& ex) {
	string name;
	getString("application.baseName", name);
	
	WinService service(name);
	return service.unregisterService(ex);
}

void ServerApplication::defineOptions(Exception& ex,Options& options) {

	options.add(ex, "registerService", "r", "Register the application as a service.");

	options.add(ex, "unregisterService", "u", "Unregister the application as a service.");

	options.add(ex, "name", "n", "Specify a display name for the service (only with /registerService).")
		.argument("name")
		.handler([this](const string& value) { _displayName = value; });

	options.add(ex, "description", "d", "Specify a description for the service (only with /registerService).")
		.argument("text")
		.handler([this](const string& value) { _description = value; });

	options.add(ex, "startup", "s", "Specify the startup mode for the service (only with /registerService).")
		.argument("automatic|manual")
		.handler([this](const string& value) {_startup = String::ICompare(value, "auto", 4) == 0 ? "auto" : "manual"; });

	Application::defineOptions(ex, options);
}

#else

TerminateSignal::TerminateSignal() {
	sigemptyset(&_signalSet);
	sigaddset(&_signalSet, SIGINT);
	sigaddset(&_signalSet, SIGQUIT);
	sigaddset(&_signalSet, SIGTERM);
	sigprocmask(SIG_BLOCK, &_signalSet, NULL);
}

void TerminateSignal::wait() {
	int signal;
	sigwait(&_signalSet, &signal);
}

//
// Unix specific code
//
void ServerApplication::waitForTerminationRequest() {
	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);
	sigaddset(&sset, SIGQUIT);
	sigaddset(&sset, SIGTERM);
	sigprocmask(SIG_BLOCK, &sset, NULL);
	int sig;
	sigwait(&sset, &sig);
}


int ServerApplication::run(int argc, char** argv) {
	try {
		bool runAsDaemon = isDaemon(argc, argv);
		if (runAsDaemon)
			beDaemon();
		if(!init(argc, argv))
			return EXIT_OK;

		if (runAsDaemon) {
			int rc = chdir("/");
			if (rc != 0)
				return EXIT_OSERR;
		}

		TerminateSignal terminateSignal;
		return main(terminateSignal);
	} catch (exception& ex) {
		FATAL( ex.what());
		return EXIT_SOFTWARE;
	} catch (...) {
		FATAL("Unknown error");
		return EXIT_SOFTWARE;
	}
}


bool ServerApplication::isDaemon(int argc, char** argv) {
	string option1("--daemon");
	string option2("-d");
	string option3("/daemon");
	string option4("/d");
	for (int i = 1; i < argc; ++i) {
		if (String::ICompare(option1,argv[i])==0 || String::ICompare(option2,argv[i])==0 || String::ICompare(option3,argv[i])==0 || String::ICompare(option4,argv[i])==0)
			return true;
	}
	return false;
}


void ServerApplication::beDaemon() {
	pid_t pid;
	if ((pid = fork()) < 0)
		throw exception("Cannot fork daemon process");
	if (pid != 0)
		exit(0);
	
	setsid();
	umask(0);
	
	// attach stdin, stdout, stderr to /dev/null
	// instead of just closing them. This avoids
	// issues with third party/legacy code writing
	// stuff to stdout/stderr.
	FILE* fin  = freopen("/dev/null", "r+", stdin);
	if (!fin)
		throw exception("Cannot attach stdin to /dev/null");
	FILE* fout = freopen("/dev/null", "r+", stdout);
	if (!fout)
		throw exception("Cannot attach stdout to /dev/null");
	FILE* ferr = freopen("/dev/null", "r+", stderr);
	if (!ferr)
		throw exception("Cannot attach stderr to /dev/null");

	setBool("application.runAsDaemon", true);
	_isInteractive=false;
}


void ServerApplication::defineOptions(OptionSet& options) {
	options.add("daemon", "d", "Run application as a daemon.")
		.handler([this](const string& value) { setBool("application.runAsDaemon", true) });
	
	options.add("pidfile", "p", "Write the process ID of the application to given file.")
		.argument("path")
		.handler(handlePidFile);

	Application::defineOptions(options);
}


void ServerApplication::handlePidFile(const string& value) {
	ofstream ostr(value.c_str());
	if (!ostr.good())
		FATAL_THROW("Cannot write PID to file ",value);
	ostr << Process::id() << endl;
	FileSystem::RegisterForDeletion(value);
}

#endif


} // namespace Mona