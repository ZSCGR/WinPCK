//////////////////////////////////////////////////////////////////////
// PckClassFileDisk.cpp: 用于解析完美世界公司的pck文件中的数据，并显示在List中
// 主要计算写入文件的初始大小，磁盘空间等
//
// 此程序由 李秋枫/stsm/liqf 编写
//
// 此代码预计将会开源，任何基于此代码的修改发布请保留原作者信息
// 
// 2018.5.31
//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "MapViewFile.h"
#include "PckClassFileDisk.h"


//创建时，剩余文件的空间量不够时，添加量
#define	PCK_STEP_ADD_SIZE				(64*1024*1024)
//创建时，剩余文件的空间量小于此值时，扩展数据
#define	PCK_SPACE_DETECT_SIZE			(4*1024*1024)
//创建时，剩余文件的空间量小于此值(PCK_SPACE_DETECT_SIZE)时，扩展数据的值
//#define PCK_RENAME_EXPAND_ADD			(16*1024*1024)

#define ZLIB_AVG_RATIO					0.6

namespace NPckClassFileDisk
{

	template <typename T>
	_inline T * __fastcall mystrcpy(T * dest, const T *src)
	{
		while((*dest = *src))
			++dest, ++src;
		return dest;
	}


	template <typename T>
	void GetDriverNameFromFilename(const T* lpszFilename, T lpszDiskName[])
	{
		lpszDiskName[0] = lpszFilename[0];
		lpszDiskName[1] = lpszFilename[1];
		lpszDiskName[2] = lpszFilename[2];
		lpszDiskName[3] = '\0';
	}

	//qwCurrentPckFilesize为已经存在的文件大小，qwToAddSpace是需要扩大的大小，返回值为（qwCurrentPckFilesize + 可以再扩大的最大大小）
	QWORD GetPckFilesizeByCompressed(QWORD qwDiskFreeSpace, QWORD qwToAddSpace, QWORD qwCurrentPckFilesize)
	{
		//计算大概需要多大空间qwTotalFileSize
		QWORD	qwTotalFileSizeTemp = qwToAddSpace;

		if(-1 != qwDiskFreeSpace) {

			//如果申请的空间小于磁盘剩余空间，则申请文件空间大小等于剩余磁盘空间
			if(qwDiskFreeSpace < qwTotalFileSizeTemp)
				qwTotalFileSizeTemp = qwDiskFreeSpace;
		}

		return (qwTotalFileSizeTemp + qwCurrentPckFilesize);

	}

	BOOL IsNeedExpandWritingFile(
		CMapViewFileWrite *lpWrite,
		QWORD dwWritingAddressPointer,
		QWORD dwFileSizeToWrite,
		QWORD &dwExpectedTotalCompressedFileSize)
	{
		//判断一下dwAddress的值会不会超过dwTotalFileSizeAfterCompress
		//如果超过，说明文件空间申请的过小，重新申请一下ReCreateFileMapping
		//新文件大小在原来的基础上增加(lpfirstFile->dwFileSize + 1mb) >= 64mb ? (lpfirstFile->dwFileSize + 1mb) :64mb
		//1mb=0x100000
		//64mb=0x4000000
		if((dwWritingAddressPointer + dwFileSizeToWrite + PCK_SPACE_DETECT_SIZE) > dwExpectedTotalCompressedFileSize) {
			lpWrite->UnMaping();

			QWORD qwSizeToExpand = ((dwFileSizeToWrite + PCK_SPACE_DETECT_SIZE) > PCK_STEP_ADD_SIZE ? (dwFileSizeToWrite + PCK_SPACE_DETECT_SIZE) : PCK_STEP_ADD_SIZE);
			ULARGE_INTEGER lpfree;

			if(GetDiskFreeSpaceExA(lpWrite->GetFileDiskName(), NULL, NULL, &lpfree)) {

				qwSizeToExpand = GetPckFilesizeByCompressed(lpfree.QuadPart, qwSizeToExpand, 0);
			} else {
				qwSizeToExpand = GetPckFilesizeByCompressed(-1, qwSizeToExpand, 0);
			}

			if(dwFileSizeToWrite > qwSizeToExpand)
				return FALSE;

			QWORD qwNewExpectedTotalCompressedFileSize = dwExpectedTotalCompressedFileSize + qwSizeToExpand;

			if(!lpWrite->Mapping(qwNewExpectedTotalCompressedFileSize)) {

				lpWrite->Mapping(dwExpectedTotalCompressedFileSize);
				return FALSE;
			}
			dwExpectedTotalCompressedFileSize = qwNewExpectedTotalCompressedFileSize;
		}
		return TRUE;


	}

	//重命名时需要的文件的大小
	QWORD GetPckFilesizeRename(LPCTSTR lpszFilename, QWORD qwCurrentPckFilesize)
	{
		//查看磁盘空间
		TCHAR szDiskName[4];
		ULARGE_INTEGER lpfree;
		GetDriverNameFromFilename(lpszFilename, szDiskName);


		if(GetDiskFreeSpaceEx(szDiskName, NULL, NULL, &lpfree)) {

			return GetPckFilesizeByCompressed(lpfree.QuadPart, PCK_SPACE_DETECT_SIZE, qwCurrentPckFilesize);
		} else {
			return GetPckFilesizeByCompressed(-1, PCK_SPACE_DETECT_SIZE, qwCurrentPckFilesize);
		}

	}

