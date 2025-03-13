//
// Created by Yiyang Huo on 11/26/23.
//

#include "sql_int.h"

void sqliteDequote(std::string &str) {
  if (!str.empty() && str.front() == '[') {
	str.erase(0, 1); // Erase the first character
  }
  if (!str.empty() && str.back() == ']') {
	str.erase(str.size() - 1); // Erase the last character
  }
}

void sqliteCompressSpaces(std::string &str) {
  int writeIndex = 0, readIndex = 0;
  bool inSpace = false;

  // Trim leading whitespace
  while (readIndex < str.size() && std::isspace(static_cast<unsigned char>(str[readIndex]))) {
	++readIndex;
  }

  // Process the string
  for (; readIndex < str.size(); ++readIndex) {
	if (std::isspace(static_cast<unsigned char>(str[readIndex]))) {
	  if (!inSpace) {
		str[writeIndex++] = ' ';
		inSpace = true;
	  }
	} else {
	  str[writeIndex++] = str[readIndex];
	  inSpace = false;
	}
  }

  // Trim trailing whitespace
  if (inSpace && writeIndex > 0) {
	writeIndex--;
  }

  str.resize(writeIndex);
}
