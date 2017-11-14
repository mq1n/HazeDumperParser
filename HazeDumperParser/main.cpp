#include <Windows.h>
#include <WinInet.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <map>

#pragma comment(lib, "wininet.lib")

#define WEB_AGENT "DumpParser"
#define TARGET_URL "https://raw.githubusercontent.com/frk1/hazedumper/master/csgo.hpp"

inline auto StringToPointer(const std::string & szInput)
{
	std::uintptr_t uiValue = 0;
	std::stringstream ss;
	ss << std::hex << uiValue;
	ss.str(szInput);
	ss >> std::hex >> uiValue;
	return uiValue;
}

auto WebStatusCheck(const std::string & szAddress, DWORD * pdwWebStat)
{
	DWORD uiStatCode = 0;
	DWORD statCodeLen = sizeof(DWORD);

	auto hInternet = InternetOpenA(WEB_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, NULL);
	if (!hInternet) {
		printf("InternetOpenA fail. Error: %u", GetLastError());
		return false;
	}

	auto hRequestHandle = InternetOpenUrlA(hInternet, szAddress.c_str(), NULL, NULL, INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, NULL);
	if (!hRequestHandle) {
		printf("InternetOpenUrlA fail. Error: %u", GetLastError());
		InternetCloseHandle(hInternet);
		return false;
	}

	auto hQuery = HttpQueryInfoA(hRequestHandle, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &uiStatCode, &statCodeLen, NULL);
	if (!hQuery) {
		printf("HttpQueryInfoA fail. Error: %u", GetLastError());
		InternetCloseHandle(hInternet);
		InternetCloseHandle(hRequestHandle);
		return false;
	}

	InternetCloseHandle(hInternet);
	InternetCloseHandle(hRequestHandle);

	if (pdwWebStat)
		*pdwWebStat = uiStatCode;
	return true;
}

auto ReadUrl(const std::string & szAddress, std::string * pszResult, std::size_t * puiSize)
{
	char szTempBuffer[4096 + 1] = { 0 };
	std::vector <uint8_t> vTempdata;
	DWORD dwBytesRead = 1;

	auto hInternet = InternetOpenA(WEB_AGENT, NULL, NULL, NULL, NULL);
	if (!hInternet) {
		printf("InternetOpenA fail. Error: %u", GetLastError());
		return false;
	}

	auto hFile = InternetOpenUrlA(hInternet, szAddress.c_str(), NULL, NULL, INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, NULL);
	if (!hFile) {
		printf("InternetOpenUrlA fail. Error: %u", GetLastError());
		InternetCloseHandle(hInternet);
		return false;
	}

	std::size_t uiOldSize = 0;
	while (dwBytesRead) {
		if (InternetReadFile(hFile, szTempBuffer, 4096, &dwBytesRead))
			std::copy(&szTempBuffer[0], &szTempBuffer[dwBytesRead], std::back_inserter(vTempdata));
	}

	if (vTempdata.empty()) {
		printf("Any data can not read. Last Error: %u", GetLastError());
		InternetCloseHandle(hInternet);
		InternetCloseHandle(hFile);
		return false;
	}

	InternetCloseHandle(hInternet);
	InternetCloseHandle(hFile);

	if (puiSize) 
		*puiSize = vTempdata.size();

	std::string szData(vTempdata.begin(), vTempdata.end());
	if (pszResult)
		*pszResult = szData;

	if (!vTempdata.empty())
		vTempdata.clear();

	return true;
}

auto ParseResult(const std::string & szContent, const std::size_t uiSize)
{
	//printf("Size: %u - Data: %s\n", uiSize, szContent.c_str());
	std::map <std::string, std::uintptr_t> mapData;

	std::istringstream iss(szContent);
	if (!iss)
		return mapData;

	std::string szCurrentLine;
	while (std::getline(iss, szCurrentLine))
	{
		std::string szUselessData = "constexpr ::std::ptrdiff_t ";
		auto szUselessDataPos = szCurrentLine.find(szUselessData);
		if (szUselessDataPos != std::string::npos)
		{
			auto szRealData = szCurrentLine.substr(szUselessDataPos + szUselessData.size(), szCurrentLine.size() - 1);
			if (!szRealData.empty())
			{
				std::string szEqSign = " = ";
				auto uiEqSignPos = szRealData.find(szEqSign);
				if (uiEqSignPos != std::string::npos)
				{
					auto szKey = szRealData.substr(0, uiEqSignPos);
					auto szValue = szRealData.substr(uiEqSignPos + szEqSign.size(), std::string::npos);
					szValue.pop_back(); // delete semicolon
					//printf("szKey: %s Value: %s\n", szKey.c_str(), szValue.c_str());

					mapData[szKey] = StringToPointer(szValue);
				}
			}
		}
	}

	return mapData;
}

auto main(int, char*[]) -> int
{
	auto bConnect = InternetCheckConnectionA(TARGET_URL, FLAG_ICC_FORCE_CONNECTION, 0);
	if (!bConnect) {
		printf("InternetCheckConnectionA fail! Error: %u\n", GetLastError());
		std::cin.get();
		return EXIT_FAILURE;
	}

	DWORD dwWebStatus = 0;
	if (!WebStatusCheck(TARGET_URL, &dwWebStatus) || dwWebStatus != 200) {
		printf("Unknown web status: %u\n", dwWebStatus);
		std::cin.get();
		return EXIT_FAILURE;
	}

	std::string szReadData = "";
	std::size_t uiDataSize = 0;
	if (!ReadUrl(TARGET_URL, &szReadData, &uiDataSize) || !uiDataSize || uiDataSize != szReadData.size()) {
		printf("ReadUrl fail! Last Error: %u Size: %u-%u\n", GetLastError(), uiDataSize, szReadData.size());
		std::cin.get();
		return EXIT_FAILURE;
	}

	auto resultMap = ParseResult(szReadData, uiDataSize);
	if (resultMap.begin() == resultMap.end()) {
		printf("ParseResult fail!\n");
		std::cin.get();
		return EXIT_FAILURE;
	}

	for (auto elem : resultMap)
	{
		printf("Key: %s Value: 0x%X\n", elem.first.c_str(), elem.second);
	}

	printf("Completed!\n");
	std::cin.get();
	return EXIT_SUCCESS;
}