	QWORD GetPckFilesizeRebuild(LPCTSTR lpszFilename, QWORD qwPckFilesize)
	{
		//查看磁盘空间
		TCHAR szDiskName[4];
		ULARGE_INTEGER lpfree;
		GetDriverNameFromFilename(lpszFilename, szDiskName);

		QWORD qwToAddSpace = qwPckFilesize + PCK_SPACE_DETECT_SIZE;

		if(GetDiskFreeSpaceEx(szDiskName, NULL, NULL, &lpfree)) {

			return GetPckFilesizeByCompressed(lpfree.QuadPart, qwToAddSpace, 0);
		} else {
			return GetPckFilesizeByCompressed(-1, qwToAddSpace, 0);
		}
	}

	QWORD GetPckFilesizeByCompressed(LPCSTR lpszFilename, QWORD qwToCompressFilesize, QWORD qwCurrentPckFilesize)
	{
		//查看磁盘空间
		char szDiskName[4];
		ULARGE_INTEGER lpfree;
		GetDriverNameFromFilename<char>(lpszFilename, szDiskName);

		QWORD qwToAddSpace = qwToCompressFilesize * ZLIB_AVG_RATIO + PCK_SPACE_DETECT_SIZE;

		if(GetDiskFreeSpaceExA(szDiskName, NULL, NULL, &lpfree)) {

			return GetPckFilesizeByCompressed(lpfree.QuadPart, qwToAddSpace, qwCurrentPckFilesize);
		} else {
			return GetPckFilesizeByCompressed(-1, qwToAddSpace, qwCurrentPckFilesize);
		}
	}

	QWORD GetPckFilesizeByCompressed(LPCWSTR lpszFilename, QWORD qwToCompressFilesize, QWORD qwCurrentPckFilesize)
	{
		//查看磁盘空间
		wchar_t szDiskName[4];
		ULARGE_INTEGER lpfree;
		GetDriverNameFromFilename<wchar_t>(lpszFilename, szDiskName);

		QWORD qwToAddSpace = qwToCompressFilesize * ZLIB_AVG_RATIO + PCK_SPACE_DETECT_SIZE;

		if(GetDiskFreeSpaceExW(szDiskName, NULL, NULL, &lpfree)) {

			return GetPckFilesizeByCompressed(lpfree.QuadPart, qwToAddSpace, qwCurrentPckFilesize);
		} else {
			return GetPckFilesizeByCompressed(-1, qwToAddSpace, qwCurrentPckFilesize);
		}

	}

	//////////////////////////以下是程序过程中需要调用的过程///////////////////////////////
	VOID EnumFile(LPSTR szFilename, BOOL IsPatition, DWORD &dwFileCount, LinkList<FILES_TO_COMPRESS> *lpFileLinkList, QWORD &qwTotalFileSize, size_t nLen)
	{

		char		szPath[MAX_PATH], szFile[MAX_PATH];

		size_t nLenBytePath = mystrcpy(szPath, szFilename) - szPath + 1;
		strcat(szFilename, "\\*.*");

		HANDLE					hFile;
		WIN32_FIND_DATAA		WFD;

		if((hFile = FindFirstFileA(szFilename, &WFD)) != INVALID_HANDLE_VALUE) {
			if(!IsPatition) {
				FindNextFileA(hFile, &WFD);
				if(!FindNextFileA(hFile, &WFD)) {
					return;
				}
			}

			do {
				if((WFD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {

					if((MAX_PATH_PCK_260 - 13) >= mystrcpy(mystrcpy(mystrcpy(szFile, szPath), "\\"), WFD.cFileName) - szFile) {
						EnumFile(szFile, FALSE, dwFileCount, lpFileLinkList, qwTotalFileSize, nLen);
					}

				} else {

					//if(NULL == pFileinfo)return;
					if(0 != WFD.nFileSizeHigh)continue;

					++dwFileCount;

					LPFILES_TO_COMPRESS pFileinfo = lpFileLinkList->last();

					if(MAX_PATH_PCK_260 < nLenBytePath + strlen(WFD.cFileName)) {
						mystrcpy(mystrcpy(mystrcpy(pFileinfo->szFilename, szPath), "\\"), WFD.cAlternateFileName);
					} else {
						mystrcpy(mystrcpy(mystrcpy(pFileinfo->szFilename, szPath), "\\"), WFD.cFileName);
					}

#if 0
					logOutput(__FUNCTION__, formatString("pFileinfo add:(%d)\t%s\r\n", (int)pFileinfo, pFileinfo->szFilename));
#endif

					pFileinfo->lpszFileTitle = pFileinfo->szFilename + nLen;
					pFileinfo->nBytesToCopy = MAX_PATH - nLen;

					qwTotalFileSize += (pFileinfo->dwFileSize = WFD.nFileSizeLow);

					lpFileLinkList->insertNext();
					//pFileinfo->next = (LPFILES_TO_COMPRESS)AllocMemory(sizeof(FILES_TO_COMPRESS));
					//pFileinfo = pFileinfo->next;

				}

			} while(FindNextFileA(hFile, &WFD));

			FindClose(hFile);
		}

	}

}