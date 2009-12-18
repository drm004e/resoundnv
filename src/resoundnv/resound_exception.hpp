#pragma once

#include <exception>
class Exception : public std::exception {
	const char* msg_;
public:
	Exception(const char* msg) : msg_(msg) {}
	const char* what() const throw() { return msg_; }
};

