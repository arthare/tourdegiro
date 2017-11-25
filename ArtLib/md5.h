#pragma once

void md5(const unsigned char *initial_msg, size_t initial_len, unsigned int* pOutBuf);
void md5tophpstring(const unsigned char *initial_msg, size_t initial_len, std::string* pstrOut);
