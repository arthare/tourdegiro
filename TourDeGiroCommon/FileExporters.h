#pragma once

void WriteCSVFile(LPCTSTR lpszPath, bool fOverwrite, bool fTrainerMode, const std::vector<RECORDEDDATA> lstData);
void WritePWXFile(LPCTSTR lpszPath, bool fOverwrite, bool fTrainerMode, IConstPlayerDataPtrConst pPlayer, const std::vector<RECORDEDDATA> lstData);
std::wstring WriteTCXFile(LPCTSTR lpszPath, bool fOverwrite, bool fTrainerMode, IConstPlayerDataPtrConst pPlayer, const std::vector<RECORDEDDATA> lstData);