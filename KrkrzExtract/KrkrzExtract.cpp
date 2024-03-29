#include "stdafx.h"
#include <Windows.h>
#include <my.h>
#pragma comment(lib, "NativeLib.lib")



typedef
BOOL
(WINAPI
	*FuncCreateProcessInternalW)(
		HANDLE                  hToken,
		LPCWSTR                 lpApplicationName,
		LPWSTR                  lpCommandLine,
		LPSECURITY_ATTRIBUTES   lpProcessAttributes,
		LPSECURITY_ATTRIBUTES   lpThreadAttributes,
		BOOL                    bInheritHandles,
		ULONG                   dwCreationFlags,
		LPVOID                  lpEnvironment,
		LPCWSTR                 lpCurrentDirectory,
		LPSTARTUPINFOW          lpStartupInfo,
		LPPROCESS_INFORMATION   lpProcessInformation,
		PHANDLE                 phNewToken
		);

BOOL
(WINAPI
	*StubCreateProcessInternalW)(
		HANDLE                  hToken,
		LPCWSTR                 lpApplicationName,
		LPWSTR                  lpCommandLine,
		LPSECURITY_ATTRIBUTES   lpProcessAttributes,
		LPSECURITY_ATTRIBUTES   lpThreadAttributes,
		BOOL                    bInheritHandles,
		ULONG                   dwCreationFlags,
		LPVOID                  lpEnvironment,
		LPCWSTR                 lpCurrentDirectory,
		LPSTARTUPINFOW          lpStartupInfo,
		LPPROCESS_INFORMATION   lpProcessInformation,
		PHANDLE                 phNewToken
		);

BOOL
WINAPI
VMeCreateProcess(
	HANDLE                  hToken,
	LPCWSTR                 lpApplicationName,
	LPWSTR                  lpCommandLine,
	LPCWSTR                 lpDllPath,
	LPSECURITY_ATTRIBUTES   lpProcessAttributes,
	LPSECURITY_ATTRIBUTES   lpThreadAttributes,
	BOOL                    bInheritHandles,
	ULONG                   dwCreationFlags,
	LPVOID                  lpEnvironment,
	LPCWSTR                 lpCurrentDirectory,
	LPSTARTUPINFOW          lpStartupInfo,
	LPPROCESS_INFORMATION   lpProcessInformation,
	PHANDLE                 phNewToken
)
{
	NTSTATUS         Status;
	BOOL             Result, IsSuspended;
	UNICODE_STRING   FullDllPath;

	RtlInitUnicodeString(&FullDllPath, lpDllPath);

	IsSuspended = !!(dwCreationFlags & CREATE_SUSPENDED);
	dwCreationFlags |= CREATE_SUSPENDED;
	Result = StubCreateProcessInternalW(
		hToken,
		lpApplicationName,
		lpCommandLine,
		lpProcessAttributes,
		lpThreadAttributes,
		bInheritHandles,
		dwCreationFlags,
		lpEnvironment,
		lpCurrentDirectory,
		lpStartupInfo,
		lpProcessInformation,
		phNewToken);

	if (!Result)
		return Result;

	Status = InjectDllToRemoteProcess(
		lpProcessInformation->hProcess,
		lpProcessInformation->hThread,
		&FullDllPath,
		IsSuspended
	);

	if (NT_FAILED(Status))
	{
		NtTerminateProcess(lpProcessInformation->hProcess, 0);
		return FALSE;
	}

	NtResumeThread(lpProcessInformation->hThread, NULL);

	return TRUE;
}


BOOL FASTCALL CreateProcessInternalWithDll(LPCWSTR ProcessName)
{
	STARTUPINFOW        si;
	PROCESS_INFORMATION pi;

	RtlZeroMemory(&si, sizeof(si));
	RtlZeroMemory(&pi, sizeof(pi));
	si.cb = sizeof(si);

	return VMeCreateProcess(NULL, ProcessName, NULL, L"KrkrzExtract.dll", NULL, NULL, FALSE, NULL, NULL, NULL, &si, &pi, NULL);
}


int NTAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	LONG_PTR   Argc;
	PWSTR*     Argv;
	DWORD      Attribute;
	BOOL       Success;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	ml::MlInitialize();
	Argv = CmdLineToArgvW(lpCmdLine, &Argc);

	StubCreateProcessInternalW = (FuncCreateProcessInternalW)EATLookupRoutineByHashPNoFix(GetKernel32Handle(), KERNEL32_CreateProcessInternalW);

	LOOP_ONCE
	{
		if (Argc < 1)
			break;
		
		Attribute = Nt_GetFileAttributes(Argv[0]);
		if (Attribute == -1 || (Attribute & FILE_ATTRIBUTE_DIRECTORY))
		{
			MessageBoxW(NULL, L"No such file", L"FATAL", MB_OK | MB_ICONERROR);
			break;
		}

		Success = CreateProcessInternalWithDll(Argv[0]);
		if (!Success)
			MessageBoxW(NULL, L"Failed to launch process...", L"FATAL", MB_OK | MB_ICONERROR);
	};

	if (Argv)
		ReleaseArgv(Argv);
	
	return 0;
}

