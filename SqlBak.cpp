// SqlBak.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// The following come from "SQL Server 2005 Virtual Backup Device Interface (VDI) Specification" 
// http://www.microsoft.com/downloads/en/details.aspx?familyid=416f8a51-65a3-4e8e-a4c8-adfe15e850fc
#include "vdi/vdi.h"        // interface declaration
#include "vdi/vdierror.h"   // error constants
#include "vdi/vdiguid.h"    // define the interface identifiers.

#include <process.h>    // _beginthread, _endthread

// Globals
TCHAR* _serverInstanceName;
bool _optionQuiet = false;

// printf to stdout (if -q quiet option isn't specified)
void log(const TCHAR* format, ...)
{
	if (_optionQuiet)
		return;

	// Basically do a fprintf to stderr. The va_* stuff is just to handle variable args
	va_list arglist;
	va_start(arglist, format);
	vfwprintf(stderr, format, arglist);
}

// printf to stdout (regardless of -q quiet option)
void err(const TCHAR* format, ...)
{
	va_list arglist;
	va_start(arglist, format);
	vfwprintf(stderr, format, arglist);
}

// Get human-readable message given a HRESULT
_bstr_t errorMessage(DWORD messageId)
{
	LPTSTR szMessage;
	if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, messageId, 0, (LPTSTR)&szMessage, 0, NULL))
	{
		szMessage = (LPTSTR)LocalAlloc(LPTR, 255);
		_snwprintf(szMessage, 255, L"Unknown error 0x%x.\n", messageId);		
	}
	_bstr_t retval(szMessage, true);
	LocalFree(szMessage);
	return retval;
}

// Execute the given SQL against _serverInstanceName
unsigned __stdcall executeSql(void* pArguments)
{
	TCHAR* sql = (TCHAR*)pArguments;
	//log(L"\nexecuteSql '%s'\n", sql);

	// Using '_Connection_Deprecated' interface for maximum MDAC compatibility
	Connection15Ptr conn;
	HRESULT hr = conn.CreateInstance(__uuidof(Connection));
	if (FAILED(hr))
	{
		err(L"ADODB.Connection CreateInstance failed: %s", (TCHAR*)errorMessage(hr));
		return hr;
	}

	// "lpc:..." = shared memory connection
	_bstr_t serverName = "lpc:.";
	if (_serverInstanceName != NULL)
		serverName += "\\" + _bstr_t(_serverInstanceName);

	try
	{
		conn->ConnectionString = "Provider=SQLOLEDB; Data Source=" + serverName + "; Initial Catalog=master; Integrated Security=SSPI;";
		//log(L"%s\n", (TCHAR*)conn->ConnectionString);
		conn->ConnectionTimeout = 25;
		conn->Open("", "", "", adConnectUnspecified);
	}
	catch(_com_error e)
	{
		err(L"\nFailed to open connection to '" + serverName + L"': ");
		err(L"%s [%s]\n", (TCHAR*)e.Description(), (TCHAR*)e.Source());
		return e.Error();
	}

	try 
	{	
		log(L"%s\n", (TCHAR*)sql);
		variant_t recordsAffected; 
		conn->CommandTimeout = 0;
		log(L"  Exec before\n");

		_Recordset* recordset;
		hr = conn->raw_Execute(sql, &recordsAffected, adOptionUnspecified, &recordset);
		if (FAILED(hr))
		{
			log(L"Execute failed 0x%x\n", hr);
			return hr;
		}
		log(L"  Exec after\n");
		conn->Close();
		log(L"  Query complete\n");
	}
	catch(_com_error e)
	{
		log(L"  ERROR!\n");
		err(L"\nQuery failed: '%s'\n%s [%s]\n", sql, (TCHAR*)e.Description(), (TCHAR*)e.Source());
		conn->Close();
		return e.Error();
	}

	return 0;
}

// Transfer data from virtualDevice to backupfile or vice-versa
HRESULT performTransfer(IClientVirtualDevice* virtualDevice, FILE* backupfile)
{
	VDC_Command *   cmd;
	DWORD           completionCode;
	DWORD           bytesTransferred;
	HRESULT         hr;
	DWORD           totalBytes = 0;

	//DWORD increment = 0;
	while (SUCCEEDED (hr = virtualDevice->GetCommand(3 * 60 * 1000, &cmd)))
	{
		//log(L">command %d, size %d\n", cmd->commandCode, cmd->size);
		bytesTransferred = 0;

		switch (cmd->commandCode)
		{
		case VDC_Read:

			while(bytesTransferred < cmd->size)
				bytesTransferred += fread(cmd->buffer + bytesTransferred, 1, cmd->size - bytesTransferred, backupfile);

			totalBytes += bytesTransferred;
			log(L"%d bytes read                               \r", totalBytes);

			cmd->size = bytesTransferred;

			completionCode = ERROR_SUCCESS;
			break;

		case VDC_Write:
			//log(L"VDC_Write - size: %d\r\n", cmd->size);

			while(bytesTransferred < cmd->size)
				bytesTransferred += fwrite(cmd->buffer + bytesTransferred, 1, cmd->size - bytesTransferred, backupfile);

			totalBytes += bytesTransferred;

			log(L"%d bytes written                             \r", totalBytes);
			completionCode = ERROR_SUCCESS;
			break;

		case VDC_Flush:
			//log(L"\nVDC_Flush %d\n", cmd->size);
			fflush(backupfile);
			completionCode = ERROR_SUCCESS;
			break;

		case VDC_ClearError:
			//log(L"\nVDC_ClearError\n");
			//Debug::WriteLine("VDC_ClearError");
			completionCode = ERROR_SUCCESS;
			break;

		default:
			log(L"Unknown commandCode %d\n", cmd->commandCode);
			// If command is unknown...
			completionCode = ERROR_NOT_SUPPORTED;
		}

		hr = virtualDevice->CompleteCommand(cmd, completionCode, bytesTransferred, 0);
		if (FAILED(hr))
		{
			err(L"\nvirtualDevice->CompleteCommand failed: %s\n", (TCHAR*)errorMessage(hr));
			return hr;
		}
	}

	log(L"\n");

	if (hr != VD_E_CLOSE)
	{
		err(L"virtualDevice->GetCommand failed: ");
		if (hr == VD_E_TIMEOUT)
			err(L" timeout awaiting data.\n");
		else if (hr == VD_E_ABORT)
			err(L" transfer is being aborted due to an error.\n");
		else
			err(L"%s\n", (TCHAR*)errorMessage(hr));

		return hr;
	}

	return NOERROR;
}

// Entry point
int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 3)
	{
		err(L"\n\
Action and database required.\n\
\n\
Usage: sqlbak backup|restore <database> [options]\n\
Options:\n\
  -q Quiet, don't print messages to STDERR\n\
  -i instancename\n");
		return 1;
	}

	TCHAR* command = argv[1];
	TCHAR* databaseName = argv[2];

	// Parse options
	for (int i = 0; i < argc; i++)
	{
		
		TCHAR* arg = _wcslwr(_wcsdup(argv[i]));
		if (wcscmp(arg, L"-q") == 0)
			_optionQuiet = true;

		if (wcscmp(arg, L"-i") == 0)
			_serverInstanceName = argv[i+1];

		free(arg);
	}

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		// Switching from apartment to multi-threaded is OK
		if(hr != RPC_E_CHANGED_MODE)
		{
			err(L"CoInitializeEx failed: ", hr);
			return 1;
		}
	}

	// Create Device Set
	IClientVirtualDeviceSet2 * virtualDeviceSet;
	hr = CoCreateInstance( 
		CLSID_MSSQL_ClientVirtualDeviceSet,  
		NULL, 
		CLSCTX_INPROC_SERVER,
		IID_IClientVirtualDeviceSet2,
		(void**)&virtualDeviceSet);

	if (FAILED(hr))
	{
		err(L"Could not create VDI component. Check registration of SQLVDI.DLL. %s\n", (TCHAR*)errorMessage(hr));
		return 2;
	}

	// Generate virtualDeviceName
	TCHAR virtualDeviceName[39];
	GUID guid; CoCreateGuid(&guid);	
	StringFromGUID2(guid, virtualDeviceName, sizeof(virtualDeviceName));

	// Create Device
	VDConfig vdConfig = {0};
	vdConfig.deviceCount = 1;
	hr = virtualDeviceSet->CreateEx(_serverInstanceName, virtualDeviceName, &vdConfig);
	if (!SUCCEEDED (hr))
	{
		err(L"IClientVirtualDeviceSet2.CreateEx failed\r\n");

		switch(hr)
		{
		case VD_E_INSTANCE_NAME:
			err(L"Didn't recognize the SQL Server instance name '"+ _bstr_t(_serverInstanceName) + L"'.\r\n");
			break;
		case E_ACCESSDENIED:
			err(L"Access Denied: You must be logged in as a Windows administrator to create virtual devices.\r\n");
			break;
		default:
			err(L"%s\n", (TCHAR*)errorMessage(hr));
			break;
		}
		return 3;
	}

	TCHAR* sql;
	FILE* backupFile;
	if (_wcsicmp(command, L"backup") == 0)
	{
		sql = "BACKUP DATABASE [" + _bstr_t(databaseName) + "] TO VIRTUAL_DEVICE = '" + virtualDeviceName + "'";
		backupFile = stdout;
	}
	else if(_wcsicmp(command, L"restore") == 0)
	{
		hr = executeSql((void*)(TCHAR*)("CREATE DATABASE ["+ _bstr_t(databaseName) +"]"));
		if (FAILED(hr))
			return hr;

		sql = "RESTORE DATABASE [" + _bstr_t(databaseName) + "] FROM VIRTUAL_DEVICE = '" + virtualDeviceName + "' WITH REPLACE";
		backupFile = stdin;
	}
	else 
	{
		err(L"Unsupported command '%s': only BACKUP or RESTORE are supported.\n", command);
		return 1;
	}

	// Invoke backup on separate thread because virtualDeviceSet->GetConfiguration will block until "BACKUP DATABASE..."
	unsigned threadId;
	//HANDLE executeSqlThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&executeSql, sql, 0, &threadId);
	HANDLE executeSqlThread = (HANDLE)_beginthreadex(NULL, 0, &executeSql, (void*)(sql), 0, &threadId);

	
	// Ready...
	hr = virtualDeviceSet->GetConfiguration(30000, &vdConfig);
	if (FAILED(hr))
	{
		err(L"\n%s: virtualDeviceSet->GetConfiguration failed: ", command);
		if (hr == VD_E_TIMEOUT)
			err(L"Timed out waiting for backup to be initiated.\n");
		else
			err(L"%s\n", (TCHAR*)errorMessage(hr));
		return 3;
	}

	// Steady...
	IClientVirtualDevice *virtualDevice = NULL;
	hr = virtualDeviceSet->OpenDevice(virtualDeviceName, &virtualDevice);
	if (FAILED(hr))
	{
		err(L"virtualDeviceSet->OpenDevice failed: 0x%x - ");
		if (hr == VD_E_TIMEOUT)
			err(L" timeout.\n");
		else
			err(L" %s.\n", (TCHAR*)errorMessage(hr));
		return 4;
	}

	// Go
	_setmode(_fileno(backupFile), _O_BINARY);
	hr = performTransfer(virtualDevice, backupFile);

	// Transferred, now wait for executeSql thread to complete
	if (SUCCEEDED(hr))
	{
		log(L"\n%s: Waiting for backup thread to complete...\n", command);
		WaitForSingleObject(executeSqlThread, 10000);
	}
	else 
	{
		log(L"Transfer failed\n");
	}

	// Tidy up 
	CloseHandle(executeSqlThread);
	virtualDeviceSet->Close();
	virtualDevice->Release();
	virtualDeviceSet->Release();

	log(L"%s: Finished.\n", command);
}